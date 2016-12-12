/*
 * DRV260X haptics driver family
 *
 * Author: Dan Murphy <dmurphy@ti.com>
 *
 * Copyright:   (C) 2014 Texas Instruments, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>

#include <dt-bindings/input/ti-drv260x.h>
#include <linux/platform_data/drv260x-pdata.h>

#define DRV260X_STATUS		0x0
#define DRV260X_MODE		0x1
#define DRV260X_RT_PB_IN	0x2
#define DRV260X_LIB_SEL		0x3
#define DRV260X_WV_SEQ_1	0x4
#define DRV260X_WV_SEQ_2	0x5
#define DRV260X_WV_SEQ_3	0x6
#define DRV260X_WV_SEQ_4	0x7
#define DRV260X_WV_SEQ_5	0x8
#define DRV260X_WV_SEQ_6	0x9
#define DRV260X_WV_SEQ_7	0xa
#define DRV260X_WV_SEQ_8	0xb
#define DRV260X_GO				0xc
#define DRV260X_OVERDRIVE_OFF	0xd
#define DRV260X_SUSTAIN_P_OFF	0xe
#define DRV260X_SUSTAIN_N_OFF	0xf
#define DRV260X_BRAKE_OFF		0x10
#define DRV260X_A_TO_V_CTRL		0x11
#define DRV260X_A_TO_V_MIN_INPUT	0x12
#define DRV260X_A_TO_V_MAX_INPUT	0x13
#define DRV260X_A_TO_V_MIN_OUT	0x14
#define DRV260X_A_TO_V_MAX_OUT	0x15
#define DRV260X_RATED_VOLT		0x16
#define DRV260X_OD_CLAMP_VOLT	0x17
#define DRV260X_CAL_COMP		0x18
#define DRV260X_CAL_BACK_EMF	0x19
#define DRV260X_FEEDBACK_CTRL	0x1a
#define DRV260X_CTRL1			0x1b
#define DRV260X_CTRL2			0x1c
#define DRV260X_CTRL3			0x1d
#define DRV260X_CTRL4			0x1e
#define DRV260X_CTRL5			0x1f
#define DRV260X_LRA_LOOP_PERIOD	0x20
#define DRV260X_VBAT_MON		0x21
#define DRV260X_LRA_RES_PERIOD	0x22
#define DRV260X_MAX_REG			0x23

#define DRV260X_GO_BIT				0x01

/* Library Selection */
#define DRV260X_LIB_SEL_MASK		0x07
#define DRV260X_LIB_SEL_RAM			0x0
#define DRV260X_LIB_SEL_OD			0x1
#define DRV260X_LIB_SEL_40_60		0x2
#define DRV260X_LIB_SEL_60_80		0x3
#define DRV260X_LIB_SEL_100_140		0x4
#define DRV260X_LIB_SEL_140_PLUS	0x5

#define DRV260X_LIB_SEL_HIZ_MASK	0x10
#define DRV260X_LIB_SEL_HIZ_EN		0x01
#define DRV260X_LIB_SEL_HIZ_DIS		0

/* Mode register */
#define DRV260X_STANDBY				(1 << 6)
#define DRV260X_STANDBY_MASK		0x40
#define DRV260X_INTERNAL_TRIGGER	0x00
#define DRV260X_EXT_TRIGGER_EDGE	0x01
#define DRV260X_EXT_TRIGGER_LEVEL	0x02
#define DRV260X_PWM_ANALOG_IN		0x03
#define DRV260X_AUDIOHAPTIC			0x04
#define DRV260X_RT_PLAYBACK			0x05
#define DRV260X_DIAGNOSTICS			0x06
#define DRV260X_AUTO_CAL			0x07

/* Audio to Haptics Control */
#define DRV260X_AUDIO_HAPTICS_PEAK_10MS		(0 << 2)
#define DRV260X_AUDIO_HAPTICS_PEAK_20MS		(1 << 2)
#define DRV260X_AUDIO_HAPTICS_PEAK_30MS		(2 << 2)
#define DRV260X_AUDIO_HAPTICS_PEAK_40MS		(3 << 2)

#define DRV260X_AUDIO_HAPTICS_FILTER_100HZ	0x00
#define DRV260X_AUDIO_HAPTICS_FILTER_125HZ	0x01
#define DRV260X_AUDIO_HAPTICS_FILTER_150HZ	0x02
#define DRV260X_AUDIO_HAPTICS_FILTER_200HZ	0x03

