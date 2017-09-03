/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012 Synaptics Incorporated
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include "tfa9890.h"
#ifdef KERNEL_ABOVE_2_6_38
#include <linux/input/mt.h>
#endif

#define DRIVER_NAME "tfa9890"
#define MAX_BUFFER_SIZE 512	
#define GPIO_SLEEP_LOW_US 10
#define RESET_DELAY 500

struct tfa9890_dev	{
	wait_queue_head_t	read_wq;
	struct mutex		read_mutex;
	struct device		*dev;
	struct i2c_client	*i2c_client;
	struct miscdevice	tfa9890_device;
	unsigned int 		ven_gpio;
	unsigned int 		firm_gpio;
	unsigned int		irq_gpio;
	unsigned int		irq_flags;
	unsigned int		irq;
	unsigned int		reset_gpio;
	unsigned int		reset_flags;
	bool i2c_pull_up;
	struct regulator *vdd;
	struct regulator *vcc_i2c;
	bool			do_reading;
	bool			irq_enabled;
	bool cancel_read;
    bool first_open;
};

static ssize_t tfa9890_dev_read(struct file *filp, char __user *buf,
		size_t count, loff_t *offset)
{
	struct tfa9890_dev *tfa9890_dev = filp->private_data;
	char tmp[MAX_BUFFER_SIZE];
	int ret;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;


	mutex_lock(&tfa9890_dev->read_mutex);

	/* Read data */
	ret = i2c_master_recv(tfa9890_dev->i2c_client, tmp, count);

	if (ret < 0) {
		pr_err("%s: i2c_master_recv returned %d\n", __func__, ret);
		mutex_unlock(&tfa9890_dev->read_mutex);
		return ret;
	}
	if (ret > count) {
		pr_err("%s: received too many bytes from i2c (%d)\n",
			__func__, ret);
		mutex_unlock(&tfa9890_dev->read_mutex);
		return -EIO;
	}

	if (copy_to_user(buf, tmp, ret)) {
		pr_warning("%s : failed to copy to user space\n", __func__);
		mutex_unlock(&tfa9890_dev->read_mutex);
		return -EFAULT;
	}
	mutex_unlock(&tfa9890_dev->read_mutex);
	return ret;

}

static ssize_t tfa9890_dev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *offset)
{
	struct tfa9890_dev  *tfa9890_dev = filp->private_data;
	char tmp[MAX_BUFFER_SIZE];
	int ret;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	if (copy_from_user(tmp, buf, count)) {
		pr_err("%s : failed to copy from user space\n", __func__);
		return -EFAULT;
	}

	/* Write data */
	ret = i2c_master_send(tfa9890_dev->i2c_client, tmp, count);
	if (ret != count) {
		pr_err("%s : i2c_master_send returned %d\n", __func__, ret);
		ret = -EIO;
	}
	return ret;
}

static int tfa9890_dev_open(struct inode *inode, struct file *filp)
{
    int ret;

	struct tfa9890_dev *tfa9890_dev = container_of(filp->private_data,
						struct tfa9890_dev,
						tfa9890_device);

	filp->private_data = tfa9890_dev;
	pr_err("%s : %d,%d\n", __func__, imajor(inode), iminor(inode));

    if (tfa9890_dev->first_open) {
        if (gpio_is_valid(tfa9890_dev->reset_gpio)) {
            /* configure tfa9890s reset out gpio */
            ret = gpio_request(tfa9890_dev->reset_gpio,
                    "tfa9890_reset_gpio");
            if (ret) {
                dev_err(&tfa9890_dev->i2c_client->dev, "unable to request gpio [%d]\n",
                        tfa9890_dev->reset_gpio);
                goto err_reset_gpio_dir;
            }

            ret = gpio_direction_output(tfa9890_dev->reset_gpio, 1);
            if (ret) {
                dev_err(&tfa9890_dev->i2c_client->dev,
                        "unable to set direction for gpio [%d]\n",
                        tfa9890_dev->reset_gpio);
                goto err_reset_gpio_dir;
            }

            gpio_set_value(tfa9890_dev->reset_gpio, 1);
            mdelay(GPIO_SLEEP_LOW_US);
            gpio_set_value(tfa9890_dev->reset_gpio, 0);
            mdelay(RESET_DELAY);
        }
        tfa9890_dev->first_open = false;
        printk("%s enter, reset PA at first open\n", __func__);
    }

	return 0;

err_reset_gpio_dir:
	if (gpio_is_valid(tfa9890_dev->reset_gpio))
		gpio_free(tfa9890_dev->reset_gpio);

	return ret;
}

