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
#ifndef _NXP_TFA9890_I2C_H_
#define _NXP_TFA9890_I2C_H_

#include <linux/version.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 38))
#define KERNEL_ABOVE_2_6_38
#endif

#ifdef KERNEL_ABOVE_2_6_38
#define sstrtoul(...) kstrtoul(__VA_ARGS__)
#else
#define sstrtoul(...) strict_strtoul(__VA_ARGS__)
#endif

#define TFA9890_VTG_MIN_UV		1800000
#define TFA9890_VTG_MAX_UV		1800000
#define TFA9890_ACTIVE_LOAD_UA	10000 //15000
#define TFA9890_LPM_LOAD_UA	10

#define TFA9890_I2C_VTG_MIN_UV	1800000
#define TFA9890_I2C_VTG_MAX_UV	1800000
#define TFA9890_I2C_LOAD_UA	10000
#define TFA9890_I2C_LPM_LOAD_UA	10

extern int msm_q6_enable_mi2s_clocks(bool enable);

struct tfa9890_i2c_platform_data {
	struct i2c_client *i2c_client;
	bool i2c_pull_up;
	bool regulator_en;
	u32 reset_flags;
	u32 irq_flags;
	unsigned int irq;
	unsigned int reset_gpio_test;
	unsigned int reset_gpio;
	unsigned int irq_gpio;
	int (*driver_opened)(void);
	void (*driver_closed)(void);
};

#endif