/* Min/Max Input/Output Voltages */
#define DRV260X_AUDIO_HAPTICS_MIN_IN_VOLT	0x19
#define DRV260X_AUDIO_HAPTICS_MAX_IN_VOLT	0x64
#define DRV260X_AUDIO_HAPTICS_MIN_OUT_VOLT	0x19
#define DRV260X_AUDIO_HAPTICS_MAX_OUT_VOLT	0xFF

/* Feedback register */
#define DRV260X_FB_REG_ERM_MODE			0x7f
#define DRV260X_FB_REG_LRA_MODE			(1 << 7)

#define DRV260X_BRAKE_FACTOR_MASK	0x1f
#define DRV260X_BRAKE_FACTOR_2X		(1 << 0)
#define DRV260X_BRAKE_FACTOR_3X		(2 << 4)
#define DRV260X_BRAKE_FACTOR_4X		(3 << 4)
#define DRV260X_BRAKE_FACTOR_6X		(4 << 4)
#define DRV260X_BRAKE_FACTOR_8X		(5 << 4)
#define DRV260X_BRAKE_FACTOR_16		(6 << 4)
#define DRV260X_BRAKE_FACTOR_DIS	(7 << 4)

#define DRV260X_LOOP_GAIN_LOW		0xf3
#define DRV260X_LOOP_GAIN_MED		(1 << 2)
#define DRV260X_LOOP_GAIN_HIGH		(2 << 2)
#define DRV260X_LOOP_GAIN_VERY_HIGH	(3 << 2)

#define DRV260X_BEMF_GAIN_0			0xfc
#define DRV260X_BEMF_GAIN_1		(1 << 0)
#define DRV260X_BEMF_GAIN_2		(2 << 0)
#define DRV260X_BEMF_GAIN_3		(3 << 0)

/* Control 1 register */
#define DRV260X_AC_CPLE_EN			(1 << 5)
#define DRV260X_STARTUP_BOOST		(1 << 7)

/* Control 2 register */

#define DRV260X_IDISS_TIME_45		0
#define DRV260X_IDISS_TIME_75		(1 << 0)
#define DRV260X_IDISS_TIME_150		(1 << 1)
#define DRV260X_IDISS_TIME_225		0x03

#define DRV260X_BLANK_TIME_45	(0 << 2)
#define DRV260X_BLANK_TIME_75	(1 << 2)
#define DRV260X_BLANK_TIME_150	(2 << 2)
#define DRV260X_BLANK_TIME_225	(3 << 2)

#define DRV260X_SAMP_TIME_150	(0 << 4)
#define DRV260X_SAMP_TIME_200	(1 << 4)
#define DRV260X_SAMP_TIME_250	(2 << 4)
#define DRV260X_SAMP_TIME_300	(3 << 4)

#define DRV260X_BRAKE_STABILIZER	(1 << 6)
#define DRV260X_UNIDIR_IN			(0 << 7)
#define DRV260X_BIDIR_IN			(1 << 7)

/* Control 3 Register */
#define DRV260X_LRA_OPEN_LOOP		(1 << 0)
#define DRV260X_ANANLOG_IN			(1 << 1)
#define DRV260X_LRA_DRV_MODE		(1 << 2)
#define DRV260X_RTP_UNSIGNED_DATA	(1 << 3)
#define DRV260X_SUPPLY_COMP_DIS		(1 << 4)
#define DRV260X_ERM_OPEN_LOOP		(1 << 5)
#define DRV260X_NG_THRESH_0			(0 << 6)
#define DRV260X_NG_THRESH_2			(1 << 6)
#define DRV260X_NG_THRESH_4			(2 << 6)
#define DRV260X_NG_THRESH_8			(3 << 6)

/* Control 4 Register */
#define DRV260X_AUTOCAL_TIME_150MS		(0 << 4)
#define DRV260X_AUTOCAL_TIME_250MS		(1 << 4)
#define DRV260X_AUTOCAL_TIME_500MS		(2 << 4)
#define DRV260X_AUTOCAL_TIME_1000MS		(3 << 4)