static long tfa9890_dev_ioctl(struct file *filp,
			    unsigned int cmd, unsigned long arg)
{
	struct tfa9890_dev *tfa9890_dev = filp->private_data;
	switch(cmd)
	{
		case I2C_SLAVE:
		{
			tfa9890_dev->i2c_client->addr = arg;
			break;
		}
		case ENABLE_MI2S_CLK:
		{
			udelay(500);
			msm_q6_enable_mi2s_clocks(arg);
			//printk("[%s][%d]\n",__func__,__LINE__);
			break;
		}
		default:
			break;
	}

	return 0;
}

static const struct file_operations tfa9890_dev_fops = {
	.owner	= THIS_MODULE,
	.llseek	= no_llseek,
	.read	= tfa9890_dev_read,
	.write	= tfa9890_dev_write,
	.open	= tfa9890_dev_open,
	.unlocked_ioctl	= tfa9890_dev_ioctl,
};

static int tfa9890_parse_dt(struct device *dev,
			 struct tfa9890_i2c_platform_data *pdata)
{
	int ret = 0;
	struct device_node *np = dev->of_node;
	
	/* reset, irq gpio info */
	pdata->reset_gpio = of_get_named_gpio_flags(np,
			"reset-gpio", 0, &pdata->reset_flags);
	pr_info("%s,pdata->reset_gpio = %d\n", __func__, pdata->reset_gpio);
	pdata->irq_gpio = of_get_named_gpio_flags(np,
			"irq-gpio", 0, &pdata->irq_flags);
	pr_info("%s,pdata->irq_gpio = %d\n", __func__, pdata->irq_gpio);

	pdata->i2c_pull_up = of_property_read_bool(np,
			"tfa9890,i2c-pull-up");
	pr_info("%s,pdata->i2c_pull_up = %d\n", __func__, pdata->i2c_pull_up);
	return ret;
}

#ifdef TFA_POWER_SET
static int reg_set_optimum_mode_check(struct regulator *reg, int load_uA)
{
	return (regulator_count_voltages(reg) > 0) ?
		regulator_set_optimum_mode(reg, load_uA) : 0;
}

