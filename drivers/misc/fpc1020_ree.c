/*
 * ZUK FPC1150 REE driver
 * Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) "ZUK-FPC: %s: " fmt, __func__
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <linux/pinctrl/consumer.h>
#include <linux/io.h>
#include <linux/of_gpio.h>
#include <linux/input.h>

#define FPC1020_TOUCH_DEV_NAME  "fpc1020tp"

#define FPC1020_RESET_LOW_US 1000
#define FPC1020_RESET_HIGH1_US 100
#define FPC1020_RESET_HIGH2_US 1250
#define FPC_TTW_HOLD_TIME 1000

struct fpc1020_data {
	struct device   *dev;
	struct pinctrl  *pin;
	wait_queue_head_t wq_irq_return;
	/*Set pins*/
	int reset_gpio;
	int irq_gpio;
	int irq;
	struct notifier_block fb_notif;
	/*Input device*/
	struct input_dev *input_dev;
	struct work_struct input_report_work;
	struct workqueue_struct *fpc1020_wq;
	u8  report_key;
	struct wake_lock wake_lock;
	struct wake_lock fp_wl;
	int wakeup_status;
	int screen_on;
};

/*
 * From drivers/input/keyboard/gpio_keys.c
 */
extern bool home_button_pressed(void);
/*
 * From drivers/input/keyboard/gpio_keys.c
 */
extern void reset_home_button(void);

bool reset;

static int fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data);

static ssize_t irq_get(struct device* device,
		struct device_attribute* attribute,
		char* buffer)
{
	struct fpc1020_data* fpc1020 = dev_get_drvdata(device);
	int irq = gpio_get_value(fpc1020->irq_gpio);
	return scnprintf(buffer, PAGE_SIZE, "%i\n", irq);
}

static ssize_t irq_set(struct device* device,
		struct device_attribute* attribute,
		const char* buffer, size_t count)
{
	int retval = 0;
	u64 val;
	struct fpc1020_data* fpc1020 = dev_get_drvdata(device);
	retval = kstrtou64(buffer, 0, &val);
	if (val == 1)
		enable_irq(fpc1020->irq);
	else if (val == 0)
		disable_irq(fpc1020->irq);
	else
		return -ENOENT;
	return strnlen(buffer, count);
}

static DEVICE_ATTR(irq, S_IRUSR | S_IWUSR, irq_get, irq_set);

static ssize_t fp_wl_get(struct device* device,
		struct device_attribute* attribute,
		char * buffer)
{
	//struct fpc1020_data* fpc1020 = dev_get_drvdata(device);
	return 0;
}

static ssize_t fp_wl_set(struct device* device,
		struct device_attribute* attribute,
		const char* buffer, size_t count)
{
	int retval = 0;
	u64 val;
	struct fpc1020_data* fpc1020 = dev_get_drvdata(device);
	retval = kstrtou64(buffer, 0, &val);
	if (val == 1 && !wake_lock_active(&fpc1020->fp_wl))
		wake_lock(&fpc1020->fp_wl);
	else if (val == 0 && wake_lock_active(&fpc1020->fp_wl))
		wake_unlock(&fpc1020->fp_wl);
	else {
		pr_err("HAL wakelock request fail, val = %d\n", (int)val);
	}
	return strnlen(buffer, count);
}

static DEVICE_ATTR(wl, S_IRUSR | S_IWUSR, fp_wl_get, fp_wl_set);

static ssize_t get_wakeup_status(struct device* device,
		struct device_attribute* attribute,
		char* buffer)
{
	struct fpc1020_data* fpc1020 = dev_get_drvdata(device);
	return scnprintf(buffer, PAGE_SIZE, "%i\n", fpc1020->wakeup_status);
}

static ssize_t set_wakeup_status(struct device* device,
		struct device_attribute* attribute,
		const char* buffer, size_t count)
{
	int retval = 0;
	u64 val;
	struct fpc1020_data* fpc1020 = dev_get_drvdata(device);

	retval = kstrtou64(buffer, 0, &val);
	pr_info("val === %d\n", (int)val);
	if (val == 1) {
		enable_irq_wake(fpc1020->irq);
		fpc1020->wakeup_status = 1;
	} else if (val == 0) {
		disable_irq_wake(fpc1020->irq);
		fpc1020->wakeup_status = 0;
	}
	else
		return -ENOENT;

	return strnlen(buffer, count);
}