/**
 * struct drv260x_data -
 * @input_dev - Pointer to the input device
 * @client - Pointer to the I2C client
 * @regmap - Register map of the device
 * @work - Work item used to off load the enable/disable of the vibration
 * @enable_gpio - Pointer to the gpio used for enable/disabling
 * @regulator - Pointer to the regulator for the IC
 * @magnitude - Magnitude of the vibration event
 * @mode - The operating mode of the IC (LRA_NO_CAL, ERM or LRA)
 * @library - The vibration library to be used
 * @rated_voltage - The rated_voltage of the actuator
 * @overdriver_voltage - The over drive voltage of the actuator
**/
struct drv260x_data {
	struct input_dev *input_dev;
	struct i2c_client *client;
	struct regmap *regmap;
	struct work_struct work;
	int enable_gpio;
	int pwm_gpio;
	struct regulator *regulator;
	u32 magnitude;
	u32 mode;
	u32 library;
	int rated_voltage;
	int overdrive_voltage;
	int drive_time;
	struct pinctrl_state *pin_active;
	struct pinctrl_state *pin_sleep;
	struct pinctrl *pinctrl;
	int init_result;
	struct work_struct init_work;
};

static const struct reg_default drv260x_reg_defs[] = {
	{ DRV260X_STATUS, 0xe0 },
	{ DRV260X_MODE, 0x40 },
	{ DRV260X_RT_PB_IN, 0x00 },
	{ DRV260X_LIB_SEL, 0x00 },
	{ DRV260X_WV_SEQ_1, 0x01 },
	{ DRV260X_WV_SEQ_2, 0x00 },
	{ DRV260X_WV_SEQ_3, 0x00 },
	{ DRV260X_WV_SEQ_4, 0x00 },
	{ DRV260X_WV_SEQ_5, 0x00 },
	{ DRV260X_WV_SEQ_6, 0x00 },
	{ DRV260X_WV_SEQ_7, 0x00 },
	{ DRV260X_WV_SEQ_8, 0x00 },
	{ DRV260X_GO, 0x00 },
	{ DRV260X_OVERDRIVE_OFF, 0x00 },
	{ DRV260X_SUSTAIN_P_OFF, 0x00 },
	{ DRV260X_SUSTAIN_N_OFF, 0x00 },
	{ DRV260X_BRAKE_OFF, 0x00 },
	{ DRV260X_A_TO_V_CTRL, 0x05 },
	{ DRV260X_A_TO_V_MIN_INPUT, 0x19 },
	{ DRV260X_A_TO_V_MAX_INPUT, 0xff },
	{ DRV260X_A_TO_V_MIN_OUT, 0x19 },
	{ DRV260X_A_TO_V_MAX_OUT, 0xff },
	{ DRV260X_RATED_VOLT, 0x3e },
	{ DRV260X_OD_CLAMP_VOLT, 0x8c },
	{ DRV260X_CAL_COMP, 0x0c },
	{ DRV260X_CAL_BACK_EMF, 0x6c },
	{ DRV260X_FEEDBACK_CTRL, 0x36 },
	{ DRV260X_CTRL1, 0x93 },
	{ DRV260X_CTRL2, 0xfa },
	{ DRV260X_CTRL3, 0xa0 },
	{ DRV260X_CTRL4, 0x20 },
	{ DRV260X_CTRL5, 0x80 },
	{ DRV260X_LRA_LOOP_PERIOD, 0x33 },
	{ DRV260X_VBAT_MON, 0x00 },
	{ DRV260X_LRA_RES_PERIOD, 0x00 },
};

#define DRV260X_DEF_RATED_VOLT		0x90
#define DRV260X_DEF_OD_CLAMP_VOLT	0x90

static int drv260x_calculate_drivetime(unsigned int frequency)
{
	return (5000/frequency-5);
}
/**
 * Rated and Overdriver Voltages:
 * Calculated using the formula r = v * 255 / 5.6
 * where r is what will be written to the register
 * and v is the rated or overdriver voltage of the actuator
 **/
static int drv260x_calculate_overvoltage(unsigned int voltage)
{
	return (voltage * 255 / 5600);
}
static int drv260x_calculate_ratedvoltage(unsigned int voltage)
{
	return (voltage * 255 / 5300);
}

static void drv260x_worker(struct work_struct *work)
{
	struct drv260x_data *haptics = container_of(work, struct drv260x_data, work);
	int error;

	if(!haptics->magnitude){
		error = regmap_write(haptics->regmap, DRV260X_MODE, DRV260X_STANDBY);
		if (error)
			dev_err(&haptics->client->dev,
				"Failed to enter standby mode: %d\n", error);
		gpio_set_value(haptics->enable_gpio, 0);
		return;
	}

	gpio_set_value(haptics->enable_gpio, 1);
	/* Data sheet says to wait 250us before trying to communicate */
	udelay(250);

	error = regmap_write(haptics->regmap,
			     DRV260X_RT_PB_IN, haptics->magnitude);
	if (error)
		dev_err(&haptics->client->dev,
			"Failed to set magnitude: %d\n", error);

	error = regmap_write(haptics->regmap,
			     DRV260X_MODE, DRV260X_RT_PLAYBACK);
	if (error) {
		dev_err(&haptics->client->dev,
			"Failed to write set mode: %d\n", error);
		return;
	}
}