static int tfa9890_power_on(struct tfa9890_dev *tfa9890_dev,
					bool on) {
	int retval;
	return 0;
	if (on == false)
		goto power_off;

	pr_info("[%s][%d]\n", __func__, __LINE__);

	retval = reg_set_optimum_mode_check(tfa9890_dev->vdd,
		TFA9890_ACTIVE_LOAD_UA);
	if (retval < 0) {
		dev_err(&tfa9890_dev->i2c_client->dev,
			"Regulator vdd set_opt failed rc=%d\n",
			retval);
		return retval;
	}

	retval = regulator_enable(tfa9890_dev->vdd);
	if (retval) {
		dev_err(&tfa9890_dev->i2c_client->dev,
			"Regulator vdd enable failed rc=%d\n",
			retval);
		goto error_reg_en_vdd;
	}

	if (tfa9890_dev->i2c_pull_up) {
		pr_info("[%s][%d]\n", __func__, __LINE__);
		retval = reg_set_optimum_mode_check(tfa9890_dev->vcc_i2c,
			TFA9890_I2C_LOAD_UA);
		if (retval < 0) {
			dev_err(&tfa9890_dev->i2c_client->dev,
				"Regulator vcc_i2c set_opt failed rc=%d\n",
				retval);
			goto error_reg_opt_i2c;
		}

		retval = regulator_enable(tfa9890_dev->vcc_i2c);
		if (retval) {
			dev_err(&tfa9890_dev->i2c_client->dev,
				"Regulator vcc_i2c enable failed rc=%d\n",
				retval);
			goto error_reg_en_vcc_i2c;
		}
	}
	return 0;

error_reg_en_vcc_i2c:
	if (tfa9890_dev->i2c_pull_up)
		reg_set_optimum_mode_check(tfa9890_dev->vdd, 0);
error_reg_opt_i2c:
	regulator_disable(tfa9890_dev->vdd);
error_reg_en_vdd:
	reg_set_optimum_mode_check(tfa9890_dev->vdd, 0);
	return retval;

power_off:
	reg_set_optimum_mode_check(tfa9890_dev->vdd, 0);
	regulator_disable(tfa9890_dev->vdd);
	if (tfa9890_dev->i2c_pull_up) {
		reg_set_optimum_mode_check(tfa9890_dev->vcc_i2c, 0);
		regulator_disable(tfa9890_dev->vcc_i2c);
	}
	return 0;
}
static int tfa9890_regulator_configure(struct tfa9890_dev *tfa9890_dev, bool on)
{
	int retval;
	return 0;
	if (on == false)
		goto hw_shutdown;

	tfa9890_dev->vdd = regulator_get(&tfa9890_dev->i2c_client->dev,
					"vdd");
	if (IS_ERR(tfa9890_dev->vdd)) {
		dev_err(&tfa9890_dev->i2c_client->dev,
				"%s: Failed to get vdd regulator\n",
				__func__);
		return PTR_ERR(tfa9890_dev->vdd);
	}

	if (regulator_count_voltages(tfa9890_dev->vdd) > 0) {
		retval = regulator_set_voltage(tfa9890_dev->vdd,
			TFA9890_VTG_MIN_UV, TFA9890_VTG_MAX_UV);
		if (retval) {
			dev_err(&tfa9890_dev->i2c_client->dev,
				"regulator set_vtg failed retval =%d\n",
				retval);
			goto err_set_vtg_vdd;
		}
	}

	if (tfa9890_dev->i2c_pull_up) {
		pr_info("[%s][%d]\n", __func__, __LINE__);
		tfa9890_dev->vcc_i2c = regulator_get(&tfa9890_dev->i2c_client->dev,
						"vcc_i2c");
		if (IS_ERR(tfa9890_dev->vcc_i2c)) {
			dev_err(&tfa9890_dev->i2c_client->dev,
					"%s: Failed to get i2c regulator\n",
					__func__);
			retval = PTR_ERR(tfa9890_dev->vcc_i2c);
			goto err_get_vtg_i2c;
		}

		if (regulator_count_voltages(tfa9890_dev->vcc_i2c) > 0) {
			retval = regulator_set_voltage(tfa9890_dev->vcc_i2c,
				TFA9890_I2C_VTG_MIN_UV, TFA9890_I2C_VTG_MAX_UV);
			if (retval) {
				dev_err(&tfa9890_dev->i2c_client->dev,
					"reg set i2c vtg failed retval =%d\n",
					retval);
			goto err_set_vtg_i2c;
			}
		}
	}
	return 0;

err_set_vtg_i2c:
	if (tfa9890_dev->i2c_pull_up)
		regulator_put(tfa9890_dev->vcc_i2c);
err_get_vtg_i2c:
	if (regulator_count_voltages(tfa9890_dev->vdd) > 0)
		regulator_set_voltage(tfa9890_dev->vdd, 0,
			TFA9890_VTG_MAX_UV);
err_set_vtg_vdd:
	regulator_put(tfa9890_dev->vdd);
	return retval;

hw_shutdown:
	if (regulator_count_voltages(tfa9890_dev->vdd) > 0)
		regulator_set_voltage(tfa9890_dev->vdd, 0,
			TFA9890_VTG_MAX_UV);
	regulator_put(tfa9890_dev->vdd);
	if (tfa9890_dev->i2c_pull_up) {
		if (regulator_count_voltages(tfa9890_dev->vcc_i2c) > 0)
			regulator_set_voltage(tfa9890_dev->vcc_i2c, 0,
					TFA9890_I2C_VTG_MAX_UV);
		regulator_put(tfa9890_dev->vcc_i2c);
	}
	return 0;
};
#endif

 /**
 * nxp_tfa9890_probe()
 *
 * Called by the kernel when an association with an I2C device of the
 * same name is made (after doing i2c_add_driver).
 *
 * This funtion allocates and initializes the resources for the driver
 * as an input driver, turns on the power to the sensor, queries the
 * sensor for its supported Functions and characteristics, registers
 * the driver to the input subsystem, sets up the interrupt, handles
 * the registration of the early_suspend and late_resume functions,
 * and creates a work queue for detection of other expansion Function
 * modules.
 */