static DEVICE_ATTR(wakeup, S_IRUSR | S_IWUSR, get_wakeup_status, set_wakeup_status);

static ssize_t get_key(struct device* device, struct device_attribute* attribute, char* buffer)
{
	struct fpc1020_data* fpc1020 = dev_get_drvdata(device);
	return scnprintf(buffer, PAGE_SIZE, "%i\n", fpc1020->report_key);
}

static ssize_t set_key(struct device* device,
		struct device_attribute* attribute,
		const char*buffer, size_t count)
{
	int retval = 0;
	u64 val;
	struct fpc1020_data* fpc1020 = dev_get_drvdata(device);
	bool home_pressed;

	retval = kstrtou64(buffer, 0, &val);
	if (!retval) {
		if (val == KEY_HOME)
			val = KEY_NAVI_LONG;  //Convert to U-touch long press keyValue

	    home_pressed = home_button_pressed();

		if (val && home_pressed) val = 0;

		pr_info("home key pressed = %d\n", (int)home_pressed);
		fpc1020->report_key = (int)val;
		queue_work(fpc1020->fpc1020_wq, &fpc1020->input_report_work);

		if (!val) {
			pr_info("calling home key reset");
			reset_home_button();
		}
	} else
		return -ENOENT;
	return strnlen(buffer, count);
}

static DEVICE_ATTR(key, S_IRUSR | S_IWUSR, get_key, set_key);

static ssize_t get_screen_stat(struct device* device, struct device_attribute* attribute, char* buffer)
{
	struct fpc1020_data* fpc1020 = dev_get_drvdata(device);
	return scnprintf(buffer, PAGE_SIZE, "%i\n", fpc1020->screen_on);
}

static ssize_t set_screen_stat(struct device* device,
		struct device_attribute* attribute,
		const char*buffer, size_t count)
{
	return 1;
}

static DEVICE_ATTR(screen, S_IRUSR | S_IWUSR, get_screen_stat, set_screen_stat);

static struct attribute *attributes[] = {
	&dev_attr_irq.attr,
	&dev_attr_wakeup.attr,
	&dev_attr_key.attr,
	&dev_attr_wl.attr,
	&dev_attr_screen.attr,
	NULL
};

static const struct attribute_group attribute_group = {
	.attrs = attributes,
};

static void fpc1020_report_work_func(struct work_struct *work)
{
	struct fpc1020_data *fpc1020 = NULL;
	fpc1020 = container_of(work, struct fpc1020_data, input_report_work);
	if (fpc1020->screen_on == 1) {
		pr_info("Report key value = %d\n", (int)fpc1020->report_key);
		input_report_key(fpc1020->input_dev, fpc1020->report_key, 1);
		input_sync(fpc1020->input_dev);
		mdelay(30);
		input_report_key(fpc1020->input_dev, fpc1020->report_key, 0);
		input_sync(fpc1020->input_dev);
		fpc1020->report_key = 0;
	}
}

static void fpc1020_hw_reset(struct fpc1020_data *fpc1020)
{
	pr_info("HW reset\n");
	gpio_set_value(fpc1020->reset_gpio, 1);
	udelay(FPC1020_RESET_HIGH1_US);

	gpio_set_value(fpc1020->reset_gpio, 0);
	udelay(FPC1020_RESET_LOW_US);

	gpio_set_value(fpc1020->reset_gpio, 1);
	udelay(FPC1020_RESET_HIGH2_US);
}

static int fpc1020_get_pins(struct fpc1020_data *fpc1020)
{
	int retval = 0;
	struct device_node *np = fpc1020->dev->of_node;

	fpc1020->irq_gpio = of_get_named_gpio(np, "fpc,gpio_irq", 0);
	if (!gpio_is_valid(fpc1020->irq_gpio)) {
		pr_err("IRQ request failed.\n");
		goto err;
	}
	fpc1020->reset_gpio = of_get_named_gpio(np, "fpc,gpio_reset", 0);
	if (!gpio_is_valid(fpc1020->reset_gpio)) {
		pr_err("RESET pin request failed\n");
		goto err;
	}

	fpc1020->pin = pinctrl_get_select_default(fpc1020->dev);
	if (IS_ERR_OR_NULL(fpc1020->pin)) {
		pr_err("pinctrl get failed.\n");
		goto err;
	}

	return 0;
err:
	pr_err("%s, err\n", __func__);
	fpc1020->irq = -EINVAL;
	fpc1020->irq_gpio = fpc1020->reset_gpio = -EINVAL;
	retval = -ENODEV;
	return retval;
}