static int drv260x_haptics_play(struct input_dev *input, void *data,
				struct ff_effect *effect)
{
	struct drv260x_data *haptics = input_get_drvdata(input);
	int magnitude = 0;

	if(!haptics->init_result)
		return 0;//-EBUSY

	haptics->mode = DRV260X_LRA_NO_CAL_MODE;

	if (effect->u.rumble.strong_magnitude > 0)
		magnitude = effect->u.rumble.strong_magnitude;
	else if (effect->u.rumble.weak_magnitude > 0)
		magnitude = effect->u.rumble.weak_magnitude;
	else
		magnitude = 0;

	haptics->magnitude = magnitude >> 8;

	schedule_work(&haptics->work);

	return 0;
}

static void drv260x_close(struct input_dev *input)
{
	struct drv260x_data *haptics = input_get_drvdata(input);
	int error;

	cancel_work_sync(&haptics->work);

	error = regmap_write(haptics->regmap, DRV260X_MODE, DRV260X_STANDBY);
	if (error)
		dev_err(&haptics->client->dev,
			"Failed to enter standby mode: %d\n", error);

	gpio_set_value(haptics->enable_gpio, 0);
}

static const struct reg_default drv260x_lra_cal_regs[] = {
	{0x16, 0x2d},
	{0x17, 0x9b},
	{0x18, 0x0b},
	{0x19, 0xcf},
	{0x1a, 0xb7},
	{0x1b, 0x90},
	//{0x1c, 0xf5},
	{0x1c, 0x75},
	//{0x1d, 0xa0},
	{0x1d, 0xa8},
	{0x1e, 0x20},
};

static const struct reg_default drv260x_lra_init_regs[] = {
	{ DRV260X_MODE, DRV260X_RT_PLAYBACK },
	{ DRV260X_A_TO_V_CTRL, DRV260X_AUDIO_HAPTICS_PEAK_20MS |
		DRV260X_AUDIO_HAPTICS_FILTER_125HZ },
	{ DRV260X_A_TO_V_MIN_INPUT, DRV260X_AUDIO_HAPTICS_MIN_IN_VOLT },
	{ DRV260X_A_TO_V_MAX_INPUT, DRV260X_AUDIO_HAPTICS_MAX_IN_VOLT },
	{ DRV260X_A_TO_V_MIN_OUT, DRV260X_AUDIO_HAPTICS_MIN_OUT_VOLT },
	{ DRV260X_A_TO_V_MAX_OUT, DRV260X_AUDIO_HAPTICS_MAX_OUT_VOLT },
	{ DRV260X_FEEDBACK_CTRL, DRV260X_FB_REG_LRA_MODE |
		DRV260X_BRAKE_FACTOR_2X | DRV260X_LOOP_GAIN_MED |
		DRV260X_BEMF_GAIN_3 },
	{ DRV260X_CTRL1, DRV260X_STARTUP_BOOST },
	{ DRV260X_CTRL2, DRV260X_SAMP_TIME_250 },
	{ DRV260X_CTRL3, DRV260X_NG_THRESH_2 | DRV260X_ANANLOG_IN },
	{ DRV260X_CTRL4, DRV260X_AUTOCAL_TIME_500MS },
};

static const struct reg_default drv260x_erm_cal_regs[] = {
	{ DRV260X_MODE, DRV260X_AUTO_CAL },
	{ DRV260X_A_TO_V_MIN_INPUT, DRV260X_AUDIO_HAPTICS_MIN_IN_VOLT },
	{ DRV260X_A_TO_V_MAX_INPUT, DRV260X_AUDIO_HAPTICS_MAX_IN_VOLT },
	{ DRV260X_A_TO_V_MIN_OUT, DRV260X_AUDIO_HAPTICS_MIN_OUT_VOLT },
	{ DRV260X_A_TO_V_MAX_OUT, DRV260X_AUDIO_HAPTICS_MAX_OUT_VOLT },
	{ DRV260X_FEEDBACK_CTRL, DRV260X_BRAKE_FACTOR_3X |
		DRV260X_LOOP_GAIN_MED | DRV260X_BEMF_GAIN_2 },
	{ DRV260X_CTRL1, DRV260X_STARTUP_BOOST },
	{ DRV260X_CTRL2, DRV260X_SAMP_TIME_250 | DRV260X_BLANK_TIME_75 |
		DRV260X_IDISS_TIME_75 },
	{ DRV260X_CTRL3, DRV260X_NG_THRESH_2 | DRV260X_ERM_OPEN_LOOP },
	{ DRV260X_CTRL4, DRV260X_AUTOCAL_TIME_500MS },
};