static int nxp_tfa9890_probe(struct i2c_client *client,
		const struct i2c_device_id *dev_id)
{
	int ret = 0;
	struct tfa9890_i2c_platform_data *platform_data;
	struct tfa9890_dev *tfa9890_dev;

	printk("%s\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: i2c check failed\n", __func__);
		ret = -ENODEV;
		goto err_i2c;
	}

	if (client->dev.of_node) {
		platform_data = devm_kzalloc(&client->dev,
			sizeof(*platform_data),
			GFP_KERNEL);
		if (!platform_data) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		ret = tfa9890_parse_dt(&client->dev, platform_data);
		if (ret)
			return ret;
	} else {
		platform_data = client->dev.platform_data;
	}
	

	tfa9890_dev = kzalloc(sizeof(*tfa9890_dev), GFP_KERNEL);
	if (tfa9890_dev == NULL) {
		pr_err("failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}
	
	tfa9890_dev->i2c_client   = client;
	tfa9890_dev->dev = &client->dev;
	tfa9890_dev->do_reading = 0;

	/* Initialise mutex and work queue */
	init_waitqueue_head(&tfa9890_dev->read_wq);
	mutex_init(&tfa9890_dev->read_mutex);

	tfa9890_dev->irq_enabled = false;
	tfa9890_dev->tfa9890_device.minor = MISC_DYNAMIC_MINOR;
	tfa9890_dev->tfa9890_device.name = "tfa9890";
	tfa9890_dev->tfa9890_device.fops = &tfa9890_dev_fops;
#ifdef TFA_POWER_SET
	tfa9890_dev->i2c_pull_up = platform_data->i2c_pull_up;
	ret = tfa9890_regulator_configure(tfa9890_dev, true);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to configure regulators\n");
		goto err_exit;
	}

	ret = tfa9890_power_on(tfa9890_dev, true);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to power on\n");
		goto err_misc_register;
	}
#endif

	if (gpio_is_valid(platform_data->reset_gpio)) {
		/* configure tfa9890s reset out gpio */
		ret = gpio_request(platform_data->reset_gpio,
				"tfa9890_reset_gpio");
		if (ret) {
			dev_err(&client->dev, "unable to request gpio [%d]\n",
						platform_data->reset_gpio);
			goto err_reset_gpio_dir;
		}

		ret = gpio_direction_output(platform_data->reset_gpio, 1);
		if (ret) {
			dev_err(&client->dev,
				"unable to set direction for gpio [%d]\n",
				platform_data->reset_gpio);
			goto err_reset_gpio_dir;
		}

		gpio_set_value(platform_data->reset_gpio, 1);
		mdelay(GPIO_SLEEP_LOW_US);
		gpio_set_value(platform_data->reset_gpio, 0);
		mdelay(RESET_DELAY);
	}

	ret = misc_register(&tfa9890_dev->tfa9890_device);
	if (ret) {
		pr_err("%s : misc_register failed\n", __FILE__);
		goto err_misc_register;
	}

	printk("%s Done\n", __func__);
	
	return 0;

