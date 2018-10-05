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
#include <linux/pinctrl/consumer.h>
#include <linux/io.h>
#include <linux/of_gpio.h>
#include <linux/input.h>

#define FPC1020_TOUCH_DEV_NAME  "fpc1020tp"

#define FPC1020_RESET_LOW_US 1000
#define FPC1020_RESET_HIGH1_US 100
#define FPC1020_RESET_HIGH2_US 1250
#define FPC_TTW_HOLD_TIME 1500

struct fpc1020_data {
	struct device   *dev;
	struct pinctrl  *pin;
	wait_queue_head_t wq_irq_return;
	/*Set pins*/
	int reset_gpio;
	int irq_gpio;
	int irq;
	bool irq_enabled;
	int wakeup_enabled;
	struct notifier_block fb_notif;
	/*Input device*/
	struct input_dev *input_dev;
	struct work_struct pm_work;
	struct work_struct input_report_work;
	struct workqueue_struct *fpc1020_wq;
	u8  report_key;
	int screen_on;
	int proximity_state; /* 0:far 1:near */
};

static void config_irq(struct fpc1020_data *fpc1020, bool enabled)
{
	if (enabled != fpc1020->irq_enabled) {
		if (enabled)
			enable_irq(gpio_to_irq(fpc1020->irq_gpio));
		else
			disable_irq(gpio_to_irq(fpc1020->irq_gpio));

		dev_info(fpc1020->dev, "%s: %s fpc irq ---\n", __func__,
			enabled ?  "enable" : "disable");
		fpc1020->irq_enabled = enabled;
	} else {
		dev_info(fpc1020->dev, "%s: dual config irq status: %s\n", __func__,
			enabled ?  "true" : "false");
	}
}

/* From drivers/input/keyboard/gpio_keys.c */
extern bool home_button_pressed(void);
extern void reset_home_button(void);

bool reset;

static int fb_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data);

static ssize_t irq_get(struct device *device,
		struct device_attribute *attribute,
		char *buffer)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(device);
	int irq = gpio_get_value(fpc1020->irq_gpio);

	return scnprintf(buffer, PAGE_SIZE, "%i\n", irq);
}

static ssize_t irq_set(struct device *device,
		struct device_attribute *attribute,
		const char *buffer, size_t count)
{
	int retval = 0;
	u64 val;
	struct fpc1020_data *fpc1020 = dev_get_drvdata(device);

	retval = kstrtou64(buffer, 0, &val);
	if (val)
		enable_irq(fpc1020->irq);
	else if (!val)
		disable_irq(fpc1020->irq);
	else
		return -ENOENT;
	return strnlen(buffer, count);
}

static DEVICE_ATTR(irq, S_IRUSR | S_IWUSR, irq_get, irq_set);

static ssize_t get_key(struct device *device,
		struct device_attribute *attribute, char *buffer)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(device);

	return scnprintf(buffer, PAGE_SIZE, "%i\n", fpc1020->report_key);
}