static irqreturn_t fpc1020_irq_handler(int irq, void *_fpc1020)
{
	struct fpc1020_data *fpc1020 = _fpc1020;
	pr_info("fpc1020 IRQ interrupt\n");
	smp_rmb();
	wake_lock_timeout(&fpc1020->wake_lock, 3*HZ);
	sysfs_notify(&fpc1020->dev->kobj, NULL, dev_attr_irq.attr.name);
	return IRQ_HANDLED;
}

static int fpc1020_initial_irq(struct fpc1020_data *fpc1020)
{
	int retval = 0;

	if (!gpio_is_valid(fpc1020->irq_gpio)) {
		pr_err("IRQ pin(%d) is not valid\n", fpc1020->irq_gpio);
		return -EINVAL;
	}

	retval = gpio_request(fpc1020->irq_gpio, "fpc_irq");
	if (retval) {
		pr_err("IRQ(%d) request failed\n", fpc1020->irq_gpio);
		return -EINVAL;
	}

	retval = gpio_direction_input(fpc1020->irq_gpio);
	if (retval) {
		pr_err("Set input(%d) failed\n", fpc1020->irq_gpio);
		return -EINVAL;
	}

	fpc1020->irq = gpio_to_irq(fpc1020->irq_gpio);
	if (fpc1020->irq < 0) {
		pr_err("gpio_to_irq(%d) failed\n", fpc1020->irq_gpio);
		return -EINVAL;
	}

	retval = devm_request_threaded_irq(fpc1020->dev, fpc1020->irq, NULL, fpc1020_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT, dev_name(fpc1020->dev), fpc1020);
	if (retval) {
		pr_err("request irq %i failed.\n", fpc1020->irq);
		fpc1020->irq = -EINVAL;
		return -EINVAL;
	}

	return 0;
}

static int fpc1020_manage_sysfs(struct fpc1020_data *fpc1020)
{
	int retval = 0;
	retval = sysfs_create_group(&fpc1020->dev->kobj, &attribute_group);
	if (retval) {
		pr_err("Could not create sysfs\n");
		return -EINVAL;
	}
	return 0;
}

static int fpc1020_alloc_input_dev(struct fpc1020_data *fpc1020)
{
	int retval = 0;
	fpc1020->input_dev = input_allocate_device();
	if (!fpc1020->input_dev) {
		pr_info("Input allocate device failed\n");
		retval = -ENOMEM;
		return retval;
	}

	fpc1020->input_dev->name = "fpc1020tp";
	set_bit(EV_KEY, fpc1020->input_dev->evbit);
	set_bit(KEY_BACK, fpc1020->input_dev->keybit);
	set_bit(KEY_LEFT, fpc1020->input_dev->keybit);
	set_bit(KEY_RIGHT, fpc1020->input_dev->keybit);
	//set_bit(KEY_NAVI_LONG, fpc1020->input_dev->keybit);
	input_set_capability(fpc1020->input_dev, EV_KEY, KEY_NAVI_LEFT);
	input_set_capability(fpc1020->input_dev, EV_KEY, KEY_NAVI_RIGHT);
	input_set_capability(fpc1020->input_dev, EV_KEY, KEY_BACK);
	//input_set_capability(fpc1020->input_dev, EV_KEY, KEY_NAVI_LONG);

	/* Register the input device */
	retval = input_register_device(fpc1020->input_dev);
	if (retval) {
		pr_err("Input_register_device failed.\n");
		input_free_device(fpc1020->input_dev);
		fpc1020->input_dev = NULL;
	}
	return retval;
}

static int fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	int *blank;
	struct fb_event *evdata = data;
	struct fpc1020_data *fpc1020 = container_of(self, struct fpc1020_data, fb_notif);
	blank = evdata->data;
	if (evdata && evdata->data && event == FB_EVENT_BLANK && fpc1020) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK) {
			pr_err("ScreenOn\n");
			fpc1020->screen_on = 1;
		} else if (*blank == FB_BLANK_POWERDOWN) {
			pr_err("ScreenOff\n");
			fpc1020->screen_on = 0;
		}
	}
	return 0;
}