static ssize_t drv260x_dump(struct device *dev, 
		struct device_attribute *attr, char *buf)
{
	struct drv260x_data *haptics = dev_get_drvdata(dev);
	int i,value;
	int idx = 0;
	
	for(i=0;i<ARRAY_SIZE(drv260x_reg_defs);i++){
		regmap_read(haptics->regmap,drv260x_reg_defs[i].reg,&value);
		idx+=sprintf(&buf[idx],"0x%02x register = 0x%02x\n",drv260x_reg_defs[i].reg,value);
	}

	return idx;
}

static ssize_t drv260x_reg_control(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	char rw[10];
	int reg,value;
	struct drv260x_data *haptics = dev_get_drvdata(dev);

	sscanf(buf,"%s %x %x",rw,&reg, &value);
	if(!strcmp(rw,"read")){
		regmap_read(haptics->regmap,reg,&value);
		dev_info(&haptics->client->dev,"read from [%x] value = 0x%2x\n", reg, value);
	}else if(!strcmp(rw,"write")){
#ifndef CONFIG_USER_KERNEL
		regmap_write(haptics->regmap, reg, value);
		dev_info(&haptics->client->dev,"write to [%x] value = 0x%2x\n", reg, value);
#else
		dev_info(&haptics->client->dev,"write is disabled from userspace\n");
#endif
	}

	return size;
}

static ssize_t drv260x_enable(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	int value;
	struct drv260x_data *haptics = dev_get_drvdata(dev);

	sscanf(buf,"%d", &value);

	gpio_set_value(haptics->enable_gpio, !!value);

	return size;
}

static DEVICE_ATTR(dump, S_IRUGO, drv260x_dump, NULL);
static DEVICE_ATTR(reg_control, S_IWUSR, NULL, drv260x_reg_control);
static DEVICE_ATTR(enable, S_IWUSR, NULL, drv260x_enable);

static struct attribute *drv260x_attrs[] = {
	&dev_attr_dump.attr,
	&dev_attr_reg_control.attr,
	&dev_attr_enable.attr,
	NULL,
};

static struct attribute_group drv260x_attr_group = {
	.attrs = drv260x_attrs,
};

static int drv260x_do_reset(struct drv260x_data *haptics)
{
	int error; 
	unsigned int result;

	error = regmap_write(haptics->regmap, DRV260X_MODE, 1<<7);
	if (error) {
		dev_err(&haptics->client->dev,
			"(%d) Failed to write DRV260X_MODE register: %d\n",
			__LINE__,error);
		return error;
	}

	do {
		usleep_range(20000,50000);
		error = regmap_read(haptics->regmap, DRV260X_MODE, &result);
		if (error) {
			dev_err(&haptics->client->dev,
				"(%d) Failed to read DRV260X_MODE register: %d\n",
				__LINE__,error);
			return error;
		}
	} while (result&(1<<7));

	return 0;

}

static int drv260x_cal(struct drv260x_data *haptics)
{
	int error;
	unsigned int val;

	error = regmap_write(haptics->regmap, DRV260X_MODE, DRV260X_AUTO_CAL);
	if (error) {
		dev_err(&haptics->client->dev,
				"Failed to write DRV260X_MODE register: %d\n",
				error);
		return error;
	}


	error = regmap_write(haptics->regmap, DRV260X_GO, DRV260X_GO_BIT);
	if (error) {
		dev_err(&haptics->client->dev,
				"Failed to write GO register: %d\n",
				error);
		return error;
	}

	do {
		usleep_range(10000,50000);
		error = regmap_read(haptics->regmap, DRV260X_GO, &val);
		if (error) {
			dev_err(&haptics->client->dev,
					"Failed to read GO register: %d\n",
					error);
			return error;
		}
	} while (val == DRV260X_GO_BIT);

	error = regmap_read(haptics->regmap, DRV260X_STATUS, &val);
	if (error) {
		dev_err(&haptics->client->dev,
				"Failed to read status register: %d\n",
				error);
		return error;
	}

	if(val&(1<<3)){
		dev_err(&haptics->client->dev,
				"Failed to calibration\n");
		return -ENODEV;
	}

	//regmap_read(haptics->regmap, 0x18, &val);printk("drv260x: reg[0x18] = 0x%02x\n",val);
	//regmap_read(haptics->regmap, 0x19, &val);printk("drv260x: reg[0x19] = 0x%02x\n",val);
	//regmap_read(haptics->regmap, 0x1a, &val);printk("drv260x: reg[0x1a] = 0x%02x\n",val);
	dev_info(&haptics->client->dev,"Calibration Passed\n");

	return 0;

}