static ssize_t set_key(struct device *device,
		struct device_attribute *attribute,
		const char *buffer, size_t count)
{
	int retval = 0;
	u64 val;
	struct fpc1020_data *fpc1020 = dev_get_drvdata(device);
	bool home_pressed;

	retval = kstrtou64(buffer, 0, &val);
	if (!retval) {
		if (val == KEY_HOME)
			/* Convert to U-touch long press keyValue */
			val = KEY_NAVI_LONG;

		home_pressed = home_button_pressed();

		if (val && home_pressed)
			val = 0;

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

static ssize_t proximity_state_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct fpc1020_data *fpc1020 = dev_get_drvdata(dev);
	int rc, val;
	rc = kstrtoint(buf, 10, &val);
	if (rc)
		return -EINVAL;
	fpc1020->proximity_state = !!val;
	if (!fpc1020->screen_on) {
		if (fpc1020->proximity_state) {
			/* Disable IRQ when screen is off and proximity sensor is covered */
			config_irq(fpc1020, false);
		} else if (fpc1020->wakeup_enabled) {
			/* Enable IRQ when screen is off and proximity sensor is uncovered,
			 but only if fingerprint wake up is enabled */
			 config_irq(fpc1020, true);
		}
	}
	return count;
}
static DEVICE_ATTR(proximity_state, S_IWUSR, NULL, proximity_state_set);

static struct attribute *attributes[] = {
	&dev_attr_irq.attr,
	&dev_attr_key.attr,
	&dev_attr_proximity_state.attr,
	NULL
};

static const struct attribute_group attribute_group = {
	.attrs = attributes,
};

static void fpc1020_report_work_func(struct work_struct *work)
{
	struct fpc1020_data *fpc1020 = NULL;

	fpc1020 = container_of(work, struct fpc1020_data, input_report_work);
	if (fpc1020->screen_on) {
		pr_info("Report key value = %d\n", (int)fpc1020->report_key);
		input_report_key(fpc1020->input_dev, fpc1020->report_key, 1);
		input_sync(fpc1020->input_dev);
		msleep(30);
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
	/* Make sure 'wakeup_enabled' is updated before using it
	 ** since this is interrupt context (other thread...) */
	smp_rmb();
	if (fpc1020->wakeup_enabled && !fpc1020->screen_on) {
		pm_wakeup_event(fpc1020->dev, 5000);
	}
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

	device_init_wakeup(fpc1020->dev, 1);
	fpc1020->wakeup_enabled = 1;

	retval = devm_request_threaded_irq(fpc1020->dev,
			fpc1020->irq, NULL, fpc1020_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT | IRQF_NO_SUSPEND,
			dev_name(fpc1020->dev), fpc1020);

	if (retval) {
		pr_err("request irq %i failed.\n", fpc1020->irq);
		fpc1020->irq = -EINVAL;
		return -EINVAL;
	}

	dev_info(fpc1020->dev, "requested irq %d\n", fpc1020->irq);
	/* Request that the interrupt should be wakeable*/
	if (fpc1020->wakeup_enabled) {
		enable_irq_wake(fpc1020->irq);
	}
	fpc1020->irq_enabled = true;

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
	set_bit(KEY_NAVI_LONG, fpc1020->input_dev->keybit);
	input_set_capability(fpc1020->input_dev, EV_KEY, KEY_NAVI_LEFT);
	input_set_capability(fpc1020->input_dev, EV_KEY, KEY_NAVI_RIGHT);
	input_set_capability(fpc1020->input_dev, EV_KEY, KEY_BACK);
	input_set_capability(fpc1020->input_dev, EV_KEY, KEY_NAVI_LONG);

	/* Register the input device */
	retval = input_register_device(fpc1020->input_dev);
	if (retval) {
		pr_err("Input_register_device failed.\n");
		input_free_device(fpc1020->input_dev);
		fpc1020->input_dev = NULL;
	}
	return retval;
}

static void set_fingerprintd_nice(int nice)
{
	struct task_struct *p;

	read_lock(&tasklist_lock);
	for_each_process(p) {
		if (!memcmp(p->comm, "fingerprint@2.1", 16)) {
			pr_debug("fingerprint nice changed to %i\n", nice);
			set_user_nice(p, nice);
			break;
		}
	}
	read_unlock(&tasklist_lock);
}

static void fpc1020_suspend_resume(struct work_struct *work)
{
	struct fpc1020_data *fpc1020 =
		container_of(work, typeof(*fpc1020), pm_work);

	/* Escalate fingerprintd priority when screen is off */
	if (!fpc1020->screen_on)
		set_fingerprintd_nice(MIN_NICE);
	else
		set_fingerprintd_nice(0);
}

static int fb_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data)
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
			queue_work(fpc1020->fpc1020_wq, &fpc1020->pm_work);
			/* Unconditionally enable IRQ when screen turns on */
			config_irq(fpc1020, true);
		} else if (*blank == FB_BLANK_POWERDOWN) {
			pr_err("ScreenOff\n");
			fpc1020->screen_on = 0;
			if (!fpc1020->wakeup_enabled)
				config_irq(fpc1020, false);
			queue_work(fpc1020->fpc1020_wq, &fpc1020->pm_work);
		}
	}
	return 0;
}

static int fpc1020_probe(struct platform_device *pdev)
{
	int retval = 0;
	struct device *dev = &pdev->dev;

	struct fpc1020_data *fpc1020 = devm_kzalloc(dev,
			sizeof(struct fpc1020_data), GFP_KERNEL);
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

	fpc1020->wakeup_enabled = 0;

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

	fpc1020->fpc1020_wq = alloc_workqueue("fpc1020_wq", WQ_HIGHPRI, 1);
	if (!fpc1020->fpc1020_wq) {
		pr_err("Create input workqueue failed\n");
		goto error_unregister_device;
	}
	INIT_WORK(&fpc1020->input_report_work, fpc1020_report_work_func);
	INIT_WORK(&fpc1020->pm_work, fpc1020_suspend_resume);
	gpio_direction_output(fpc1020->reset_gpio, 1);
	/*Do HW reset*/
	fpc1020_hw_reset(fpc1020);

	fpc1020->fb_notif.notifier_call = fb_notifier_callback;
	retval = fb_register_client(&fpc1020->fb_notif);
	if (retval) {
		pr_err("Unable to register fb_notifier : %d\n", retval);
		goto error_destroy_workqueue;
	}

	device_init_wakeup(dev, true);

	retval = fpc1020_initial_irq(fpc1020);
	if (retval != 0) {
		pr_err("IRQ initialized failure\n");
		goto error_unregister_client;
	}

	/* Disable IRQ */
	disable_irq(fpc1020->irq);
	/* Enable irq wake */
	enable_irq_wake(fpc1020->irq);
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