err_misc_register:
	misc_deregister(&tfa9890_dev->tfa9890_device);
	mutex_destroy(&tfa9890_dev->read_mutex);
	kfree(tfa9890_dev);

#if 1
err_reset_gpio_dir:
	if (gpio_is_valid(platform_data->reset_gpio))
		gpio_free(platform_data->reset_gpio);
#endif

err_exit:
err_i2c:
	kfree(platform_data);

	return ret;
}

 /**
 * nxp_tfa9890_remove()
 *
 * Called by the kernel when the association with an I2C device of the
 * same name is broken (when the driver is unloaded).
 *
 * This funtion terminates the work queue, stops sensor data acquisition,
 * frees the interrupt, unregisters the driver from the input subsystem,
 * turns off the power to the sensor, and frees other allocated resources.
 */
static int nxp_tfa9890_remove(struct i2c_client *client)
{
	struct tfa9890_dev *tfa9890_dev;

	pr_info("%s\n", __func__);
	tfa9890_dev = i2c_get_clientdata(client);
	misc_deregister(&tfa9890_dev->tfa9890_device);
	mutex_destroy(&tfa9890_dev->read_mutex);

	kfree(tfa9890_dev);

	return 0;
}

#ifdef CONFIG_PM
 /**
 * nxp_tfa9890_suspend()
 *
 * Called by the kernel during the suspend phase when the system
 * enters suspend.
 *
 * This function stops finger data acquisition and puts the sensor to
 * sleep (if not already done so during the early suspend phase),
 * disables the interrupt, and turns off the power to the sensor.
 */
static int nxp_tfa9890_suspend(struct device *dev)
{
	pr_info(KERN_ALERT "----------------suspend");

	return 0;
}

 /**
 * nxp_tfa9890_resume()
 *
 * Called by the kernel during the resume phase when the system
 * wakes up from suspend.
 *
 * This function turns on the power to the sensor, wakes the sensor
 * from sleep, enables the interrupt, and starts finger data
 * acquisition.
 */
static int nxp_tfa9890_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops nxp_tfa9890_dev_pm_ops = {
	.suspend = nxp_tfa9890_suspend,
	.resume  = nxp_tfa9890_resume,
};
#endif

static const struct i2c_device_id nxp_tfa9890_id_table[] = {
	{DRIVER_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, nxp_tfa9890_id_table);

#ifdef CONFIG_OF
static struct of_device_id tfa9890_match_table[] = {
	{ .compatible = "nxp,tfa9890",},
	{ },
};
#else
#define tfa9890_match_table NULL
#endif

static struct i2c_driver nxp_tfa9890_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tfa9890_match_table,
#ifdef CONFIG_PM
		.pm = &nxp_tfa9890_dev_pm_ops,
#endif
	},
	.probe = nxp_tfa9890_probe,
	.remove =nxp_tfa9890_remove,
	.id_table = nxp_tfa9890_id_table,
};

 /**
 * nxp_tfa9890_init()
 *
 * Called by the kernel during do_initcalls (if built-in)
 * or when the driver is loaded (if a module).
 *
 * This function registers the driver to the I2C subsystem.
 *
 */
static int __init nxp_tfa9890_init(void)
{
	printk("%s\n",__func__);
	return i2c_add_driver(&nxp_tfa9890_driver);
}

 /**
 * nxp_tfa9890_exit()
 *
 * Called by the kernel when the driver is unloaded.
 *
 * This funtion unregisters the driver from the I2C subsystem.
 *
 */
static void __exit nxp_tfa9890_exit(void)
{
	i2c_del_driver(&nxp_tfa9890_driver);

	return;
}

module_init(nxp_tfa9890_init);
module_exit(nxp_tfa9890_exit);

MODULE_AUTHOR("NXP, Inc.");
MODULE_DESCRIPTION("NXP TFA9885 I2C Touch Driver");
MODULE_LICENSE("GPL v2");