static int fpc1020_probe(struct platform_device *pdev)
{
	int retval = 0;
	struct device *dev = &pdev->dev;

	struct fpc1020_data *fpc1020 = devm_kzalloc(dev, sizeof(struct fpc1020_data), GFP_KERNEL);
	if (fpc1020 == NULL) {
		pr_err("fpc1020 allocation error\n");
		retval = -ENOMEM;
		goto error;
	}

	fpc1020->dev = &pdev->dev;
	dev_set_drvdata(dev, fpc1020);

	retval = fpc1020_get_pins(fpc1020);
	if (retval != 0) {
		pr_err("Get pins failed\n");
		goto error;
	}

	/*create sfs nodes*/
	retval = fpc1020_manage_sysfs(fpc1020);
	if (retval != 0) {
		pr_err("Create sysfs nodes failed\n");
		goto error;
	}

	/*create input device for navigation*/
	retval = fpc1020_alloc_input_dev(fpc1020);
	if (retval != 0) {
		pr_err("Allocate input device failed\n");
		goto error_remove_sysfs;
	}

	fpc1020->fpc1020_wq = create_workqueue("fpc1020_wq");
	if (!fpc1020->fpc1020_wq) {
		pr_err("Create input workqueue failed\n");
		goto error_unregister_device;
	}
	INIT_WORK(&fpc1020->input_report_work, fpc1020_report_work_func);

	gpio_direction_output(fpc1020->reset_gpio, 1);
	/*Do HW reset*/
	fpc1020_hw_reset(fpc1020);

	fpc1020->fb_notif.notifier_call = fb_notifier_callback;
	retval = fb_register_client(&fpc1020->fb_notif);
	if (retval) {
		pr_err("Unable to register fb_notifier : %d\n", retval);
		goto error_destroy_workqueue;
	}

	wake_lock_init(&fpc1020->wake_lock, WAKE_LOCK_SUSPEND, "fpc_wakelock");
	wake_lock_init(&fpc1020->fp_wl, WAKE_LOCK_SUSPEND, "fp_hal_wl");

	retval = fpc1020_initial_irq(fpc1020);
	if (retval != 0) {
		pr_err("IRQ initialized failure\n");
		goto error_unregister_client;
	}

	//Disable IRQ
	disable_irq(fpc1020->irq);

	return 0;

error_unregister_client:
	fb_unregister_client(&fpc1020->fb_notif);

error_destroy_workqueue:
	destroy_workqueue(fpc1020->fpc1020_wq);

error_unregister_device:
	input_unregister_device(fpc1020->input_dev);

error_remove_sysfs:
	sysfs_remove_group(&fpc1020->dev->kobj, &attribute_group);

error:
	if (fpc1020 != NULL)
		kzfree(fpc1020);

	return retval;
}

static int fpc1020_resume(struct platform_device *pdev)
{
	int retval = 0;
	return retval;
}

static int fpc1020_suspend(struct platform_device *pdev, pm_message_t state)
{
	int retval = 0;
	return retval;
}

static int fpc1020_remove(struct platform_device *pdev)
{
	int retval = 0;
	return retval;
}

static struct of_device_id fpc1020_match[] = {
	{
		.compatible = "fpc,fpc1020",
	},
	{}
};

static struct platform_driver fpc1020_plat_driver = {
	.probe = fpc1020_probe,
	.remove = fpc1020_remove,
	.suspend = fpc1020_suspend,
	.resume = fpc1020_resume,
	.driver = {
		.name = "fpc1020",
		.owner = THIS_MODULE,
		.of_match_table = fpc1020_match,
	},
};

static int fpc1020_init(void)
{
	return platform_driver_register(&fpc1020_plat_driver);
}

static void fpc1020_exit(void)
{
	platform_driver_unregister(&fpc1020_plat_driver);
}

module_init(fpc1020_init);
module_exit(fpc1020_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ZUK ShenQi <lvxin1@zuk.com>");
MODULE_DESCRIPTION("FPC1020 fingerprint sensor ree driver.");