static void drv260x_do_init_work(struct work_struct *init_work)
{
	struct drv260x_data *haptics = container_of(init_work, struct drv260x_data, init_work);
	int error;

	gpio_set_value(haptics->enable_gpio, 1);
	udelay(250);

	error = drv260x_do_reset(haptics);
	if(error)
		goto err_return;

	error = regmap_register_patch(haptics->regmap,
			drv260x_lra_cal_regs,
			ARRAY_SIZE(drv260x_lra_cal_regs));
	if (error) {
		dev_err(&haptics->client->dev,
				"Failed to write LRA cal registers: %d\n",
				error);
		goto err_return;
	}

	error = drv260x_cal(haptics);
	if(error){
		goto err_return;
	}

	error = regmap_update_bits(haptics->regmap, DRV260X_LIB_SEL,
			DRV260X_LIB_SEL_MASK,
			haptics->library);
	if (error) {
		dev_err(&haptics->client->dev,
				"Failed to write DRV260X_LIB_SEL register: %d\n",
				error);
		goto err_return;
	}

	haptics->init_result = 1;
	gpio_set_value(haptics->enable_gpio, 0);

	return;

err_return:
	haptics->init_result = 0;
	gpio_set_value(haptics->enable_gpio, 0);


}

static const struct regmap_config drv260x_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = DRV260X_MAX_REG,
	.reg_defaults = drv260x_reg_defs,
	.num_reg_defaults = ARRAY_SIZE(drv260x_reg_defs),
	.cache_type = REGCACHE_NONE,
};

#ifdef CONFIG_OF
static int drv260x_parse_dt(struct device *dev,
			    struct drv260x_data *haptics)
{
	struct device_node *np = dev->of_node;
	unsigned int voltage,frequency;
	int error;

        haptics->pinctrl = devm_pinctrl_get(dev);
        if (IS_ERR_OR_NULL(haptics->pinctrl)) {
                error = -ENODEV;
		return error;
        }

        haptics->pin_active = pinctrl_lookup_state(haptics->pinctrl, "haptics_en_active");
        if (IS_ERR(haptics->pin_active)) {
                dev_err(dev, "Unable find haptics_en_active\n");
                error = PTR_ERR(haptics->pin_active);
		return error;
        }

        haptics->pin_sleep = pinctrl_lookup_state(haptics->pinctrl, "haptics_en_sleep");
        if (IS_ERR(haptics->pin_sleep)) {
                dev_err(dev, "Unable find haptics_en_sleep\n");
                error = PTR_ERR(haptics->pin_sleep);
		return error;
        }

        error = pinctrl_select_state(haptics->pinctrl, haptics->pin_active);
        if (error < 0) {
                dev_err(dev, "Unable select active state in pinctrl\n");
		return error;
        }

	error = of_property_read_u32(np, "mode", &haptics->mode);
	if (error) {
		dev_err(dev, "%s: No entry for mode\n", __func__);
		return error;
	}

	error = of_property_read_u32(np, "library-sel", &haptics->library);
	if (error) {
		dev_err(dev, "%s: No entry for library selection\n",
			__func__);
		return error;
	}

	error = of_property_read_u32(np, "vib-rated-mv", &voltage);
	if (!error)
		haptics->rated_voltage = drv260x_calculate_ratedvoltage(voltage);


	error = of_property_read_u32(np, "vib-overdrive-mv", &voltage);
	if (!error)
		haptics->overdrive_voltage = drv260x_calculate_overvoltage(voltage);

	of_property_read_u32(np, "vib-frequency", &frequency);
	if (!error)
		haptics->drive_time = drv260x_calculate_drivetime(frequency);

	haptics->enable_gpio = of_get_named_gpio(np, "enable-gpio", 0);
	if (!gpio_is_valid(haptics->enable_gpio))
		return haptics->enable_gpio;

	haptics->pwm_gpio = of_get_named_gpio(np, "pwm-gpio", 0);

	return 0;
}
#else
static inline int drv260x_parse_dt(struct device *dev,
				   struct drv260x_data *haptics)
{
	dev_err(dev, "no platform data defined\n");

	return -EINVAL;
}
#endif

static int drv260x_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	const struct drv260x_platform_data *pdata = dev_get_platdata(&client->dev);
	struct drv260x_data *haptics;
	int error;

	haptics = devm_kzalloc(&client->dev, sizeof(*haptics), GFP_KERNEL);
	if (!haptics)
		return -ENOMEM;

	haptics->overdrive_voltage = DRV260X_DEF_OD_CLAMP_VOLT;
	haptics->rated_voltage = DRV260X_DEF_RATED_VOLT;

	if (pdata) {
		haptics->mode = pdata->mode;
		haptics->library = pdata->library_selection;
		if (pdata->vib_overdrive_voltage)
			haptics->overdrive_voltage = drv260x_calculate_overvoltage(pdata->vib_overdrive_voltage);
		if (pdata->vib_rated_voltage)
			haptics->rated_voltage = drv260x_calculate_ratedvoltage(pdata->vib_rated_voltage);
	} else if (client->dev.of_node) {
		error = drv260x_parse_dt(&client->dev, haptics);
		if (error)
			return error;
	} else {
		dev_err(&client->dev, "Platform data not set\n");
		return -ENODEV;
	}


	if (haptics->mode < DRV260X_LRA_MODE ||
	    haptics->mode > DRV260X_ERM_MODE) {
		dev_err(&client->dev,
			"Vibrator mode is invalid: %i\n",
			haptics->mode);
		return -EINVAL;
	}

	if (haptics->library < DRV260X_LIB_EMPTY ||
	    haptics->library > DRV260X_ERM_LIB_F) {
		dev_err(&client->dev,
			"Library value is invalid: %i\n", haptics->library);
		return -EINVAL;
	}

	if (haptics->mode == DRV260X_LRA_MODE &&
	    haptics->library != DRV260X_LIB_EMPTY &&
	    haptics->library != DRV260X_LIB_LRA) {
		dev_err(&client->dev,
			"LRA Mode with ERM Library mismatch\n");
		return -EINVAL;
	}

	if (haptics->mode == DRV260X_ERM_MODE &&
	    (haptics->library == DRV260X_LIB_EMPTY ||
	     haptics->library == DRV260X_LIB_LRA)) {
		dev_err(&client->dev,
			"ERM Mode with LRA Library mismatch\n");
		return -EINVAL;
	}

	haptics->regulator = devm_regulator_get(&client->dev, "vbat");
	if (IS_ERR(haptics->regulator)) {
		dev_info(&client->dev, "Can't get regulator\n");
		//return -ENODEV;
	}

	if(gpio_is_valid(haptics->enable_gpio)){
		error = gpio_request(haptics->enable_gpio, "drv260x_enable_gpio");
                if (error) {
                        dev_err(&client->dev,
                                        "%s-->unable to request gpio [%d]\n",
                                        __func__,haptics->enable_gpio);
			return -EINVAL;
                }
                error = gpio_direction_output(haptics->enable_gpio,0);
                if (error) {
                        dev_err(&client->dev,
                                "%s-->unable to set direction for gpio [%d]\n",
                                __func__,haptics->enable_gpio);
                        return -EINVAL;
                }
	}

	if(gpio_is_valid(haptics->pwm_gpio)){
		error = gpio_request(haptics->pwm_gpio, "drv260x_pwm_gpio");
                if (error) {
                        dev_err(&client->dev,
                                        "%s-->unable to request gpio [%d]\n",
                                        __func__,haptics->pwm_gpio);
			return -EINVAL;
                }
                error = gpio_direction_output(haptics->pwm_gpio,0);
                if (error) {
                        dev_err(&client->dev,
                                "%s-->unable to set direction for gpio [%d]\n",
                                __func__,haptics->pwm_gpio);
                        return -EINVAL;
                }
	}

	haptics->input_dev = devm_input_allocate_device(&client->dev);
	if (!haptics->input_dev) {
		dev_err(&client->dev, "Failed to allocate input device\n");
		return -ENOMEM;
	}

	haptics->input_dev->name = "drv260x:haptics";
	haptics->input_dev->close = drv260x_close;
	input_set_drvdata(haptics->input_dev, haptics);
	input_set_capability(haptics->input_dev, EV_FF, FF_RUMBLE);

	error = input_ff_create_memless(haptics->input_dev, NULL,
					drv260x_haptics_play);
	if (error) {
		dev_err(&client->dev, "input_ff_create() failed: %d\n",
			error);
		return error;
	}

	INIT_WORK(&haptics->work, drv260x_worker);
	INIT_WORK(&haptics->init_work, drv260x_do_init_work);

	haptics->client = client;
	i2c_set_clientdata(client, haptics);

	haptics->regmap = devm_regmap_init_i2c(client, &drv260x_regmap_config);
	if (IS_ERR(haptics->regmap)) {
		error = PTR_ERR(haptics->regmap);
		dev_err(&client->dev, "Failed to allocate register map: %d\n",
			error);
		return error;
	}

	schedule_work(&haptics->init_work);

	error = input_register_device(haptics->input_dev);
	if (error) {
		dev_err(&client->dev, "couldn't register input device: %d\n",
			error);
		return error;
	}


	error = sysfs_create_group(&client->dev.kobj, &drv260x_attr_group);
	if (error) {
		dev_err(&client->dev,
				"%s-->Unable to create sysfs,"
				" errors: %d\n", __func__, error);
		return error;
	}

	return 0;
}

static int __maybe_unused drv260x_suspend(struct device *dev)
{
	struct drv260x_data *haptics = dev_get_drvdata(dev);
	int ret = 0;

	mutex_lock(&haptics->input_dev->mutex);

	if (haptics->input_dev->users) {
		ret = regmap_update_bits(haptics->regmap,
					 DRV260X_MODE,
					 DRV260X_STANDBY_MASK,
					 DRV260X_STANDBY);
		if (ret) {
			dev_err(dev, "Failed to set standby mode\n");
			goto out;
		}

		gpio_set_value(haptics->enable_gpio, 0);
	
		ret = pinctrl_select_state(haptics->pinctrl, haptics->pin_sleep);
		if (ret < 0) {
			dev_err(dev, "Unable select active state in pinctrl\n");
			goto out;
        	}

		if (!IS_ERR(haptics->regulator)) {
			ret = regulator_disable(haptics->regulator);
			if (ret) {
				dev_err(dev, "Failed to disable regulator\n");
				regmap_update_bits(haptics->regmap,
						DRV260X_MODE,
						DRV260X_STANDBY_MASK, 0);
			}
		}
	}
out:
	mutex_unlock(&haptics->input_dev->mutex);
	return ret;
}

static int __maybe_unused drv260x_resume(struct device *dev)
{
	struct drv260x_data *haptics = dev_get_drvdata(dev);
	int ret = 0;

	mutex_lock(&haptics->input_dev->mutex);

	if (haptics->input_dev->users) {
		if (!IS_ERR(haptics->regulator)) {
			ret = regulator_enable(haptics->regulator);
			if (ret) {
				dev_err(dev, "Failed to enable regulator\n");
				goto out;
			}
		}

		ret = pinctrl_select_state(haptics->pinctrl, haptics->pin_active);
		if (ret < 0) {
			dev_err(dev, "Unable select active state in pinctrl\n");
			goto out;
        	}
#if 0
		gpio_set_value(haptics->enable_gpio, 1);

		ret = regmap_update_bits(haptics->regmap,
				DRV260X_MODE,
				DRV260X_STANDBY_MASK, 0);
		if (ret) {
			dev_err(dev, "Failed to unset standby mode\n");
			if (!IS_ERR(haptics->regulator)) {
				regulator_disable(haptics->regulator);
			}
			goto out;
		}
#endif
	}

out:
	mutex_unlock(&haptics->input_dev->mutex);
	return ret;
}

static SIMPLE_DEV_PM_OPS(drv260x_pm_ops, drv260x_suspend, drv260x_resume);

static const struct i2c_device_id drv260x_id[] = {
	{ "drv2605l", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, drv260x_id);

#ifdef CONFIG_OF
static const struct of_device_id drv260x_of_match[] = {
	{ .compatible = "ti,drv2604", },
	{ .compatible = "ti,drv2604l", },
	{ .compatible = "ti,drv2605", },
	{ .compatible = "ti,drv2605l", },
	{ }
};
MODULE_DEVICE_TABLE(of, drv260x_of_match);
#endif

static struct i2c_driver drv260x_driver = {
	.probe		= drv260x_probe,
	.driver		= {
		.name	= "drv260x-haptics",
		.of_match_table = of_match_ptr(drv260x_of_match),
		.pm	= &drv260x_pm_ops,
	},
	.id_table = drv260x_id,
};
module_i2c_driver(drv260x_driver);

MODULE_DESCRIPTION("TI DRV260x haptics driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dan Murphy <dmurphy@ti.com>");
