/*
 * Copyright (c) 2014 Ismail Kose, ismail.kose@maximintegrated.com
 * Copyright (c) 2014 JY Kim, jy.kim@maximintegrated.com
 * Copyright (c) 2013 Maxim Integrated Products, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include "linux/max86x00.h"
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

#define LVS2_I2C11_1P8_HPM_LOAD 19000 //uA

/* I2C function */
int max86000_write_reg(struct max86x00_device_data *device,
	u8 reg_addr, u8 data)
{
	int err;
	int tries = 0;
	u8 buffer[2] = { reg_addr, data };
	struct i2c_msg msgs[] = {
		{
			.addr = device->client->addr,
			.flags = device->client->flags & I2C_M_TEN,
			.len = 2,
			.buf = buffer,
		},
	};

	do {
		mutex_lock(&device->i2clock);
		err = i2c_transfer(device->client->adapter, msgs, 1);
		mutex_unlock(&device->i2clock);
		if (err != 1)
			msleep_interruptible(MAX86000_I2C_RETRY_DELAY);

	} while ((err != 1) && (++tries < MAX86000_I2C_MAX_RETRIES));

	if (err != 1) {
		pr_err("%s -write transfer error\n", __func__);
		err = -EIO;
		return err;
	}

	return 0;
}

int max86000_read_reg(struct max86x00_device_data *data,
	u8 *buffer, int length)
{
	int err = -1;
	int tries = 0; /* # of attempts to read the device */

	struct i2c_msg msgs[] = {
		{
			.addr = data->client->addr,
			.flags = data->client->flags & I2C_M_TEN,
			.len = 1,
			.buf = buffer,
		},
		{
			.addr = data->client->addr,
			.flags = (data->client->flags & I2C_M_TEN) | I2C_M_RD,
			.len = length,
			.buf = buffer,
		},
	};

	do {
		mutex_lock(&data->i2clock);
		err = i2c_transfer(data->client->adapter, msgs, 2);
		mutex_unlock(&data->i2clock);
		if (err != 2)
			msleep_interruptible(MAX86000_I2C_RETRY_DELAY);
	} while ((err != 2) && (++tries < MAX86000_I2C_MAX_RETRIES));

	if (err != 2) {
		pr_err("%s -read transfer error\n", __func__);
		err = -EIO;
	} else
		err = 0;

	return err;
}

/* Device Control */
int max86000_regulator_onoff(struct max86x00_device_data *data, int onoff)
{
#if 0
	int rc = 0;

	if (!data->regulator_vdd_1p8) {
		atomic_set(&data->regulator_is_enable, onoff);
		pr_info("%s:%d, onoff: %d\n", __func__, __LINE__, onoff);
		return rc;
	}

#ifdef CONFIG_SENSORS_MAX86X00_LED_POWER
	if (!data->regulator_led_3p3) {
		pr_info("%s - Warning: regulator_led_3p3 does not exist\n", __func__);
		atomic_set(&data->regulator_is_enable, onoff);
		return rc;
	}
#endif

	if (atomic_read(&data->regulator_is_enable) == 0 && onoff == HRM_LDO_ON) {
		regulator_set_voltage(data->regulator_vdd_1p8, 1800000, 1800000);
		rc = regulator_enable(data->regulator_vdd_1p8);
		if (rc) {
			pr_err("%s - enable vdd_1p8 failed, rc=%d\n",
				__func__, rc);
			goto done;
		}
#ifdef CONFIG_SENSORS_MAX86X00_LED_POWER
		gpio_set_value_cansleep(data->hrm_en, 1);
		pr_err("%s - hrm_en enable : %d\n", __func__,
			gpio_get_value_cansleep(data->hrm_en));
/*
#else
		regulator_set_voltage(data->regulator_led_3p3, 3300000, 3300000);
		rc = regulator_enable(data->regulator_led_3p3);
		if (rc) {
			pr_err("%s - enable led_3p3 failed, rc=%d\n",
				__func__, rc);
			goto done;
		}
*/
#endif
	} else if (atomic_read(&data->regulator_is_enable) == 1 && onoff == HRM_LDO_OFF) {
		rc = regulator_disable(data->regulator_vdd_1p8);
		if (rc) {
			pr_err("%s - disable vdd_1p8 failed, rc=%d\n",
				__func__, rc);
			goto done;
		}
#ifdef CONFIG_SENSORS_MAX86X00_LED_POWER
		gpio_set_value_cansleep(data->hrm_en, 0);
		pr_err("%s - hrm_en disable : %d\n", __func__,
			gpio_get_value_cansleep(data->hrm_en));
/*
#else
		rc = regulator_disable(data->regulator_led_3p3);
		if (rc) {
			pr_err("%s - disable led_3p3 failed, rc=%d\n",
				__func__, rc);
			goto done;
		}
*/
#endif
	} else {
		pr_err("%s - Regulator setting error. regulator_is_enable: %d, onoff: %d\n",
													__func__, atomic_read(&data->regulator_is_enable), onoff);
	}

	atomic_set(&data->regulator_is_enable, onoff);
done:
	return rc;
#endif
	atomic_set(&data->regulator_is_enable, onoff);
	return 0;
}

static int max86000_update_led_current(struct max86x00_device_data *data,
	int new_led_uA,
	int led_num)
{
	int err;
	u8 old_led_reg_val;
	u8 new_led_reg_val;

	if (new_led_uA > MAX86000_MAX_CURRENT) {
		pr_err("%s - Tried to set LED%d to %duA. Max is %duA\n",
				__func__, led_num, new_led_uA, MAX86000_MAX_CURRENT);
		new_led_uA = MAX86000_MAX_CURRENT;
	} else if (new_led_uA < MAX86000_MIN_CURRENT) {
		pr_err("%s - Tried to set LED%d to %duA. Min is %duA\n",
				__func__, led_num, new_led_uA, MAX86000_MIN_CURRENT);
		new_led_uA = MAX86000_MIN_CURRENT;
	}

	new_led_reg_val = new_led_uA / MAX86000_CURRENT_PER_STEP;

	old_led_reg_val = MAX86000_LED_CONFIGURATION;
	err = max86000_read_reg(data, &old_led_reg_val, 1);
	if (err) {
		pr_err("%s - error updating led current!\n", __func__);
		return -EIO;
	}

	if (led_num == MAX86X00_LED1)
		new_led_reg_val = (old_led_reg_val & 0xF0) | (new_led_reg_val);
	else
		new_led_reg_val = (old_led_reg_val & 0x0F) | (new_led_reg_val << 4);

	pr_err("%s - setting LED reg to %.2X\n", __func__, new_led_reg_val);

	err = max86000_write_reg(data, MAX86000_LED_CONFIGURATION, new_led_reg_val);
	if (err) {
		pr_err("%s - error updating led current!\n", __func__);
		return -EIO;
	}

	return 0;
}

static int max86100_update_led_current(struct max86x00_device_data *data,
	int new_led_uA,
	int led_num)
{
	int err;
	u8 led_reg_val;
	int led_reg_addr;

	if (new_led_uA > MAX86100_MAX_CURRENT) {
		pr_err("%s - Tried to set LED%d to %duA. Max is %duA\n",
				__func__, led_num, new_led_uA, MAX86100_MAX_CURRENT);
		new_led_uA = MAX86100_MAX_CURRENT;
	} else if (new_led_uA < MAX86100_MIN_CURRENT) {
		pr_err("%s - Tried to set LED%d to %duA. Min is %duA\n",
				__func__, led_num, new_led_uA, MAX86100_MIN_CURRENT);
		new_led_uA = MAX86100_MIN_CURRENT;
	}

	led_reg_val = new_led_uA / MAX86100_CURRENT_PER_STEP;

	if (led_num == MAX86X00_LED1)
		led_reg_addr = MAX86100_LED1_PA;
	else
		led_reg_addr = MAX86100_LED2_PA;

	/* pr_err("%s - Setting LED%d to 0x%.2X (%duA)\n", __func__, led_num, led_reg_val, new_led_uA); */

	err = max86000_write_reg(data, led_reg_addr, led_reg_val);
	if (err != 0) {
		pr_err("%s - error initializing register 0x%.2X!\n",
			__func__, led_reg_addr);
		return -EIO;
	}

	return 0;
}

int max86000_prox_led_init(struct max86x00_device_data *data)
{
	int err = 0;
	data->prox_sample = 0;
	data->prox_detect = false;

	pr_info("%s\n", __func__);

	/* Write low power LED settings for prox sensing */
	err |= data->update_led(data, MAX86000_LED1_PROX_PA, MAX86X00_LED1);
	err |= data->update_led(data, MAX86000_LED2_PROX_PA, MAX86X00_LED2);
	err |= max86000_write_reg(data, MAX86000_SPO2_CONFIGURATION, 0x40);
	if (err != 0) {
		pr_err("%s - error initializing LED prox mode!\n",
			__func__);
		return -EIO;
	}

	data->agc_current[MAX86X00_LED1] = MAX86000_LED1_PROX_PA;
	data->agc_current[MAX86X00_LED2] = MAX86000_LED2_PROX_PA;

	return 0;
}

int max86100_prox_led_init(struct max86x00_device_data *data)
{
	int err = 0;
	data->prox_sample = 0;
	data->prox_detect = false;

	pr_info("%s\n", __func__);

	/* Write low power LED settings for prox sensing */
	err |= data->update_led(data, MAX86100_LED1_PROX_PA, MAX86X00_LED1);
	err |= data->update_led(data, MAX86100_LED2_PROX_PA, MAX86X00_LED2);
	err |= max86000_write_reg(data, MAX86100_SPO2_CONFIGURATION,
			0x03 | (MAX86100_SPO2_ADC_RGE << MAX86100_SPO2_ADC_RGE_OFFSET));
	err |= max86000_write_reg(data, MAX86100_FIFO_CONFIG, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing LED prox mode!\n",
			__func__);
		return -EIO;
	}

	data->agc_current[MAX86X00_LED1] = MAX86100_LED1_PROX_PA;
	data->agc_current[MAX86X00_LED2] = MAX86100_LED2_PROX_PA;

	return 0;
}

int max86000_hrm_led_init(struct max86x00_device_data *data)
{
	int err = 0;
	data->prox_sample = 0;
	data->prox_detect = true;
	data->reached_thresh[MAX86X00_LED1] = 0;
	data->reached_thresh[MAX86X00_LED2] = 0;
	pr_err("%s\n", __func__);

#if MAX86000_SAMPLE_RATE == 1
	err |= max86000_write_reg(data, MAX86000_SPO2_CONFIGURATION,
		(MAX86000_SPO2_HI_RES_EN << 6) | 0x07);
#endif

#if MAX86000_SAMPLE_RATE == 2
	err |= max86000_write_reg(data, MAX86000_SPO2_CONFIGURATION,
		(MAX86000_SPO2_HI_RES_EN << 6) | 0x0E);
#endif

#if MAX86000_SAMPLE_RATE == 4
	err |= max86000_write_reg(data, MAX86000_SPO2_CONFIGURATION,
		(MAX86000_SPO2_HI_RES_EN << 6) | 0x11);
#endif

	err |= max86000_write_reg(data, MAX86000_LED_CONFIGURATION,
		data->default_current);

	if (err) {
		pr_err("%s - error initializing LED hrm mode!\n",
			__func__);
	}

	data->agc_current[MAX86X00_LED1] =
		(data->default_current1 * MAX86000_CURRENT_PER_STEP);
	data->agc_current[MAX86X00_LED2] =
		(data->default_current2 * MAX86000_CURRENT_PER_STEP);

	return 0;
}

int max86100_hrm_led_init(struct max86x00_device_data *data)
{
	int err = 0;
	data->prox_sample = 0;
	data->prox_detect = true;
	data->reached_thresh[MAX86X00_LED1] = 0;
	data->reached_thresh[MAX86X00_LED2] = 0;
	pr_err("%s\n", __func__);

	/*write LED currents ir=1, red=2, violet=4*/
	err |= max86000_write_reg(data, MAX86100_LED1_PA,
		data->default_current1);
	err |= max86000_write_reg(data, MAX86100_LED2_PA,
			data->default_current2);

	err |= max86000_write_reg(data, MAX86100_SPO2_CONFIGURATION,
			0x0E | (MAX86100_SPO2_ADC_RGE << MAX86100_SPO2_ADC_RGE_OFFSET));

#if MAX86100_SAMPLE_RATE == 1
	err |= max86000_write_reg(data, MAX86100_FIFO_CONFIG,
			(0x02 << MAX86100_SMP_AVE_OFFSET) & MAX86100_SMP_AVE_MASK);
#endif

#if MAX86100_SAMPLE_RATE == 2
	err |= max86000_write_reg(data, MAX86100_FIFO_CONFIG,
			(0x01 << MAX86100_SMP_AVE_OFFSET) & MAX86100_SMP_AVE_MASK);
#endif

#if MAX86100_SAMPLE_RATE == 4
	err |= max86000_write_reg(data, MAX86100_FIFO_CONFIG,
			(0x00 << MAX86100_SMP_AVE_OFFSET) & MAX86100_SMP_AVE_MASK);
#endif
	if (err) {
		pr_err("%s - error initializing LED hrm mode!\n",
			__func__);
	}

	data->agc_current[MAX86X00_LED1] =
		(data->default_current1 * MAX86100_CURRENT_PER_STEP);
	data->agc_current[MAX86X00_LED2] =
		(data->default_current2 * MAX86100_CURRENT_PER_STEP);

	return 0;
}

int max86x00_prox_check(struct max86x00_device_data *data, int counts)
{
	int avg;
	int err;
	int prox_enter_thresh;
	int prox_exit_thresh;
	int prox_enter_db;
	int prox_exit_db;

	if (data->prox_sample == 0) {
		data->prox_sum = 0;
		memset(data->change_by_percent_of_range, 0,
			sizeof(data->change_by_percent_of_range));
		memset(data->change_by_percent_of_current_setting, 0,
			sizeof(data->change_by_percent_of_current_setting));
		memset(data->change_led_by_absolute_count, 0,
			sizeof(data->change_led_by_absolute_count));
		memset(data->agc_sum, 0, sizeof(data->agc_sum));
	}

	if (data->part_type < PART_TYPE_MAX86100A) {
		prox_enter_thresh = MAX86000_PROX_ENTER_THRESH;
		prox_exit_thresh = MAX86000_PROX_EXIT_THRESH;
		prox_enter_db = MAX86000_PROX_ENTER_DB;
		prox_exit_db = MAX86000_PROX_EXIT_DB;
	} else {
		prox_enter_thresh = MAX86100_PROX_ENTER_THRESH;
		prox_exit_thresh = MAX86100_PROX_EXIT_THRESH;
		prox_enter_db = MAX86100_PROX_ENTER_DB;
		prox_exit_db = MAX86100_PROX_EXIT_DB;
	}

	data->prox_sample++;
	data->prox_sum += counts;

	if (data->prox_detect) {
		if (data->prox_sample % prox_exit_db)
			return 0;

		avg = data->prox_sum / prox_exit_db;
		data->prox_sum = 0;
		if (avg <= prox_exit_thresh) {
			/* pr_err("%s - Finger Not Detected\n", __func__); */
			data->prox_detect = false;
			err = data->prox_led_init(data);
			if (err)
				return err;
		}

	} else {
		if (data->prox_sample % prox_enter_db)
			return 0;

		avg = data->prox_sum / prox_enter_db;
		data->prox_sum = 0;
		if (avg >= prox_enter_thresh) {
			/* pr_err("%s - Finger Detected\n", __func__); */
			data->prox_detect = true;
			err = data->hrm_led_init(data);
			if (err)
				return err;
		}
	}

	return 0;
}

static int agc_adj_calculator(struct max86x00_device_data *data,
			s32 *change_by_percent_of_range,
			s32 *change_by_percent_of_current_setting,
			s32 *change_led_by_absolute_count,
			s32 *set_led_to_absolute_count,
			s32 target_percent_of_range,
			s32 correction_coefficient,
			s32 allowed_error_in_percentage,
			s32 *reached_thresh,
			s32 current_average,
			s32 number_of_samples_averaged,
			s32 led_drive_current_value)
{
	s32 diode_min_val;
	s32 diode_max_val;
	s32 diode_min_current;
	s32 diode_fs_current;

	s32 current_percent_of_range = 0;
	s32 delta = 0;
	s32 desired_delta = 0;
	s32 current_power_percent = 0;

	if (change_by_percent_of_range == 0
			 || change_by_percent_of_current_setting == 0
			 || change_led_by_absolute_count == 0
			 || set_led_to_absolute_count == 0)
		return ILLEGAL_OUTPUT_POINTER;

	if (target_percent_of_range > 90 || target_percent_of_range < 10)
		return CONSTRAINT_VIOLATION;

	if (correction_coefficient > 100 || correction_coefficient < 0)
		return CONSTRAINT_VIOLATION;

	if (allowed_error_in_percentage > 100
			|| allowed_error_in_percentage < 0)
		return CONSTRAINT_VIOLATION;

#if ((MAX86000_MAX_DIODE_VAL-MAX86000_MIN_DIODE_VAL) <= 0 \
			 || (MAX86100_MAX_DIODE_VAL < 0) || (MAX86100_MIN_DIODE_VAL < 0))
	#error "Illegal max86000 diode Min/Max Pair"
#endif

#if ((MAX86000_CURRENT_FULL_SCALE) <= 0 \
		|| (MAX86100_MAX_CURRENT < 0) || (MAX86100_MIN_CURRENT < 0))
	#error "Illegal max86000 LED Min/Max current Pair"
#endif

#if ((MAX86100_MAX_DIODE_VAL-MAX86100_MIN_DIODE_VAL) <= 0 \
			 || (MAX86100_MAX_DIODE_VAL < 0) || (MAX86100_MIN_DIODE_VAL < 0))
	#error "Illegal max86100 diode Min/Max Pair"
#endif

#if ((MAX86100_CURRENT_FULL_SCALE) <= 0 \
		|| (MAX86100_MAX_CURRENT < 0) || (MAX86100_MIN_CURRENT < 0))
	#error "Illegal max86100 LED Min/Max current Pair"
#endif

	if (led_drive_current_value > MAX86100_MAX_CURRENT
			|| led_drive_current_value < MAX86100_MIN_CURRENT)
		return CONSTRAINT_VIOLATION;

	if (current_average < MAX86100_MIN_DIODE_VAL
				|| current_average > MAX86100_MAX_DIODE_VAL)
		return CONSTRAINT_VIOLATION;

	if (data->part_type < PART_TYPE_MAX86100A) {
		diode_min_val = MAX86000_MIN_DIODE_VAL;
		diode_max_val = MAX86000_MAX_DIODE_VAL;
		diode_min_current = MAX86000_MIN_CURRENT;
		diode_fs_current = MAX86000_CURRENT_FULL_SCALE;
	} else {
		diode_min_val = MAX86100_MIN_DIODE_VAL;
		diode_max_val = MAX86100_MAX_DIODE_VAL;
		diode_min_current = MAX86100_MIN_CURRENT;
		diode_fs_current = MAX86100_CURRENT_FULL_SCALE;
	}

	current_percent_of_range = 100 *
		(current_average - diode_min_val) /
		(diode_max_val - diode_min_val);

	delta = current_percent_of_range - target_percent_of_range;
	delta = delta * correction_coefficient / 100;

	if (!(*reached_thresh))
		allowed_error_in_percentage =
			allowed_error_in_percentage * 3 / 4;

	if (delta > -allowed_error_in_percentage
			&& delta < allowed_error_in_percentage) {
		*change_by_percent_of_range = 0;
		*change_by_percent_of_current_setting = 0;
		*change_led_by_absolute_count = 0;
		*set_led_to_absolute_count = led_drive_current_value;
		*reached_thresh = 1;
		return 0;
	}

	current_power_percent = 100 *
			(led_drive_current_value - diode_min_current) /
			diode_fs_current;

	if (delta < 0)
		desired_delta = -delta * (100 - current_power_percent) /
				(100 - current_percent_of_range);

	if (delta > 0)
		desired_delta = -delta * (current_power_percent)
				/ (current_percent_of_range);

	*change_by_percent_of_range = desired_delta;

	*change_led_by_absolute_count = (desired_delta
			* diode_fs_current / 100);
	*change_by_percent_of_current_setting =
			(*change_led_by_absolute_count * 100)
			/ (led_drive_current_value);
	*set_led_to_absolute_count = led_drive_current_value
			+ *change_led_by_absolute_count;
	return 0;
}

int max86x00_hrm_agc(struct max86x00_device_data *data, int counts, int led_num)
{
	int err = 0;
	int avg;

	if (led_num < MAX86X00_LED1 || led_num > MAX86X00_LED2)
		return -EIO;

	data->agc_sum[led_num] += counts;
	if ((data->prox_sample+1) % data->agc_min_num_samples)
		return 0;

	avg = data->agc_sum[led_num] / data->agc_min_num_samples;
	data->agc_sum[led_num] = 0;

	err = agc_adj_calculator(data,
			&data->change_by_percent_of_range[led_num],
			&data->change_by_percent_of_current_setting[led_num],
			&data->change_led_by_absolute_count[led_num],
			&data->agc_current[led_num],
			data->agc_led_out_percent,
			data->agc_corr_coeff,
			data->agc_sensitivity_percent,
			&data->reached_thresh[led_num],
			avg,
			data->agc_min_num_samples,
			data->agc_current[led_num]);
	if (err)
		return err;

	if (data->change_led_by_absolute_count[led_num] == 0)
		return 0;

	err = data->update_led(data, data->agc_current[led_num], led_num);
	if (err)
		pr_err("%s failed\n", __func__);
	return err;
}

static int max86000_init_device(struct max86x00_device_data *data)
{
	int err = 0;
	u8 recvData;

	err |= max86000_write_reg(data, MAX86000_MODE_CONFIGURATION, MAX86100_RESET_MASK);

	/* Interrupt Clear */
	recvData = MAX86000_INTERRUPT_STATUS;
	err |= max86000_read_reg(data, &recvData, 1);
	err |= max86000_write_reg(data, MAX86000_LED_CONFIGURATION, 0x00);
	err |= max86000_write_reg(data, MAX86000_UV_CONFIGURATION, 0x00);
	if (err != 0) {
		pr_err("%s - I2C error\n", __func__);
		return -EIO;
	}
	pr_info("%s done, part_type = %u\n", __func__, data->part_type);

	return 0;
}


int max86100_init_device(struct max86x00_device_data *data)
{
	int err = 0;
	u8 recvData;

	err |= max86000_write_reg(data, MAX86100_MODE_CONFIGURATION, MAX86100_RESET_MASK);

	/* Interrupt Clear */
	recvData = MAX86100_INTERRUPT_STATUS;
	err |= max86000_read_reg(data, &recvData, 1);

	/* Interrupt2 Clear */
	recvData = MAX86100_INTERRUPT_STATUS_2;
	err |= max86000_read_reg(data, &recvData, 1);
	if (err) {
		pr_err("%s - I2C error\n", __func__);
		return -EIO;
	}
	pr_info("%s done, part_type = %u\n", __func__, data->part_type);

	return 0;
}

static int max86000_hrm_enable(struct max86x00_device_data *data)
{
	int err = 0;
	data->led = 0;
	data->sample_cnt = 0;
	data->ir_sum = 0;
	data->r_sum = 0;

	/* Write LED and SPO2 settings */
	if (data->agc_is_enable)
		err |= data->prox_led_init(data);
	else
		err |= data->hrm_led_init(data);

	err |= max86000_write_reg(data, MAX86000_INTERRUPT_ENABLE, PROX_INT_MASK);
	err |= max86000_write_reg(data, MAX86000_FIFO_WRITE_POINTER, 0x00);
	err |= max86000_write_reg(data, MAX86000_OVF_COUNTER, 0x00);
	err |= max86000_write_reg(data, MAX86000_FIFO_READ_POINTER, 0x00);
	err |= max86000_write_reg(data, MAX86000_MODE_CONFIGURATION,
			MAX86100_GESTURE_EN_MASK | MAX86100_MODE_HRSPO2);

	if (err != 0) {
		pr_err("%s - error initializing hrm mode!\n", __func__);
		return -EIO;
	}

	if (!atomic_read(&data->irq_enable)) {
		enable_irq(data->irq);
		atomic_set(&data->irq_enable, 1);
	}

	return 0;
}

int max86100_acfd_enable(struct max86x00_device_data *data)
{
	int err = 0;
	u8 flex_config[2] = {0, };
	u8 recv_data;

	data->led = 0;
	data->sample_cnt = 0;
	data->led_sum[0] = 0;
	data->led_sum[1] = 0;
	data->led_sum[2] = 0;
	data->led_sum[3] = 0;
	data->num_samples = 0;
	data->flex_mode = 0;

	pr_info("%s:%d\n", __func__, __LINE__);
	flex_config[0] = IR_LED_CH;
	flex_config[1] = 0x00;
	if (flex_config[0] & MAX86100_S1_MASK) {
		data->num_samples++;
		data->flex_mode |= (1 << 0);
	}
	if (flex_config[0] & MAX86100_S2_MASK) {
		data->num_samples++;
		data->flex_mode |= (1 << 1);
	}
	if (flex_config[1] & MAX86100_S3_MASK) {
		data->num_samples++;
		data->flex_mode |= (1 << 2);
	}
	if (flex_config[1] & MAX86100_S4_MASK) {
		data->num_samples++;
		data->flex_mode |= (1 << 3);
	}

	pr_info("%s - flexmode : 0x%02x, num_samples : %d\n",
		__func__, data->flex_mode, data->num_samples);

	pr_info("%s done\n", __func__);
	if (!atomic_read(&data->irq_enable)) {
		enable_irq(data->irq);
		atomic_set(&data->irq_enable, 1);
	}

	err |= max86000_write_reg(data, 0xFF, 0x54);
	err |= max86000_write_reg(data, 0xFF, 0x4d);
	/* PW_EN = 0 */
	err |= max86000_write_reg(data, 0x8F, 0x81);
	err |= max86000_write_reg(data, 0x82, 0x04);
	err |= max86000_write_reg(data, 0xFF, 0x00);
	err |= max86000_write_reg(data, MAX86100_INTERRUPT_ENABLE, PPG_RDY_MASK);
	/* Interrupt Clear */
	recv_data = MAX86100_INTERRUPT_STATUS;
	err |= max86000_read_reg(data, &recv_data, 1);
	/* 1000Hz, LED_PW=400us */
	err |= max86000_write_reg(data, MAX86100_SPO2_CONFIGURATION, 0x17);
	err |= max86000_write_reg(data, MAX86100_MODE_CONFIGURATION, MAX86100_MODE_HR);

	if (err != 0)
		goto i2c_err;

	return 0;
i2c_err:
	pr_err("%s - I2c communication has failed\n", __func__);
	return err;

}

int max86100_hrm_enable(struct max86x00_device_data *data)
{
	int err = 0;
	u8 flex_config[2] = {0, };
	data->led = 0;
	data->sample_cnt = 0;
	data->led_sum[0] = 0;
	data->led_sum[1] = 0;
	data->led_sum[2] = 0;
	data->led_sum[3] = 0;
	data->num_samples = 0;
	data->flex_mode = 0;

	flex_config[0] = (IR_LED_CH << MAX86100_S2_OFFSET) | RED_LED_CH;
	flex_config[1] = 0x00;
	if (flex_config[0] & MAX86100_S1_MASK) {
		data->num_samples++;
		data->flex_mode |= (1 << 0);
	}
	if (flex_config[0] & MAX86100_S2_MASK) {
		data->num_samples++;
		data->flex_mode |= (1 << 1);
	}
	if (flex_config[1] & MAX86100_S3_MASK) {
		data->num_samples++;
		data->flex_mode |= (1 << 2);
	}
	if (flex_config[1] & MAX86100_S4_MASK) {
		data->num_samples++;
		data->flex_mode |= (1 << 3);
	}

	pr_info("%s - flexmode : 0x%02x, num_samples : %d\n", __func__,
			data->flex_mode, data->num_samples);


	/* Write LED and SPO2 settings */
	if (data->agc_is_enable)
		err |= data->prox_led_init(data);
	else
		err |= data->hrm_led_init(data);

	err |= max86000_write_reg(data, MAX86100_INTERRUPT_ENABLE, PPG_RDY_MASK);
	err |= max86000_write_reg(data, MAX86100_LED_FLEX_CONTROL_1,
			flex_config[0]);
	err |= max86000_write_reg(data, MAX86100_LED_FLEX_CONTROL_2,
			flex_config[1]);
	err |= max86000_write_reg(data, MAX86100_FIFO_WRITE_POINTER, 0x00);
	err |= max86000_write_reg(data, MAX86100_OVF_COUNTER, 0x00);
	err |= max86000_write_reg(data, MAX86100_FIFO_READ_POINTER, 0x00);
	err |= max86000_write_reg(data, MAX86100_MODE_CONFIGURATION, MODE_FLEX);

	/* Temperature Enable */
	err |= max86000_write_reg(data, MAX86100_TEMP_CONFIG, 0x01);
	if (err != 0) {
		pr_err("%s - error initializing hrm mode!\n", __func__);
		return -EIO;
	}

	if (!atomic_read(&data->irq_enable)) {
		enable_irq(data->irq);
		atomic_set(&data->irq_enable, 1);
	}

	return 0;
}

static int max86000_uv_enable(struct max86x00_device_data *data)
{
	int err = 0;
	data->led = 0;
	data->sample_cnt = 0;

	err |= max86000_write_reg(data, MAX86000_INTERRUPT_ENABLE, UV_RDY_MASK);
	err |= max86000_write_reg(data, MAX86000_MODE_CONFIGURATION, 0x09);
	if (err != 0) {
		pr_err("%s - I2C error\n", __func__);
		return -EIO;
	}

	if (!atomic_read(&data->irq_enable)) {
		enable_irq(data->irq);
		atomic_set(&data->irq_enable, 1);
	}

	return 0;
}

int max86100_uv_enable(struct max86x00_device_data *data)
{
	int err = 0;
	u8 recvData;

	data->sample_cnt = 0;
	err |= max86000_write_reg(data, MAX86100_MODE_CONFIGURATION, MAX86100_RESET_MASK);

	usleep_range(1000, 1100);
	err |= max86000_write_reg(data, MAX86100_INTERRUPT_ENABLE, UV_RDY_MASK);
	err |= max86000_write_reg(data, MAX86100_UV_CONFIGURATION, 0x05);
	err |= max86000_write_reg(data, MAX86100_MODE_CONFIGURATION, MAX86100_MODE_UV);

	/* Interrupt Clear */
	recvData = MAX86100_INTERRUPT_STATUS;
	err |= max86000_read_reg(data, &recvData, 1);

	/* Interrupt2 Clear */
	recvData = MAX86100_INTERRUPT_STATUS_2;
	err |= max86000_read_reg(data, &recvData, 1);
	if (err) {
		pr_err("%s - I2C error\n", __func__);
		return -EIO;
	}


	if (!atomic_read(&data->irq_enable)) {
		enable_irq(data->irq);
		atomic_set(&data->irq_enable, 1);
	}
	return 0;
}


static int max86100_uv_init_fov_correction(struct max86x00_device_data *data)
{
	int err = 0;

	err |= max86000_write_reg(data, 0xFF, 0x54);
	err |= max86000_write_reg(data, 0xFF, 0x4d);
	err |= max86000_write_reg(data, 0x82, 0x04);
	err |= max86000_write_reg(data, 0x8f, 0x01);
	err |= max86000_write_reg(data, 0x8e, 0x40);
	err |= max86000_write_reg(data, 0x90, 0x00);
	err |= max86000_write_reg(data, 0xFF, 0x00);
	err |= max86000_write_reg(data, MAX86100_LED_FLEX_CONTROL_1,
			RED_LED_CH); /* IR/RED SWAPPED */
	err |= max86000_write_reg(data, MAX86100_FIFO_CONFIG,
			(MAX86100_FIFO_ROLLS_ON_MASK | 0x0C)); /* Interrupt 20th(32-20) */
	if (err != 0) {
		pr_err("%s - I2C error\n", __func__);
		return -EIO;
	}

	return err;
}

static int max86100_uv_eol_init_fov_correction(struct max86x00_device_data *data)
{
	int err = 0;
//	err |= max86000_write_reg(data, 0xFF, 0x54);
//	err |= max86000_write_reg(data, 0xFF, 0x4d);
//	err |= max86000_write_reg(data, 0x82, 0x04);
//	err |= max86000_write_reg(data, 0x8f, 0x01);
//	err |= max86000_write_reg(data, 0x8e, 0x40);
//	err |= max86000_write_reg(data, 0x90, 0x00);
//	err |= max86000_write_reg(data, 0xFF, 0x00);
	err |= max86000_write_reg(data, MAX86100_LED1_PA, 0x00);
	err |= max86000_write_reg(data, MAX86100_LED2_PA, data->led_current2);
	err |= max86000_write_reg(data, MAX86100_LED3_PA, 0x00);
	err |= max86000_write_reg(data, MAX86100_LED4_PA, 0x00);
	err |= max86000_write_reg(data, MAX86100_LED_FLEX_CONTROL_1,
			RED_LED_CH); /* IR/RED SWAPED */
	err |= max86000_write_reg(data, MAX86100_FIFO_CONFIG,
			(MAX86100_FIFO_ROLLS_ON_MASK | 0x0C)); /* Interrupt 20th(32-20) */
	if (err != 0) {
		pr_err("%s - I2C error\n", __func__);
		return -EIO;
	}

	return err;
}

static int max86100_uv_enable_gesture(struct max86x00_device_data *data)
{
	int err = 0;

	data->sample_cnt = 0;
	data->num_samples = 1;
	data->sum_gesture_data = 0;

	err |= max86000_write_reg(data, MAX86100_MODE_CONFIGURATION, MAX86100_RESET_MASK);

	usleep_range(1000, 1100);

	if ( data->eol_test_is_enable )
		err |= max86100_uv_eol_init_fov_correction(data);
	else
	err |= max86100_uv_init_fov_correction(data);
	err |= max86000_write_reg(data, MAX86100_SPO2_CONFIGURATION, 0xEF); /* 400 hz PDMUX=1 */

	atomic_set(&data->enhanced_uv_mode, MAX86100_ENHANCED_UV_GESTURE_MODE);

	err |= max86000_write_reg(data, MAX86100_MODE_CONFIGURATION,
			(MODE_GEST | MODE_FLEX));

	/* pr_info("%s: %d, uv_mode: %d\n", __func__, __LINE__, atomic_read(&data->enhanced_uv_mode)); */
	err |= max86000_write_reg(data, MAX86100_INTERRUPT_ENABLE, PPG_RDY_MASK);
	if (err != 0) {
		pr_err("%s - I2C error\n", __func__);
		return -EIO;
	}

	return err;
}

static int max86100_uv_enable_hr(struct max86x00_device_data *data)
{
	int err = 0;
	u8 recvData;

	data->num_samples = 20;

	err |= max86000_write_reg(data, MAX86100_MODE_CONFIGURATION, MAX86100_RESET_MASK);

	usleep_range(1000, 1100);
	/* Temperature Enable */
	err |= max86000_write_reg(data, MAX86100_TEMP_CONFIG, 0x01);
	if ( data->eol_test_is_enable )
		err |= max86100_uv_eol_init_fov_correction(data);
	else
	err |= max86100_uv_init_fov_correction(data);
	err |= max86000_write_reg(data, MAX86100_SPO2_CONFIGURATION, 0xEF); /* 400Hz */
	err |= max86000_write_reg(data, MAX86100_INTERRUPT_ENABLE, A_FULL_MASK);
	err |= max86000_write_reg(data, MAX86100_MODE_CONFIGURATION, MODE_FLEX);

	/* Interrupt Clear */
	recvData = MAX86100_INTERRUPT_STATUS;
	err |= max86000_read_reg(data, &recvData, 1);

	/* Interrupt2 Clear */
	recvData = MAX86100_INTERRUPT_STATUS_2;
	err |= max86000_read_reg(data, &recvData, 1);
	if (err) {
		pr_err("%s - I2C error\n", __func__);
		return -EIO;
	}

	atomic_set(&data->enhanced_uv_mode, MAX86100_ENHANCED_UV_HR_MODE);

	return err;
}

static int max86100_uv_enable_uv(struct max86x00_device_data *data)
{
	int err = 0;
	u8 recvData;

	data->sample_cnt = 0;
	err |= max86000_write_reg(data, MAX86100_MODE_CONFIGURATION, MAX86100_RESET_MASK);

	usleep_range(1000, 1100);
	err |= max86000_write_reg(data, MAX86100_INTERRUPT_ENABLE, 0x08);
	err |= max86000_write_reg(data, MAX86100_UV_CONFIGURATION, 0x05);
	err |= max86000_write_reg(data, MAX86100_MODE_CONFIGURATION, 0x01);

	/* Interrupt Clear */
	recvData = MAX86100_INTERRUPT_STATUS;
	err |= max86000_read_reg(data, &recvData, 1);

	/* Interrupt2 Clear */
	recvData = MAX86100_INTERRUPT_STATUS_2;
	err |= max86000_read_reg(data, &recvData, 1);
	if (err) {
		pr_err("%s - I2C error\n", __func__);
		return -EIO;
	}

	atomic_set(&data->enhanced_uv_mode, MAX86100_ENHANCED_UV_MODE);

	return err;
}

static int max86100_uvfov_enable(struct max86x00_device_data *data)
{
	int err;
	err = max86100_uv_enable_gesture(data);

	if (!atomic_read(&data->irq_enable)) {
		enable_irq(data->irq);
		atomic_set(&data->irq_enable, 1);
	}
	return err;
}

static int max86100_uv_eol_enable_vb(struct max86x00_device_data *data)
{
	int err = 0;
	u8 recvData;

	data->sample_cnt = 0;
	data->num_samples = 1;

	err |= max86000_write_reg(data, MAX86100_MODE_CONFIGURATION, MAX86100_RESET_MASK);
	usleep_range(1000, 1100);

	err |= max86100_uv_eol_init_fov_correction(data);

	err |= max86000_write_reg(data, MAX86100_SPO2_CONFIGURATION, 0xE7); /* 100hz */
	err |= max86000_write_reg(data, MAX86100_INTERRUPT_ENABLE, PPG_RDY_MASK);
	err |= max86000_write_reg(data, MAX86100_MODE_CONFIGURATION,
			(MODE_GEST | MODE_FLEX));

	/* Interrupt Clear */
	recvData = MAX86100_INTERRUPT_STATUS;
	err |= max86000_read_reg(data, &recvData, 1);

	/* Interrupt2 Clear */
	recvData = MAX86100_INTERRUPT_STATUS_2;
	err |= max86000_read_reg(data, &recvData, 1);
	if (err) {
		pr_err("%s - I2C error\n", __func__);
		return -EIO;
	}

	atomic_set(&data->enhanced_uv_mode, MAX86100_ENHANCED_UV_EOL_VB_MODE);

	return err;
}

static int max86100_uv_eol_enable_sum(struct max86x00_device_data *data)
{
	int err = 0;
	u8 recvData;

	data->sample_cnt = 0;
	data->num_samples = 1;

	err |= max86000_write_reg(data, MAX86100_MODE_CONFIGURATION, MAX86100_RESET_MASK);
	usleep_range(1000, 1100);

	err |= max86100_uv_eol_init_fov_correction(data);

	err |= max86000_write_reg(data, MAX86100_SPO2_CONFIGURATION, 0x6F); /* 400 hz */
	err |= max86000_write_reg(data, MAX86100_INTERRUPT_ENABLE, PPG_RDY_MASK);
	err |= max86000_write_reg(data, MAX86100_MODE_CONFIGURATION,
			(MODE_GEST | MODE_FLEX));

	/* Interrupt Clear */
	recvData = MAX86100_INTERRUPT_STATUS;
	err |= max86000_read_reg(data, &recvData, 1);

	/* Interrupt2 Clear */
	recvData = MAX86100_INTERRUPT_STATUS_2;
	err |= max86000_read_reg(data, &recvData, 1);
	if (err) {
		pr_err("%s - I2C error\n", __func__);
		return -EIO;
	}

	atomic_set(&data->enhanced_uv_mode, MAX86100_ENHANCED_UV_EOL_SUM_MODE);

	return err;
}

static int max86100_uv_eol_enable_hr(struct max86x00_device_data *data)
{
	int err = 0;
	u8 recvData;

	data->num_samples = 20;

	err |= max86000_write_reg(data, MAX86100_MODE_CONFIGURATION, MAX86100_RESET_MASK);
	usleep_range(1000, 1100);

	err |= max86100_uv_eol_init_fov_correction(data);

	err |= max86000_write_reg(data, MAX86100_SPO2_CONFIGURATION, 0xEF); /* 400Hz */
	err |= max86000_write_reg(data, MAX86100_INTERRUPT_ENABLE, A_FULL_MASK);
	err |= max86000_write_reg(data, MAX86100_MODE_CONFIGURATION, MODE_FLEX);
	/* err |= max86000_write_reg(data, MAX86100_MODE_CONFIGURATION, 0x02); */

	/* Interrupt Clear */
	recvData = MAX86100_INTERRUPT_STATUS;
	err |= max86000_read_reg(data, &recvData, 1);

	/* Interrupt2 Clear */
	recvData = MAX86100_INTERRUPT_STATUS_2;
	err |= max86000_read_reg(data, &recvData, 1);
	if (err) {
		pr_err("%s - I2C error\n", __func__);
		return -EIO;
	}

	atomic_set(&data->enhanced_uv_mode, MAX86100_ENHANCED_UV_EOL_HR_MODE);

	return err;
}

static int max86000_disable(struct max86x00_device_data *data)
{
	u8 err = 0;

	if (atomic_read(&data->irq_enable)) {
		disable_irq(data->irq);
		atomic_set(&data->irq_enable, 0);
	}

	err |= max86000_write_reg(data, MAX86000_MODE_CONFIGURATION, MAX86100_RESET_MASK);
	err |= max86000_write_reg(data, MAX86000_MODE_CONFIGURATION, MAX86100_SHDN_MASK);
	if (err != 0) {
		pr_err("%s - I2C Error\n", __func__);
		return -EIO;
	}

	switch (data->part_type) {
	case PART_TYPE_MAX86000A:
		data->default_current =	MAX86000A_DEFAULT_CURRENT;
		break;
	case PART_TYPE_MAX86000B:
		data->default_current = MAX86000A_DEFAULT_CURRENT;
		break;
	case PART_TYPE_MAX86000C:
		data->default_current = MAX86000C_DEFAULT_CURRENT;
		break;
	default:
		pr_err("%s - unsupported part type %d\n", __func__, data->part_type);
	}

	return 0;
}

int max86100_disable(struct max86x00_device_data *data)
{
	u8 err = 0;

	if (atomic_read(&data->irq_enable)) {
		disable_irq(data->irq);
		atomic_set(&data->irq_enable, 0);
	}

	err |= max86000_write_reg(data, MAX86100_MODE_CONFIGURATION, MAX86100_RESET_MASK);
	err |= max86000_write_reg(data, MAX86100_MODE_CONFIGURATION, MAX86100_SHDN_MASK);
	if (err != 0) {
		pr_err("%s - I2C Error\n",
			__func__);
		return -EIO;
	}
	return 0;
}

static int max86000_read_temperature(struct max86x00_device_data *data)
{
	u8 recvData[2] = { 0x00, };
	int err;

	recvData[0] = MAX86000_TEMP_INTEGER;

	err = max86000_read_reg(data, recvData, 2);
	if (err != 0) {
		pr_err("%s max86000_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData[0]);
		return -EIO;
	}
	data->hrm_temp = ((char)recvData[0]) * 16 + recvData[1];

	pr_info("%s - %d(%x, %x)\n", __func__,
		data->hrm_temp, recvData[0], recvData[1]);

	return 0;
}

static int max86100_read_temperature(struct max86x00_device_data *data)
{
	u8 recvData[2] = { 0x00, };
	int err = 0;

	recvData[0] = MAX86100_TEMP_INTEGER;

	err |= max86000_read_reg(data, recvData, 1);

	recvData[1] = MAX86100_TEMP_FRACTION;
	err |= max86000_read_reg(data, &recvData[1], 1);

	data->hrm_temp = ((char)recvData[0]) * 16 + recvData[1];

	err |= max86000_write_reg(data, MAX86100_TEMP_CONFIG, 0x01);
	if (err != 0) {
		pr_err("%s - I2C Error\n", __func__);
		return -EIO;
	}

	/* pr_info("%s - %d(%x, %x)\n", __func__,
		data->hrm_temp, recvData[0], recvData[1]); */

	return 0;
}

static int max86000_eol_test_control(struct max86x00_device_data *data)
{
	int err = 0;
	u8 led_current = 0;

	if (data->sample_cnt < data->hr_range2)	{
		data->hr_range = 1;
	} else if (data->sample_cnt < (data->hr_range2 + 297)) {
		/* Fake pulse */
		if (data->sample_cnt % 8 < 4) {
			data->test_current_ir++;
			data->test_current_red++;
		} else {
			data->test_current_ir--;
			data->test_current_red--;
		}

		led_current = (data->test_current_red << 4)
			| data->test_current_ir;
		err |= max86000_write_reg(data, MAX86000_LED_CONFIGURATION,
				led_current);

		data->hr_range = 2;
	} else if (data->sample_cnt == (data->hr_range2 + 297)) {
		/* Measure */
		err |= max86000_write_reg(data, MAX86000_LED_CONFIGURATION,
				data->led_current);
		/* 400Hz setting */
		err |= max86000_write_reg(data,
				MAX86000_SPO2_CONFIGURATION, 0x51);
	} else if (data->sample_cnt < ((data->hr_range2 + 297) + 400 * 10)) {
		data->hr_range = 3;
	} else if (data->sample_cnt == ((data->hr_range2 + 297) + 400 * 10)) {
		err |= max86000_write_reg(data,
				MAX86000_LED_CONFIGURATION, data->default_current);
#if MAX86000_SAMPLE_RATE == 1
		err |= max86000_write_reg(data,
				MAX86000_SPO2_CONFIGURATION, 0x47);
#endif

#if MAX86000_SAMPLE_RATE == 2
		err |= max86000_write_reg(data,
				MAX86000_SPO2_CONFIGURATION, 0x4E);
#endif

#if MAX86000_SAMPLE_RATE == 4
		err |= max86000_write_reg(data,
				MAX86000_SPO2_CONFIGURATION, 0x51);
#endif
	}

	if (err != 0) {
		pr_err("%s - I2C Error\n", __func__);
		return -EIO;
	}
	

	data->sample_cnt++;
	return 0;
}

int max86000_hrm_read_data(struct max86x00_device_data *device, u16 *data)
{
	u8 err;
	u8 recvData[4] = { 0x00, };
	int i;
	int ret = 0;

	if (device->sample_cnt == MAX86000_COUNT_MAX)
		device->sample_cnt = 0;

	recvData[0] = MAX86000_FIFO_DATA;
	err = max86000_read_reg(device, recvData, 4);
	if (err) {
		pr_err("%s max86000_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData[0]);
		return -EIO;
	}

	for (i = 0; i < 2; i++)	{
		data[i] = ((((u16)recvData[i*2]) << 8) & 0xff00)
			| (((u16)recvData[i*2+1]) & 0x00ff);
	}

	data[2] = device->led;

	if ((device->sample_cnt % 1000) == 1)
		pr_info("%s - %u, %u, %u, %u\n", __func__,
			data[0], data[1], data[2], data[3]);

	if (device->sample_cnt == 20 && device->led == 0) {
		err = max86000_read_temperature(device);
		if (err < 0) {
			pr_err("%s - max86000_read_temperature err : %d\n",
				__func__, err);
			return -EIO;
		}
	}

	if (device->agc_is_enable) {
		ret = max86x00_prox_check(device, data[0]);
		if (ret) {
			pr_err("Proximity check error. - %s:%d\n",
			    __func__, __LINE__);
			return ret;
		}

		if (device->prox_detect) {
			ret = max86x00_hrm_agc(device, data[0], MAX86X00_LED1);
			ret |= max86x00_hrm_agc(device, data[1], MAX86X00_LED2);
			if (ret) {
				pr_err("Auto gain control error. - %s:%d\n",
					__func__, __LINE__);
				return ret;
			}
		}

	}

	if (device->eol_test_is_enable) {
		err = max86000_eol_test_control(device);
		if (err < 0) {
			pr_err("%s - max86000_eol_test_control err : %d\n",
					__func__, err);
			return -EIO;
		}
	} else {
		device->ir_sum += data[0];
		device->r_sum += data[1];
		if ((device->sample_cnt % MAX86000_SAMPLE_RATE) == MAX86000_SAMPLE_RATE - 1) {
			data[0] = device->ir_sum / MAX86000_SAMPLE_RATE;
			data[1] = device->r_sum / MAX86000_SAMPLE_RATE;
			device->ir_sum = 0;
			device->r_sum = 0;
			ret = 0;
		} else
			ret = 1;

		if (device->sample_cnt++ > 100 && device->led == 0)
			device->led = 1;
	}

	return ret;
}

int max86100_hrm_read_data(struct max86x00_device_data *device, int *data)
{
	u8 err;
	u8 recvData[MAX_LED_NUM * NUM_BYTES_PER_SAMPLE] = { 0x00, };
	int i, j = 0;
	int ret = 0;

	if (device->sample_cnt == MAX86100_COUNT_MAX)
		device->sample_cnt = 0;

	recvData[0] = MAX86100_FIFO_DATA;
	err = max86000_read_reg(device, recvData,
		device->num_samples * NUM_BYTES_PER_SAMPLE);
	if (err) {
		pr_err("%s max86000_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData[0]);
		return -EIO;
	}

	for (i = 0; i < MAX_LED_NUM; i++)	{
		if (device->flex_mode | (1 << i)) {
			data[i] =  recvData[j++] << 16 & 0x30000;
			data[i] += recvData[j++] << 8;
			data[i] += recvData[j++] << 0;
		} else
			data[i] = 0;
	}

	data[4] = device->led;

	if ((device->sample_cnt % 1000) == 1)
		pr_info("%s - %u, %u, %u, %u\n", __func__,
			data[0], data[1], data[2], data[3]);

	if (device->sample_cnt == 20 && device->led == 0) {
		err = max86100_read_temperature(device);
		if (err < 0) {
			pr_err("%s - max86000_read_temperature err : %d\n",
				__func__, err);
			return -EIO;
		}
	}

	for (i = 0; i < MAX_LED_NUM; i++)
		device->led_sum[i] += data[i];
	if ((device->sample_cnt % MAX86100_SAMPLE_RATE) == MAX86100_SAMPLE_RATE - 1) {
		for (i = 0; i < MAX_LED_NUM; i++) {
			data[i] = device->led_sum[i] / MAX86100_SAMPLE_RATE;
			device->led_sum[i] = 0;
		}
		ret = 0;
	} else
		ret = 1;

	if (device->sample_cnt++ > 100 && device->led == 0)
		device->led = 1;

	return ret;
}

int max86000_uv_read_data(struct max86x00_device_data *device, int *data)
{
	u8 err;
	u8 recvData[7] = { 0x00, };
	int ret = 0;

	if (device->sample_cnt == MAX86000_COUNT_MAX)
		device->sample_cnt = 0;

	recvData[0] = MAX86000_UV_DATA;
	err = max86000_read_reg(device, recvData, 7);
	if (err) {
		pr_err("%s max86000_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData[0]);
		return -EIO;
	}

	data[0] = ((int)recvData[0] << 8) | (int)recvData[1];
	data[1] = ((int)recvData[2] << 16) | ((int)recvData[3] << 8) | (int)recvData[4];
	data[2] = ((int)recvData[5] << 8) | (int)recvData[6];

	pr_info("%s - %u,%u,%u\n", __func__, data[0], data[1], data[2]);

	if (device->sample_cnt == 1 && device->led == 0) {
		err = max86000_read_temperature(device);
		if (err < 0) {
			pr_err("%s - max86000_read_temperature err : %d\n",
				__func__, err);
			return -EIO;
		}
	}

	if (device->sample_cnt++ > 0 && device->led == 0)
		device->led = 1;

	return ret;
}

int max86100_uv_read_data(struct max86x00_device_data *device, int *data)
{
	u8 err;
	u8 recvData[7] = { 0x00, };
	int ret = 0;

	if (device->sample_cnt == MAX86100_COUNT_MAX)
		device->sample_cnt = 0;

	recvData[0] = MAX86100_UV_DATA_HI;
	err = max86000_read_reg(device, recvData, 7);
	if (err) {
		pr_err("%s max86000_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData[0]);
		return -EIO;
	}

	data[0] = ((int)recvData[0] << 8) | (int)recvData[1];
	data[1] = ((int)recvData[2] << 16) | ((int)recvData[3] << 8) | (int)recvData[4];
	data[2] = ((int)recvData[5] << 8) | (int)recvData[6];

	err = max86100_read_temperature(device);
	if (err < 0) {
		pr_err("%s - max86000_read_temperature err : %d\n",
			__func__, err);
		return -EIO;
	}

	/* pr_info("%s - %u,%u,%u,%u\n", __func__, data[0], data[1], data[2], device->hrm_temp); */
	return ret;
}

int max86100_uv_read_data_gesture(struct max86x00_device_data *device, int *data)
{
	u8 err;
	int ret = MAX86100_ENHANCED_UV_NONE_MODE;
	u8 status;
	u8 recvData[NUM_BYTES_PER_SAMPLE] = { 0x00, };
	int reg_data = 0;

	status = MAX86100_INTERRUPT_STATUS;
	err = max86000_read_reg(device, &status, 1);
	if (err < 0) {
		pr_err("%s: read status err: %d\n", __func__, err);
		return -EIO;
	}

	if (status & PPG_RDY_MASK) {
		recvData[0] = MAX86100_FIFO_DATA;
		err = max86000_read_reg(device, recvData, NUM_BYTES_PER_SAMPLE);
		if (err) {
			pr_err("%s max86000_read_reg err:%d, address:0x%02x\n",
				__func__, err, recvData[0]);
			return -EIO;
		}

		device->sample_cnt++;

		if (device->sample_cnt > 0 && device->sample_cnt <= 4) {
			reg_data =  recvData[0] << 16 & 0x30000;
			reg_data += recvData[1] << 8;
			reg_data += recvData[2] << 0;
			if (((recvData[0] >> 6) == 1) || ((recvData[0] >> 6) == 3)) {
				device->sum_gesture_data += reg_data;
				/* pr_info("gest:%d,%d,%d\n",
						reg_data, recvData[0] >> 6, device->sum_gesture_data); */
			}
		}

		if (device->sample_cnt >= 4) {
			/* Mode change */
			err = max86100_uv_enable_hr(device);
			if (err < 0) {
				pr_err("%s - max86100_uv_enable_hr err : %d\n",
					__func__, err);
				return -EIO;
			}

			*data = device->sum_gesture_data;
			/* pr_info("%s - %u\n", __func__, *data); */
			ret = MAX86100_ENHANCED_UV_GESTURE_MODE;
		}
	}

	return ret;
}

static int max86100_uv_read_data_hr(struct max86x00_device_data *device,
	int *data)
{
	u8 err;
	int ret = MAX86100_ENHANCED_UV_NONE_MODE;
	u8 status;
	u8 recvData[20 * NUM_BYTES_PER_SAMPLE] = { 0x00, };
	int reg_data = 0;
	int sum_data = 0;
	int i;

	status = MAX86100_INTERRUPT_STATUS;
	err = max86000_read_reg(device, &status, 1);
	if (err < 0) {
		pr_err("%s: read status err: %d\n", __func__, err);
		return -EIO;
	}

	if (status & A_FULL_MASK) {
		recvData[0] = MAX86100_FIFO_DATA;
		err = max86000_read_reg(device, recvData,
			device->num_samples * NUM_BYTES_PER_SAMPLE);
		if (err) {
			pr_err("%s max86000_read_reg err:%d, address:0x%02x\n",
				__func__, err, recvData[0]);
			return -EIO;
		}

		for (i = 16; i < 20; i++) {
			reg_data =  recvData[i*NUM_BYTES_PER_SAMPLE+0] << 16 & 0x30000;
			reg_data += recvData[i*NUM_BYTES_PER_SAMPLE+1] << 8;
			reg_data += recvData[i*NUM_BYTES_PER_SAMPLE+2] << 0;
			/* pr_info("hr:%d,%d\n", reg_data, recvData[0] >> 6); */
			sum_data += reg_data;
		}

		/* Read Temperature */
		err = max86100_read_temperature(device);
		if (err < 0) {
			pr_err("%s - max86000_read_temperature err : %d\n",
				__func__, err);
			return -EIO;
		}

		/* Mode change */
		err = max86100_uv_enable_uv(device);
		if (err < 0) {
			pr_err("%s - max86100_uv_enable_uv err : %d\n",
				__func__, err);
			return -EIO;
		}

		*data = sum_data;
		/* pr_info("%s - %u\n", __func__, *data); */
		ret = MAX86100_ENHANCED_UV_HR_MODE;
	}

	return ret;
}

static int max86100_uv_read_data_uv(struct max86x00_device_data *device,
	int *data)
{
	u8 err;
	u8 recvData[5] = { 0x00, };
	int ret = MAX86100_ENHANCED_UV_NONE_MODE;
	u8 status;
	status = MAX86100_INTERRUPT_STATUS;
	err = max86000_read_reg(device, &status, 1);
	if (err < 0) {
		pr_err("%s: read status err: %d\n", __func__, err);
		return -EIO;
	}
	if (status & UV_RDY_MASK) {
		recvData[0] = MAX86100_UV_DATA_HI;
		err = max86000_read_reg(device, recvData, 5);
		if (err) {
			pr_err("%s max86000_read_reg err:%d, address:0x%02x\n",
				__func__, err, recvData[0]);
			return -EIO;
		}

		data[0] = ((int)recvData[0] << 8) | (int)recvData[1];
		data[1] = ((int)recvData[2] << 16)
					| ((int)recvData[3] << 8) | (int)recvData[4];

		/* Mode change */
		err = max86100_uv_enable_gesture(device);
		if (err < 0) {
			pr_err("%s - max86100_uv_enable_gesture err : %d\n",
				__func__, err);
			return -EIO;
		}
		/* pr_info("%s - %u, %u\n", __func__, data[0], data[1]); */
		ret = MAX86100_ENHANCED_UV_MODE;
	}
	return ret;
}

static int max86100_uv_eol_read_data_vb(struct max86x00_device_data *device,
																		int *data)
{
	u8 err;
	int ret = MAX86100_ENHANCED_UV_NONE_MODE;
	u8 status;
	u8 recvData[MAX_LED_NUM * NUM_BYTES_PER_SAMPLE] = { 0x00, };
	int reg_data;

	status = MAX86100_INTERRUPT_STATUS;
	err = max86000_read_reg(device, &status, 1);
	if (err < 0) {
		pr_err("%s: read status err: %d\n", __func__, err);
		return -EIO;
	}

	if (status & PPG_RDY_MASK) {
		recvData[0] = MAX86100_FIFO_DATA;
		err = max86000_read_reg(device, recvData,
			device->num_samples * NUM_BYTES_PER_SAMPLE);
		if (err) {
			pr_err("%s max86000_read_reg err:%d, address:0x%02x\n",
				__func__, err, recvData[0]);
			return -EIO;
		}

		device->sample_cnt++;

		if (device->sample_cnt > 16 && device->sample_cnt <= 20) {
			reg_data =  recvData[0] << 16;
			reg_data += recvData[1] << 8;
			reg_data += recvData[2] << 0;
			pr_info("vb:%d,%d\n", (reg_data & 0x3ffff), reg_data >> 22);
			*data = reg_data;
			ret = MAX86100_ENHANCED_UV_EOL_VB_MODE;
		}

		if (device->sample_cnt >= 20) {
			/* Mode change */
			err = max86100_uv_eol_enable_sum(device);
			if (err < 0) {
				pr_err("%s - max86100_uv_eol_enable_sum err : %d\n",
					__func__, err);
				return -EIO;
			}
		}
	}

	return ret;
}

static int max86100_uv_eol_read_data_sum(struct max86x00_device_data *device,
																		int *data)
{
	u8 err;
	int ret = MAX86100_ENHANCED_UV_NONE_MODE;
	u8 status;
	u8 recvData[NUM_BYTES_PER_SAMPLE] = { 0x00, };
	int reg_data = 0;

	status = MAX86100_INTERRUPT_STATUS;
	err = max86000_read_reg(device, &status, 1);
	if (err < 0) {
		pr_err("%s: read status err: %d\n", __func__, err);
		return -EIO;
	}

	if (status & PPG_RDY_MASK) {
		recvData[0] = MAX86100_FIFO_DATA;
		err = max86000_read_reg(device, recvData, NUM_BYTES_PER_SAMPLE);
		if (err) {
			pr_err("%s max86000_read_reg err:%d, address:0x%02x\n",
				__func__, err, recvData[0]);
			return -EIO;
		}

		device->sample_cnt++;

		reg_data =  recvData[0] << 16 & 0x30000;
		reg_data += recvData[1] << 8;
		reg_data += recvData[2] << 0;
		pr_info("sum:%d,%d\n", reg_data, recvData[0] >> 6);

		if (device->sample_cnt >= 4) {
			/* Mode change */
			err = max86100_uv_eol_enable_hr(device);
			if (err < 0) {
				pr_err("%s - max86100_uv_eol_enable_hr err : %d\n",
					__func__, err);
				return -EIO;
			}

			*data = reg_data;
			pr_info("%s - %u\n", __func__, *data);
			ret = MAX86100_ENHANCED_UV_EOL_SUM_MODE;
		}
	}

	return ret;
}

static int max86100_uv_eol_read_data_hr(
						struct max86x00_device_data *device, int *data)
{
	u8 err;
	int ret = MAX86100_ENHANCED_UV_NONE_MODE;
	u8 status;
	u8 recvData[20 * NUM_BYTES_PER_SAMPLE] = { 0x00, };
	int reg_data = 0;
	int sum_data = 0;
	int i;

	status = MAX86100_INTERRUPT_STATUS;
	err = max86000_read_reg(device, &status, 1);
	if (err < 0) {
		pr_err("%s: read status err: %d\n", __func__, err);
		return -EIO;
	}

	if (status & A_FULL_MASK) {
		recvData[0] = MAX86100_FIFO_DATA;
		err = max86000_read_reg(device, recvData,
			device->num_samples * NUM_BYTES_PER_SAMPLE);
		if (err) {
			pr_err("%s max86000_read_reg err:%d, address:0x%02x\n",
				__func__, err, recvData[0]);
			return -EIO;
		}

		for (i = 16; i < 20; i++) {
			reg_data =  recvData[i*NUM_BYTES_PER_SAMPLE+0] << 16 & 0x30000;
			reg_data += recvData[i*NUM_BYTES_PER_SAMPLE+1] << 8;
			reg_data += recvData[i*NUM_BYTES_PER_SAMPLE+2] << 0;
			pr_info("eh:%d,%d\n", reg_data, recvData[0] >> 6);
			sum_data += reg_data;
		}

		/* Mode change */
		err = max86100_uv_eol_enable_vb(device);
		if (err < 0) {
			pr_err("%s - max86100_uv_eol_enable_vb err : %d\n",
				__func__, err);
			return -EIO;
		}

		*data = sum_data;
		pr_info("%s - %u\n", __func__, *data);
		ret = MAX86100_ENHANCED_UV_EOL_HR_MODE;
	}

	return ret;
}

int max86100_uvfov_read_data(struct max86x00_device_data *device, int *data)
{
	int err;

	switch (atomic_read(&device->enhanced_uv_mode)) {
	case MAX86100_ENHANCED_UV_GESTURE_MODE:
		err = max86100_uv_read_data_gesture(device, data);
		break;
	case MAX86100_ENHANCED_UV_HR_MODE:
		err = max86100_uv_read_data_hr(device, data);
		break;
	case MAX86100_ENHANCED_UV_MODE:
		err = max86100_uv_read_data_uv(device, data);
		break;
	case MAX86100_ENHANCED_UV_EOL_VB_MODE:
		err = max86100_uv_eol_read_data_vb(device, data);
		break;
	case MAX86100_ENHANCED_UV_EOL_SUM_MODE:
		err = max86100_uv_eol_read_data_sum(device, data);
		break;
	case MAX86100_ENHANCED_UV_EOL_HR_MODE:
		err = max86100_uv_eol_read_data_hr(device, data);
		break;
	default:
		err = -EIO;
	}

	return err;
}

void max86000_hrm_mode_enable(struct max86x00_device_data *data, int onoff)
{
	int err;
	if (onoff) {
		err = max86000_regulator_onoff(data, HRM_LDO_ON);
		if (err < 0)
			pr_err("%s max86000_regulator_on fail err = %d\n",
				__func__, err);
		usleep_range(1000, 1100);
		err = max86000_init_device(data);
		if (err)
			pr_err("%s max86000_init device fail err = %d\n",
				__func__, err);
		err = max86000_hrm_enable(data);
		if (err != 0)
			pr_err("max86000_hrm_enable err : %d\n", err);

		atomic_set(&data->hrm_is_enable, 1);
	} else {
		atomic_set(&data->hrm_is_enable, 0);

		if (atomic_read(&data->regulator_is_enable) == 0)
			return;

		err = max86000_disable(data);
		if (err != 0)
			pr_err("max86000_disable err : %d\n", err);

		usleep_range(2000, 3000);
		err = max86000_regulator_onoff(data, HRM_LDO_OFF);
		if (err < 0)
			pr_err("%s max86000_regulator_off fail err = %d\n",
				__func__, err);
	}
	pr_info("%s - part_type = %u, onoff = %d\n",
		__func__, data->part_type, onoff);
}

static void max86100_hrm_mode_enable(struct max86x00_device_data *data, int onoff)
{
	int err;
	if (onoff) {
		err = max86000_regulator_onoff(data, HRM_LDO_ON);
		if (err < 0)
			pr_err("%s max86000_regulator_on fail err = %d\n",
				__func__, err);
		usleep_range(1000, 1100);
		err = max86100_init_device(data);
		if (err)
			pr_err("%s max86100_init device fail err = %d\n",
				__func__, err);

		if (onoff == MAX86X100_PPG_MODE) {
			err = max86100_hrm_enable(data);
			if (err != 0)
				pr_err("max86100_hrm_enable err : %d\n", err);
			atomic_set(&data->hrm_is_enable, 1);
		} else if (onoff == MAX86X100_ACFD_MODE) {
			err = max86100_acfd_enable(data);
			if (err != 0)
				pr_err("max86100_hrm_enable err : %d\n", err);
			atomic_set(&data->hrm_is_enable, 2);
		} else
			pr_err("Invalid Onoff  value: %d\n", onoff);
	} else {
		atomic_set(&data->hrm_is_enable, 0);

		if (atomic_read(&data->regulator_is_enable) == 0)
			return;

		err = max86100_disable(data);
		if (err != 0)
			pr_err("max86000_disable err : %d\n", err);

		usleep_range(2000, 3000);
		err = max86000_regulator_onoff(data, HRM_LDO_OFF);
		if (err < 0)
			pr_err("%s max86000_regulator_off fail err = %d\n",
				__func__, err);
	}
	pr_info("%s - part_type = %u, onoff = %d\n",
		__func__, data->part_type, onoff);
}

void max86000_uv_mode_enable(struct max86x00_device_data *data, int onoff)
{
	int err;
	if (onoff) {
		if (atomic_read(&data->uv_is_enable) == 1)
				return;
		err = max86000_regulator_onoff(data, HRM_LDO_ON);
		if (err < 0)
			pr_err("%s max86000_regulator_on fail err = %d\n",
				__func__, err);
		usleep_range(1000, 1100);
		err = max86000_init_device(data);
		if (err)
			pr_err("%s max86000_init device fail err = %d\n",
				__func__, err);
		err = max86000_uv_enable(data);
		if (err != 0)
			pr_err("max86000_uv_enable err : %d\n", err);

		atomic_set(&data->uv_is_enable, 1);
	} else {
		if (atomic_read(&data->uv_is_enable) == 0)
				return;
		atomic_set(&data->uv_is_enable, 0);

		if (atomic_read(&data->regulator_is_enable) == 0)
			return;

		err = max86000_disable(data);
		if (err != 0)
			pr_err("max86000_disable err : %d\n", err);
		usleep_range(2000, 3000);
		err = max86000_regulator_onoff(data, HRM_LDO_OFF);
		if (err < 0)
			pr_err("%s max86000_regulator_off fail err = %d\n",
				__func__, err);
	}
	pr_info("%s - part_type = %u, onoff = %d\n",
		__func__, data->part_type, onoff);
}

void max86100_uv_mode_enable(struct max86x00_device_data *data, int onoff)
{
	int err;
	if (onoff) {
		if (atomic_read(&data->uv_is_enable) == onoff)
			return;

		if (onoff == 1) {

			if (atomic_read(&data->uv_is_enable) == 1)
				return;

			if (atomic_read(&data->uv_is_enable) == 2) {
				err = max86100_disable(data);
				if (err != 0)
					pr_err("max86000_disable err : %d\n", err);
				atomic_set(&data->uv_is_enable, 0);

			} else {
				err = max86000_regulator_onoff(data, HRM_LDO_ON);
				if (err < 0)
					pr_err("%s max86000_regulator_on fail err = %d\n",
						__func__, err);
				usleep_range(1000, 1100);
			}

			err = max86100_init_device(data);
			if (err)
				pr_err("%s max86000_init device fail err = %d\n",
					__func__, err);

			atomic_set(&data->uv_is_enable, 1);
			err = max86100_uvfov_enable(data);
			if (err != 0)
				pr_err("max86000_uv_enable err : %d\n", err);

		} else if (onoff == 2) {
			if (atomic_read(&data->uv_is_enable) == 2)
				return;

			if (atomic_read(&data->uv_is_enable) == 1) {
				err = max86100_disable(data);
				if (err != 0)
					pr_err("max86000_disable err : %d\n", err);
				atomic_set(&data->uv_is_enable, 0);

			} else {
				err = max86000_regulator_onoff(data, HRM_LDO_ON);
				if (err < 0)
					pr_err("%s max86000_regulator_on fail err = %d\n",
						__func__, err);
				usleep_range(1000, 1100);
			}

			err = max86100_init_device(data);
			if (err)
				pr_err("%s max86000_init device fail err = %d\n",
					__func__, err);

			atomic_set(&data->uv_is_enable, 2);
			err = max86100_uv_enable(data);
			if (err < 0)
				pr_err("%s - UV enable is failed err = %d\n",
					__func__, err);
		}
	} else {
		if (atomic_read(&data->uv_is_enable) == 0)
			return;

		if (atomic_read(&data->uv_is_enable)) {
			atomic_set(&data->uv_is_enable, 0);

			if (atomic_read(&data->regulator_is_enable) == 0)
				return;

			err = max86100_disable(data);
			if (err != 0)
				pr_err("max86000_disable err : %d\n", err);

			usleep_range(10000, 12000);
			err = max86000_regulator_onoff(data, HRM_LDO_OFF);
			if (err < 0)
				pr_err("%s max86000_regulator_off fail err = %d\n",
					__func__, err);
		}
	}
	pr_info("%s - part_type = %u, onoff = %d\n",
		__func__, data->part_type, onoff);
}


/* hrm sysfs */
static ssize_t max86000_hrm_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
		(atomic_read(&data->hrm_is_enable) == 1 ? 1 : 0));
}

static ssize_t max86000_hrm_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);
	int new_value;

	if (sysfs_streq(buf, "1"))
		new_value = 1;
	else if (sysfs_streq(buf, "0"))
		new_value = 0;
	else {
		pr_err("%s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	pr_info("Enable : %d - %s\n", new_value, __func__);

	mutex_lock(&data->activelock);
	if (data->part_type < PART_TYPE_MAX86100A) {
		if (atomic_read(&data->uv_is_enable))
			max86000_uv_mode_enable(data, 0);
		if (new_value && !atomic_read(&data->hrm_is_enable))
			max86000_hrm_mode_enable(data, 1);
		else if (!new_value && atomic_read(&data->hrm_is_enable))
			max86000_hrm_mode_enable(data, 0);
	} else {
		if (atomic_read(&data->uv_is_enable))
			max86100_uv_mode_enable(data, 0);

		if (new_value && (atomic_read(&data->hrm_is_enable) == 0)) {
			max86100_hrm_mode_enable(data, 1);
		} else if (new_value && atomic_read(&data->hrm_is_enable) == MAX86X100_ACFD_MODE) {
			max86100_hrm_mode_enable(data, 0);
			max86100_hrm_mode_enable(data, 1);
		} else if (!new_value && atomic_read(&data->hrm_is_enable) == MAX86X100_PPG_MODE)
			max86100_hrm_mode_enable(data, 0);
		else if (!new_value && atomic_read(&data->hrm_is_enable) == MAX86X100_ACFD_MODE) {
			pr_info("The mode that is tried to disable is not active."
					"It's already disabled. %s:%d\n", __func__, __LINE__);
		}
	}
	mutex_unlock(&data->activelock);
	return count;
}

static ssize_t max86000_hrm_poll_delay_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%lld\n", 10000000LL);
}

static ssize_t max86000_hrm_poll_delay_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	pr_info("%s - max86000 hrm sensor delay was fixed as 10ms\n", __func__);
	return size;
}

static struct device_attribute dev_attr_hrm_enable =
	__ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
	max86000_hrm_enable_show, max86000_hrm_enable_store);

static struct device_attribute dev_attr_hrm_poll_delay =
	__ATTR(poll_delay, S_IRUGO|S_IWUSR|S_IWGRP,
	max86000_hrm_poll_delay_show, max86000_hrm_poll_delay_store);

static struct attribute *hrm_sysfs_attrs[] = {
	&dev_attr_hrm_enable.attr,
	&dev_attr_hrm_poll_delay.attr,
	NULL
};

static struct attribute_group hrm_attribute_group = {
	.attrs = hrm_sysfs_attrs,
};

static ssize_t max86100_acfd_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
		(atomic_read(&data->hrm_is_enable) == 2 ? 1 : 0));
}

static ssize_t max86100_acfd_poll_delay_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%lld\n", 2500000LL);
}

static ssize_t max86100_acfd_poll_delay_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	pr_info("%s - max86000 ACFD sensor delay was fixed as 2.5ms\n", __func__);
	return size;
}

static ssize_t max86100_acfd_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);
	int new_value;

	if (sysfs_streq(buf, "1"))
		new_value = 2;
	else if (sysfs_streq(buf, "0"))
		new_value = 0;
	else {
		pr_err("%s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	pr_info("Enable : %d - %s - part_type: %d - %d\n",
		new_value, __func__,
		data->part_type, PART_TYPE_MAX86100A);

	mutex_lock(&data->activelock);
	if (data->part_type >= PART_TYPE_MAX86100A) {
		if (atomic_read(&data->uv_is_enable))
			max86100_uv_mode_enable(data, 0);

		if (new_value && (atomic_read(&data->hrm_is_enable) == 0)) {
			max86100_hrm_mode_enable(data, 2);
		} else if (new_value && (atomic_read(&data->hrm_is_enable) == 1)) {
			max86100_hrm_mode_enable(data, 0);
			max86100_hrm_mode_enable(data, 2);
		} else if (!new_value && atomic_read(&data->hrm_is_enable) == 2)
			max86100_hrm_mode_enable(data, 0);
		else if (!new_value && atomic_read(&data->hrm_is_enable) == 1) {
			pr_info("The mode that is tried to disable is not active."
					"It's already disabled. %s:%d\n", __func__, __LINE__);
		}
	}

	mutex_unlock(&data->activelock);
	return count;
}

static struct device_attribute dev_attr_acfd_enable =
	__ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
	max86100_acfd_enable_show, max86100_acfd_enable_store);

static struct device_attribute dev_attr_acfd_poll_delay =
	__ATTR(poll_delay, S_IRUGO|S_IWUSR|S_IWGRP,
	max86100_acfd_poll_delay_show, max86100_acfd_poll_delay_store);

static struct attribute *acfd_sysfs_attrs[] = {
	&dev_attr_acfd_enable.attr,
	&dev_attr_acfd_poll_delay.attr,
	NULL
};

static struct attribute_group acfd_attribute_group = {
	.attrs = acfd_sysfs_attrs,
};

/* uv sysfs */
static ssize_t max86000_uv_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&data->uv_is_enable));
}

static ssize_t max86000_uv_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);
	int new_value;

	if (sysfs_streq(buf, "1"))
		new_value = 1;
	else if (sysfs_streq(buf, "2"))
		new_value = 2;
	else if (sysfs_streq(buf, "0"))
		new_value = 0;
	else {
		pr_err("%s - invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	mutex_lock(&data->activelock);
	if (data->part_type < PART_TYPE_MAX86100A) {
		if (atomic_read(&data->hrm_is_enable))
			max86000_hrm_mode_enable(data, 0);

		max86000_uv_mode_enable(data, new_value);
	} else {
		if (atomic_read(&data->hrm_is_enable))
			max86100_hrm_mode_enable(data, 0);

		max86100_uv_mode_enable(data, new_value);
	}

	mutex_unlock(&data->activelock);
	return count;
}

static ssize_t max86000_uv_poll_delay_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%lld\n", 100000000LL);
}

static ssize_t max86000_uv_poll_delay_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	pr_info("%s - uv sensor delay was fixed as 100ms\n", __func__);
	return size;
}

static struct device_attribute dev_attr_uv_enable =
	__ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
	max86000_uv_enable_show, max86000_uv_enable_store);

static struct device_attribute dev_attr_uv_poll_delay =
	__ATTR(poll_delay, S_IRUGO|S_IWUSR|S_IWGRP,
	max86000_uv_poll_delay_show, max86000_uv_poll_delay_store);

static struct attribute *uv_sysfs_attrs[] = {
	&dev_attr_uv_enable.attr,
	&dev_attr_uv_poll_delay.attr,
	NULL
};

static struct attribute_group uv_attribute_group = {
	.attrs = uv_sysfs_attrs,
};

/* hrm test sysfs */
static int max86000_set_led_current(struct max86x00_device_data *data)
{
	int err;

	err = max86000_write_reg(data, MAX86000_LED_CONFIGURATION,
		data->led_current);
	if (err != 0) {
		pr_err("%s - error initializing MAX86000_LED_CONFIGURATION!\n",
			__func__);
		return -EIO;
	}
	pr_info("%s - led current = %u\n", __func__, data->led_current);
	return 0;
}

static int max86100_set_led_current1(struct max86x00_device_data *data)
{
	int err;

	err = max86000_write_reg(data, MAX86100_LED1_PA,
		data->led_current1);
	if (err != 0) {
		pr_err("%s - error initializing MAX86100_LED1_PA!\n",
			__func__);
		return -EIO;
	}
	pr_info("%s - led current = %u\n", __func__, data->led_current1);
	return 0;
}

static int max86100_set_led_current2(struct max86x00_device_data *data)
{
	int err;

	err = max86000_write_reg(data, MAX86100_LED2_PA,
		data->led_current2);
	if (err != 0) {
		pr_err("%s - error initializing MAX86100_LED2_PA!\n",
			__func__);
		return -EIO;
	}
	pr_info("%s - led current = %u\n", __func__, data->led_current2);
	return 0;
}

static int max86100_set_led_current3(struct max86x00_device_data *data)
{
	int err;

	err = max86000_write_reg(data, MAX86100_LED3_PA,
		data->led_current3);
	if (err != 0) {
		pr_err("%s - error initializing MAX86100_LED3_PA!\n",
			__func__);
		return -EIO;
	}
	pr_info("%s - led current = %u\n", __func__, data->led_current3);
	return 0;
}

static int max86100_set_led_current4(struct max86x00_device_data *data)
{
	int err;

	err = max86000_write_reg(data, MAX86100_LED4_PA,
		data->led_current4);
	if (err != 0) {
		pr_err("%s - error initializing MAX86100_LED4_PA!\n",
			__func__);
		return -EIO;
	}
	pr_info("%s - led current = %u\n", __func__, data->led_current4);
	return 0;
}

static int max86000_set_hr_range(struct max86x00_device_data *data)
{
	pr_info("%s - hr_range = %u(0x%x)\n", __func__,
			data->hr_range, data->hr_range);
	return 0;
}

static int max86000_set_hr_range2(struct max86x00_device_data *data)
{
	pr_info("%s - hr_range2 = %u\n", __func__, data->hr_range2);
	return 0;
}

static int max86000_set_look_mode_ir(struct max86x00_device_data *data)
{
	pr_info("%s - look mode ir = %u\n", __func__, data->look_mode_ir);
	return 0;
}

static int max86000_set_look_mode_red(struct max86x00_device_data *data)
{
	pr_info("%s - look mode red = %u\n", __func__, data->look_mode_red);
	return 0;
}

static int max86000_hrm_eol_test_enable(struct max86x00_device_data *data)
{
	int err;
	u8 led_current;
	data->led = 1; /* Prevent resetting MAX86000_LED_CONFIGURATION */
	data->sample_cnt = 0;

	pr_info("%s\n", __func__);
	/* Test Mode Setting Start */
	data->hr_range = 0; /* Set test phase as 0 */
	data->eol_test_status = 0;
	data->test_current_ir = data->look_mode_ir;
	data->test_current_red = data->look_mode_red;
	led_current = (data->test_current_red << 4) | data->test_current_ir;

	err = max86000_write_reg(data, MAX86000_INTERRUPT_ENABLE, 0x10);
	if (err != 0) {
		pr_err("%s - error initializing MAX86000_INTERRUPT_ENABLE!\n",
			__func__);
		return -EIO;
	}

	err = max86000_write_reg(data, MAX86000_LED_CONFIGURATION, led_current);
	if (err != 0) {
		pr_err("%s - error initializing MAX86000_LED_CONFIGURATION!\n",
			__func__);
		return -EIO;
	}

	err = max86000_write_reg(data, MAX86000_SPO2_CONFIGURATION, 0x47);
	if (err != 0) {
		pr_err("%s - error initializing MAX86000_SPO2_CONFIGURATION!\n",
			__func__);
		return -EIO;
	}

	/* Clear FIFO */
	err = max86000_write_reg(data, MAX86000_FIFO_WRITE_POINTER, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86000_FIFO_WRITE_POINTER!\n",
			__func__);
		return -EIO;
	}

	err = max86000_write_reg(data, MAX86000_OVF_COUNTER, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86000_OVF_COUNTER!\n",
			__func__);
		return -EIO;
	}

	err = max86000_write_reg(data, MAX86000_FIFO_READ_POINTER, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86000_FIFO_READ_POINTER!\n",
			__func__);
		return -EIO;
	}

	/* Shutdown Clear */
	err = max86000_write_reg(data, MAX86000_MODE_CONFIGURATION, 0x0B);
	if (err != 0) {
		pr_err("%s - error initializing MAX86000_MODE_CONFIGURATION!\n",
			__func__);
		return -EIO;
	}

	return 0;
}

static void max86000_eol_test_onoff(
			struct max86x00_device_data *data, int onoff)
{
	int err;

	if (onoff) {
		err = max86000_hrm_eol_test_enable(data);
		data->eol_test_is_enable = 1;
		if (err != 0)
			pr_err("max86000_hrm_eol_test_enable err : %d\n", err);
	} else {
		pr_info("%s - eol test off\n", __func__);
		err = max86000_disable(data);
		if (err != 0)
			pr_err("max86000_disable err : %d\n", err);

		data->hr_range = 0;
		data->led_current = data->default_current;

		err = max86000_init_device(data);
		if (err)
			pr_err("%s max86000_init device fail err = %d",
				__func__, err);

		err = max86000_hrm_enable(data);
		if (err != 0)
			pr_err("max86000_enable err : %d\n", err);

		data->eol_test_is_enable = 0;
	}
	pr_info("%s - onoff = %d\n", __func__, onoff);
}

static void max86100_eol_test_onoff(
			struct max86x00_device_data *data, int onoff)
{
	if (onoff) {
		data->default_current1 = MAX86100_DEFAULT_CURRENT1;
		data->default_current2 = MAX86100_DEFAULT_CURRENT2;
		data->led_current1 = MAX86100_DEFAULT_CURRENT2;
		data->led_current2 = MAX86100_DEFAULT_CURRENT2;
		data->eol_test_is_enable = 1;
		data->agc_is_enable = false;
	} else {
		data->eol_test_is_enable = 0;
		data->agc_is_enable = true;
		data->default_current1 = MAX86100_DEFAULT_CURRENT1;
		data->default_current2 = MAX86100_DEFAULT_CURRENT2;
		data->led_current1 = MAX86100_DEFAULT_CURRENT2;
		data->led_current2 = MAX86100_DEFAULT_CURRENT2;
	}
	pr_info("%s - onoff = %d\n", __func__, onoff);
}

static int max86100_get_device_id(struct max86x00_device_data *data,
										u8 *part_id,
										u8 *rev_id,
										unsigned long long *device_id)
{
	u8 recvData;
	int err;
	int low = 0;
	int high = 0;
	int clock_code = 0;
	int VREF_trim_code = 0;
	int IREF_trim_code = 0;
	int UVL_trim_code = 0;
	int SPO2_trim_code = 0;
	int ir_led_code = 0;
	int red_led_code = 0;
	int TS_trim_code = 0;

	if (!atomic_read(&data->uv_is_enable)
			&& !atomic_read(&data->hrm_is_enable)) {
		pr_info("%s - regulator on\n", __func__);
		err = max86000_regulator_onoff(data, HRM_LDO_ON);
		if (err < 0) {
			pr_err("%s max86000_regulator_on fail err = %d\n",
				__func__, err);
			return -EIO;
		}
		usleep_range(1000, 1100);
	}

	*part_id = MAX86100_WHOAMI_REG_PART;
	err = max86000_read_reg(data, part_id, 1);
	if (err) {
		pr_err("%s - max86000_read_reg err:%d, address:0x%02x\n",
			__func__, err, *part_id);
		return -EIO;
	}

	*rev_id = MAX86100_WHOAMI_REG_REV;
	err = max86000_read_reg(data, rev_id, 1);
	if (err) {
		pr_err("%s - max86000_read_reg err:%d, address:0x%02x\n",
			__func__, err, *rev_id);
		return -EIO;
	}

	*device_id = 0;

	err = max86000_write_reg(data, 0xFF, 0x54);
	if (err) {
		pr_err("%s - error initializing MAX86000_MODE_TEST0!\n",
			__func__);
		return -EIO;
	}

	err = max86000_write_reg(data, 0xFF, 0x4d);
	if (err) {
		pr_err("%s - error initializing MAX86000_MODE_TEST1!\n",
			__func__);
		return -EIO;
	}

	recvData = 0x8B;
	err = max86000_read_reg(data, &recvData, 1);
	if (err) {
		pr_err("%s - max86000_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}
	high = recvData;

	recvData = 0x8C;
	err = max86000_read_reg(data, &recvData, 1);
	if (err) {
		pr_err("%s - max86000_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}
	low = recvData;

	recvData = 0x88;
	err = max86000_read_reg(data, &recvData, 1);
	if (err) {
		pr_err("%s - max86000_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}
	clock_code = recvData;

	recvData = 0x89;
	err = max86000_read_reg(data, &recvData, 1);
	if (err) {
		pr_err("%s - max86000_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}
	VREF_trim_code = recvData & 0x0F;

	recvData = 0x8A;
	err = max86000_read_reg(data, &recvData, 1);
	if (err) {
		pr_err("%s - max86000_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}
	IREF_trim_code = (recvData >> 4) & 0x0F;
	UVL_trim_code = recvData & 0x0F;

	recvData = 0x90;
	err = max86000_read_reg(data, &recvData, 1);
	if (err) {
		pr_err("%s - max86000_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}
	SPO2_trim_code = recvData & 0x7F;

	recvData = 0x98;
	err = max86000_read_reg(data, &recvData, 1);
	if (err) {
		pr_err("%s - max86000_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}
	ir_led_code = (recvData >> 4) & 0x0F;
	red_led_code = recvData & 0x0F;

	recvData = 0x9D;
	err = max86000_read_reg(data, &recvData, 1);
	if (err) {
		pr_err("%s - max86000_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		return -EIO;
	}
	TS_trim_code = recvData;

	err = max86000_write_reg(data, 0xFF, 0x00);
	if (err) {
		pr_err("%s - error initializing MAX86000_MODE_TEST0!\n",
			__func__);
		return -EIO;
	}

	if (!atomic_read(&data->uv_is_enable)
			&& !atomic_read(&data->hrm_is_enable)) {
		pr_info("%s - regulator off\n", __func__);
		err = max86000_regulator_onoff(data, HRM_LDO_OFF);
		if (err < 0) {
			pr_err("%s max86000_regulator_off fail err = %d\n",
				__func__, err);
			return -EIO;
		}
	}
	
	*device_id = clock_code * 16 + VREF_trim_code;
	*device_id = *device_id * 16 + IREF_trim_code;
	*device_id = *device_id * 16 + UVL_trim_code;
	*device_id = *device_id * 128 + SPO2_trim_code;
	*device_id = *device_id * 64 + ir_led_code;
	*device_id = *device_id * 64 + red_led_code;
	*device_id = *device_id * 16 + TS_trim_code;

	pr_info("%s - Device ID = %lld\n", __func__, *device_id);

	return 0;
}

static ssize_t max86000_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	if (data->part_type < PART_TYPE_MAX86100A)
		return snprintf(buf, PAGE_SIZE, "%s\n", MAX86000_CHIP_NAME);
	else if (data->part_type < PART_TYPE_MAX86100LC)
		return snprintf(buf, PAGE_SIZE, "%s\n", MAX86100_CHIP_NAME);
	else
		return snprintf(buf, PAGE_SIZE, "%s\n", MAX86100LC_CHIP_NAME);
}

static ssize_t max86000_hrm_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t led_current_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	if (data->part_type < PART_TYPE_MAX86100A) {

		err = kstrtou8(buf, 10, &data->led_current);
		if (err < 0)
			return err;

		mutex_lock(&data->activelock);
		err = max86000_set_led_current(data);
		if (err < 0) {
			mutex_unlock(&data->activelock);
			return err;
		}

		data->default_current = data->led_current;
		mutex_unlock(&data->activelock);
	}

	return size;
}

static ssize_t led_current_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	pr_info("max86000_%s - led_current = %u\n",
		__func__, data->led_current);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->led_current);
}

static ssize_t led_current1_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	err = kstrtou8(buf, 10, &data->led_current1);
	if (err < 0)
		return err;

	mutex_lock(&data->activelock);
	err = max86100_set_led_current1(data);
	if (err < 0) {
		mutex_unlock(&data->activelock);
		return err;
	}

	data->default_current1 = data->led_current1;
	mutex_unlock(&data->activelock);

	return size;
}

static ssize_t led_current1_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	pr_info("max86000_%s - led_current1 = %u\n", __func__,
		data->led_current1);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->led_current1);
}

static ssize_t led_current2_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	err = kstrtou8(buf, 10, &data->led_current2);
	if (err < 0)
		return err;

	mutex_lock(&data->activelock);
	err = max86100_set_led_current2(data);
	if (err < 0) {
		mutex_unlock(&data->activelock);
		return err;
	}

	data->default_current2 = data->led_current2;
	mutex_unlock(&data->activelock);

	return size;
}

static ssize_t led_current2_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	pr_info("max86000_%s - led_current2 = %u\n",
		__func__, data->led_current2);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->led_current2);
}

static ssize_t led_current3_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	err = kstrtou8(buf, 10, &data->led_current3);
	if (err < 0)
		return err;

	mutex_lock(&data->activelock);
	err = max86100_set_led_current3(data);
	if (err < 0) {
		mutex_unlock(&data->activelock);
		return err;
	}

	data->default_current3 = data->led_current3;
	mutex_unlock(&data->activelock);

	return size;
}

static ssize_t led_current3_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	pr_info("max86000_%s - led_current3 = %u\n", __func__, data->led_current3);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->led_current3);
}

static ssize_t led_current4_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	err = kstrtou8(buf, 10, &data->led_current4);
	if (err < 0)
		return err;

	mutex_lock(&data->activelock);
	err = max86100_set_led_current4(data);
	if (err < 0) {
		mutex_unlock(&data->activelock);
		return err;
	}

	data->default_current4 = data->led_current4;
	mutex_unlock(&data->activelock);

	return size;
}

static ssize_t led_current4_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	pr_info("max86000_%s - led_current4 = %u\n",
		__func__, data->led_current4);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->led_current4);
}

static ssize_t hr_range_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	err = kstrtou8(buf, 10, &data->hr_range);
	if (err < 0)
		return err;

	mutex_lock(&data->activelock);
	err = max86000_set_hr_range(data);
	mutex_unlock(&data->activelock);
	if (err < 0)
		return err;

	return size;
}

static ssize_t hr_range_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	pr_info("max86000_%s - hr_range = %x\n", __func__, data->hr_range);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->hr_range);
}

static ssize_t hr_range2_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	err = kstrtou8(buf, 10, &data->hr_range2);
	if (err < 0)
		return err;

	mutex_lock(&data->activelock);
	err = max86000_set_hr_range2(data);
	mutex_unlock(&data->activelock);
	if (err < 0)
		return err;

	return size;
}

static ssize_t hr_range2_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	pr_info("max86000_%s - hr_range2 = %x\n", __func__, data->hr_range2);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->hr_range2);
}

static ssize_t look_mode_ir_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	err = kstrtou8(buf, 10, &data->look_mode_ir);
	if (err < 0)
		return err;

	mutex_lock(&data->activelock);
	err = max86000_set_look_mode_ir(data);
	mutex_unlock(&data->activelock);
	if (err < 0)
		return err;

	return size;
}

static ssize_t look_mode_ir_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	pr_info("max86000_%s - look_mode_ir = %x\n",
		__func__, data->look_mode_ir);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->look_mode_ir);
}

static ssize_t look_mode_red_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	err = kstrtou8(buf, 10, &data->look_mode_red);
	if (err < 0)
		return err;

	mutex_lock(&data->activelock);
	err = max86000_set_look_mode_red(data);
	mutex_unlock(&data->activelock);
	if (err < 0)
		return err;

	return size;
}

static ssize_t look_mode_red_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	pr_info("max86000_%s - look_mode_red = %x\n",
		__func__, data->look_mode_red);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->look_mode_red);
}

static ssize_t eol_test_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int test_onoff;
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "1")) /* eol_test start */
		test_onoff = 1;
	else if (sysfs_streq(buf, "0")) /* eol_test stop */
		test_onoff = 0;
	else {
		pr_debug("max86000_%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	mutex_lock(&data->activelock);
	if (data->part_type < PART_TYPE_MAX86100A)
		max86000_eol_test_onoff(data, test_onoff);
	else
		max86100_eol_test_onoff(data, test_onoff);

	mutex_unlock(&data->activelock);
	return size;
}

static ssize_t eol_test_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%u\n", data->eol_test_is_enable);
}

static ssize_t eol_test_result_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);
	unsigned int buf_len;
	mutex_lock(&data->activelock);
	buf_len = strlen(buf) + 1;
	if (buf_len > MAX_EOL_RESULT)
		buf_len = MAX_EOL_RESULT;

	if (data->eol_test_result != NULL)
		kfree(data->eol_test_result);

	data->eol_test_result = kzalloc(sizeof(char) * buf_len, GFP_KERNEL);
	if (data->eol_test_result == NULL) {
		pr_err("max86000_%s - couldn't allocate memory\n",
			__func__);
		mutex_unlock(&data->activelock);
		return -ENOMEM;
	}
	strlcpy(data->eol_test_result, buf, buf_len);
	pr_info("max86000_%s - result = %s, buf_len(%u)\n",
		__func__, data->eol_test_result, buf_len);
	data->eol_test_status = 1;
	mutex_unlock(&data->activelock);
	return size;
}

static ssize_t eol_test_result_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	if (data->eol_test_result == NULL) {
		pr_info("max86000_%s - data->eol_test_result is NULL\n",
			__func__);
		data->eol_test_status = 0;
		return snprintf(buf, PAGE_SIZE, "%s\n", "NO_EOL_TEST");
	}
	pr_info("max86000_%s - result = %s\n", __func__, data->eol_test_result);
	data->eol_test_status = 0;
	return snprintf(buf, PAGE_SIZE, "%s\n", data->eol_test_result);
}

static ssize_t eol_test_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->eol_test_status);
}

static ssize_t int_pin_check(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);
	int err = -1;
	int pin_state = -1;
	u8 recvData;

	mutex_lock(&data->activelock);

	/* DEVICE Power-up */
	err = max86000_regulator_onoff(data, HRM_LDO_ON);
	if (err < 0) {
		pr_err("max86000_%s - regulator on fail\n", __func__);
		goto exit;
	}

	usleep_range(1000, 1100);
	/* check INT pin state */
	pin_state = gpio_get_value_cansleep(data->hrm_int);

	if (pin_state) {
		pr_err("max86000_%s - INT pin state is high before INT clear\n",
			__func__);
		err = -1;
		max86000_regulator_onoff(data, HRM_LDO_OFF);
		goto exit;
	}

	pr_info("max86000_%s - Before INT clear %d\n", __func__, pin_state);
	/* Interrupt Clear */
	recvData = MAX86000_INTERRUPT_STATUS;
	err = max86000_read_reg(data, &recvData, 1);
	if (err) {
		pr_err("max86000_%s - max86000_read_reg err:%d, address:0x%02x\n",
			__func__, err, recvData);
		max86000_regulator_onoff(data, HRM_LDO_OFF);
		goto exit;
	}

	/* check INT pin state */
	pin_state = gpio_get_value_cansleep(data->hrm_int);

	if (!pin_state) {
		pr_err("max86000_%s - INT pin state is low after INT clear\n",
			__func__);
		err = -1;
		max86000_regulator_onoff(data, HRM_LDO_OFF);
		goto exit;
	}
	pr_info("max86000_%s - After INT clear %d\n", __func__, pin_state);

	err = max86000_regulator_onoff(data, HRM_LDO_OFF);
	if (err < 0)
		pr_err("max86000_%s - regulator off fail\n", __func__);

	pr_info("max86000_%s - success\n", __func__);
exit:
	mutex_unlock(&data->activelock);
	return snprintf(buf, PAGE_SIZE, "%d\n", err);
}

static ssize_t max86000_lib_ver_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);
	unsigned int buf_len;
	mutex_lock(&data->activelock);
	buf_len = strlen(buf) + 1;
	if (buf_len > MAX_LIB_VER)
		buf_len = MAX_LIB_VER;

	if (data->lib_ver != NULL)
		kfree(data->lib_ver);

	data->lib_ver = kzalloc(sizeof(char) * buf_len, GFP_KERNEL);
	if (data->lib_ver == NULL) {
		pr_err("%s - couldn't allocate memory\n", __func__);
		mutex_unlock(&data->activelock);
		return -ENOMEM;
	}
	strlcpy(data->lib_ver, buf, buf_len);
	pr_info("%s - lib_ver = %s\n", __func__, data->lib_ver);
	mutex_unlock(&data->activelock);
	return size;
}

static ssize_t max86000_lib_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	if (data->lib_ver == NULL) {
		pr_info("%s - data->lib_ver is NULL\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s\n", "NULL");
	}
	pr_info("%s - lib_ver = %s\n", __func__, data->lib_ver);
	return snprintf(buf, PAGE_SIZE, "%s\n", data->lib_ver);
}

static ssize_t regulator_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int regulator_onoff;
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "1")) /* Regulator On */
		regulator_onoff = HRM_LDO_ON;
	else if (sysfs_streq(buf, "0")) /* Regulator Off */
		regulator_onoff = HRM_LDO_OFF;
	else {
		pr_debug("max86000_%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	mutex_lock(&data->activelock);
	if (atomic_read(&data->regulator_is_enable) != regulator_onoff)
		max86000_regulator_onoff(data, regulator_onoff);

	mutex_unlock(&data->activelock);
	return size;
}

static ssize_t regulator_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%u\n", atomic_read(&data->regulator_is_enable));
}


static ssize_t pwell_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);
	int pwell;
	int ret;

	if (sysfs_streq(buf, "1")) /* Pwell On */
		pwell = 0x9F;
	else if (sysfs_streq(buf, "0")) /* Pwell Off */
		pwell = 0x00;
	else {
		pr_debug("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	mutex_lock(&data->activelock);
	ret = max86000_write_reg(data, 0xFF, 0x54);
	ret |= max86000_write_reg(data, 0xFF, 0x4d);
	ret |= max86000_write_reg(data, 0x8F, pwell);
	ret |= max86000_write_reg(data, 0xFF, 0x00);
	if (ret < 0) {
		pr_err("%s failed. ret: %d\n", __func__, ret);
		mutex_unlock(&data->activelock);
		return ret;
	}

	mutex_unlock(&data->activelock);
	return size;
}

static ssize_t pwell_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);
	u8 tmp;
	int ret;
	int pwell = 0;

	mutex_lock(&data->activelock);
	ret = max86000_write_reg(data, 0xFF, 0x54);
	ret |= max86000_write_reg(data, 0xFF, 0x4d);
	tmp = 0x8F;
	ret |= max86000_read_reg(data, &tmp, 1);
	ret |= max86000_write_reg(data, 0xFF, 0x00);
	if (ret < 0) {
		pr_err("%s failed. ret: %d\n", __func__, ret);
		mutex_unlock(&data->activelock);
		return ret;
	}

	if (tmp & 0x80)
		pwell = 1;

	mutex_unlock(&data->activelock);
	return snprintf(buf, PAGE_SIZE, "%u\n", pwell);
}

static ssize_t part_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%u\n", data->part_type);
}

static ssize_t device_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);
	u8 part_id;
	u8 rev_id;
	unsigned long long device_id = 0;

	mutex_lock(&data->activelock);
	max86100_get_device_id(data, &part_id, &rev_id, &device_id);
	mutex_unlock(&data->activelock);
	return snprintf(buf, PAGE_SIZE, "%.2X,%.2X,%lld\n", part_id, rev_id, device_id);
}

static struct device_attribute dev_attr_name =
		__ATTR(name, S_IRUGO, max86000_name_show, NULL);
static struct device_attribute dev_attr_hrm_vendor =
		__ATTR(vendor, S_IRUGO, max86000_hrm_vendor_show, NULL);

static DEVICE_ATTR(led_current, S_IRUGO | S_IWUSR | S_IWGRP,
	led_current_show, led_current_store);
static DEVICE_ATTR(led_current1, S_IRUGO | S_IWUSR | S_IWGRP,
	led_current1_show, led_current1_store);
static DEVICE_ATTR(led_current2, S_IRUGO | S_IWUSR | S_IWGRP,
	led_current2_show, led_current2_store);
static DEVICE_ATTR(led_current3, S_IRUGO | S_IWUSR | S_IWGRP,
	led_current3_show, led_current3_store);
static DEVICE_ATTR(led_current4, S_IRUGO | S_IWUSR | S_IWGRP,
	led_current4_show, led_current4_store);
static DEVICE_ATTR(hr_range, S_IRUGO | S_IWUSR | S_IWGRP,
	hr_range_show, hr_range_store);
static DEVICE_ATTR(hr_range2, S_IRUGO | S_IWUSR | S_IWGRP,
	hr_range2_show, hr_range2_store);
static DEVICE_ATTR(look_mode_ir, S_IRUGO | S_IWUSR | S_IWGRP,
	look_mode_ir_show, look_mode_ir_store);
static DEVICE_ATTR(look_mode_red, S_IRUGO | S_IWUSR | S_IWGRP,
	look_mode_red_show, look_mode_red_store);
static DEVICE_ATTR(eol_test, S_IRUGO | S_IWUSR | S_IWGRP,
	eol_test_show, eol_test_store);
static DEVICE_ATTR(eol_test_result, S_IRUGO | S_IWUSR | S_IWGRP,
	eol_test_result_show, eol_test_result_store);
static DEVICE_ATTR(eol_test_status, S_IRUGO, eol_test_status_show, NULL);
static DEVICE_ATTR(int_pin_check, S_IRUGO, int_pin_check, NULL);
static DEVICE_ATTR(lib_ver, S_IRUGO | S_IWUSR | S_IWGRP,
	max86000_lib_ver_show, max86000_lib_ver_store);
static DEVICE_ATTR(regulator, S_IRUGO | S_IWUSR | S_IWGRP,
	regulator_show, regulator_store);
static DEVICE_ATTR(part_type, S_IRUGO, part_type_show, NULL);
static DEVICE_ATTR(device_id, S_IRUGO, device_id_show, NULL);
static DEVICE_ATTR(pwell_enable, S_IRUGO | S_IWUSR | S_IWGRP,
	pwell_enable_show, pwell_enable_store);

static struct device_attribute *hrm_sensor_attrs[] = {
	&dev_attr_name,
	&dev_attr_hrm_vendor,
	&dev_attr_led_current,
	&dev_attr_led_current1,
	&dev_attr_led_current2,
	&dev_attr_led_current3,
	&dev_attr_led_current4,
	&dev_attr_hr_range,
	&dev_attr_hr_range2,
	&dev_attr_look_mode_ir,
	&dev_attr_look_mode_red,
	&dev_attr_eol_test,
	&dev_attr_eol_test_result,
	&dev_attr_eol_test_status,
	&dev_attr_int_pin_check,
	&dev_attr_lib_ver,
	&dev_attr_regulator,
	&dev_attr_part_type,
	&dev_attr_device_id,
	&dev_attr_pwell_enable,
	NULL,
};

static void max86100_uv_eol_test_onoff(
			struct max86x00_device_data *data, int onoff)
{
	if (onoff) {
		data->uv_eol_test_is_enable = 1;
	} else {
		data->uv_eol_test_is_enable = 0;
	}
	pr_info("%s - onoff = %d\n", __func__, onoff);
}

/* uv test sysfs */
static ssize_t max86000_uv_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t max86000_uv_lib_ver_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);
	unsigned int buf_len;
	mutex_lock(&data->activelock);
	buf_len = strlen(buf) + 1;
	if (buf_len > MAX_LIB_VER)
		buf_len = MAX_LIB_VER;

	if (data->uv_lib_ver != NULL)
		kfree(data->uv_lib_ver);

	data->uv_lib_ver = kzalloc(sizeof(char) * buf_len, GFP_KERNEL);
	if (data->uv_lib_ver == NULL) {
		pr_err("%s - couldn't allocate memory\n", __func__);
		mutex_unlock(&data->activelock);
		return -ENOMEM;
	}
	strlcpy(data->uv_lib_ver, buf, buf_len);
	pr_info("%s - uv_lib_ver = %s\n", __func__, data->uv_lib_ver);
	mutex_unlock(&data->activelock);
	return size;
}

static ssize_t max86000_uv_lib_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	if (data->uv_lib_ver == NULL) {
		pr_info("%s - data->uv_lib_ver is NULL\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s\n", "NULL");
	}
	pr_info("%s - lib_ver = %s\n", __func__, data->uv_lib_ver);
	return snprintf(buf, PAGE_SIZE, "%s\n", data->uv_lib_ver);
}

static ssize_t max86000_uv_sr_interval_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	mutex_lock(&data->activelock);
	err = kstrtou16(buf, 10, &data->uv_sr_interval);
	mutex_unlock(&data->activelock);
	if (err < 0)
		return err;

	return size;
}

static ssize_t max86000_uv_sr_interval_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	pr_info("max86000_%s - uv_sr_interval = %u\n",
							__func__, data->uv_sr_interval);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->uv_sr_interval);
}

static ssize_t uv_eol_test_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int test_onoff;
	struct max86x00_device_data *data = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "1")) /* eol_test start */
		test_onoff = 1;
	else if (sysfs_streq(buf, "0")) /* eol_test stop */
		test_onoff = 0;
	else {
		pr_debug("max86000_%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	mutex_lock(&data->activelock);
	max86100_uv_eol_test_onoff(data, test_onoff);
	mutex_unlock(&data->activelock);

	return size;
}

static ssize_t uv_eol_test_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%u\n", data->eol_test_is_enable);
}

static struct device_attribute dev_attr_uv_vendor =
		__ATTR(vendor, S_IRUGO, max86000_uv_vendor_show, NULL);
static DEVICE_ATTR(uv_lib_ver, S_IRUGO | S_IWUSR | S_IWGRP,
	max86000_uv_lib_ver_show, max86000_uv_lib_ver_store);
static DEVICE_ATTR(uv_sr_interval, S_IRUGO | S_IWUSR | S_IWGRP,
	max86000_uv_sr_interval_show, max86000_uv_sr_interval_store);
static DEVICE_ATTR(uv_eol_test, S_IRUGO | S_IWUSR | S_IWGRP,
	uv_eol_test_show, uv_eol_test_store);

static struct device_attribute *uv_sensor_attrs[] = {
	&dev_attr_name,
	&dev_attr_device_id,
	&dev_attr_uv_vendor,
	&dev_attr_uv_lib_ver,
	&dev_attr_uv_sr_interval,
	&dev_attr_uv_eol_test,
	NULL,
};

static struct device_attribute *acfd_sensor_attrs[] = {
	&dev_attr_name,
	&dev_attr_part_type,
	&dev_attr_device_id,
	NULL,
};

static void max86000_hrm_irq_handler(struct max86x00_device_data *data)
{
	int err;
	u16 raw_data[4] = {0x00, };

	err = max86000_hrm_read_data(data, raw_data);
	if (err < 0)
		pr_err("max86000_hrm_read_data err : %d\n", err);

	if (err == 0) {
		if (!data->agc_is_enable || (data->agc_is_enable && data->prox_detect)) {
			input_report_rel(data->hrm_input_dev, REL_X, raw_data[0] + 1); /* IR */
			input_report_rel(data->hrm_input_dev, REL_Y, raw_data[1] + 1); /* RED */
			input_report_rel(data->hrm_input_dev, REL_Z, data->hrm_temp + 1);
			input_sync(data->hrm_input_dev);
		}
	}

	return;
}

static void max86100_hrm_irq_handler(struct max86x00_device_data *data)
{
	int err;
	int raw_data[5] = {0x00, };
	struct input_dev *input_dev;

	err = max86100_hrm_read_data(data, raw_data);
	if (err < 0)
		pr_err("max86100_hrm_read_data err : %d\n", err);

	if (err == 0) {
		if (atomic_read(&data->hrm_is_enable) == MAX86X100_PPG_MODE) {
			if (data->agc_is_enable) {
				err = max86x00_prox_check(data, raw_data[0]);
				if (err) {
					pr_err("Proximity check error. - %s:%d\n",
						__func__, __LINE__);
					return;
				}

				if (data->prox_detect) {
					input_dev = data->hrm_input_dev;
					input_report_rel(input_dev, REL_X,  raw_data[0] + 1);
					input_report_rel(input_dev, REL_Y,  raw_data[1] + 1);
					input_report_rel(input_dev, REL_Z,  data->hrm_temp + 1);
					input_report_rel(input_dev, REL_RX, raw_data[2] + 1);
					input_report_rel(input_dev, REL_RY, raw_data[3] + 1);
					input_sync(input_dev);

					err = max86x00_hrm_agc(data, raw_data[0], MAX86X00_LED2);
					err |= max86x00_hrm_agc(data, raw_data[1], MAX86X00_LED1);
					if (err) {
						pr_err("Auto gain control error. - %s:%d\n",
							__func__, __LINE__);
						return;
					}
				}

			} else {
				input_dev = data->hrm_input_dev;
				input_report_rel(input_dev, REL_X,  raw_data[0] + 1);
				input_report_rel(input_dev, REL_Y,  raw_data[1] + 1);
				input_report_rel(input_dev, REL_Z,  data->hrm_temp + 1);
				input_report_rel(input_dev, REL_RX, raw_data[2] + 1);
				input_report_rel(input_dev, REL_RY, raw_data[3] + 1);
				input_sync(input_dev);
			}
		} else if (atomic_read(&data->hrm_is_enable) == MAX86X100_ACFD_MODE) {
			input_dev = data->acfd_input_dev;
			input_report_rel(input_dev, REL_X,  raw_data[0] + 1);
			input_sync(input_dev);
		} else {
			pr_err("Wrong mode: %d - %s:%d\n",
				atomic_read(&data->hrm_is_enable),
				__func__, __LINE__);
			return;
		}

	}

	return;
}

static void max86000_uv_irq_handler(struct max86x00_device_data *data)
{
	int err;
	int raw_data[3];

	err = max86000_uv_read_data(data, raw_data);
	if (err < 0)
		pr_err("max86000_uv_read_data err : %d\n", err);

	if (err == 0) {
		input_report_rel(data->uv_input_dev, REL_X, raw_data[0] + 1); /* UV Data */
		input_report_rel(data->uv_input_dev, REL_Y, data->hrm_temp + 1);
		input_report_rel(data->uv_input_dev, REL_Z, raw_data[1] + 1);
		input_sync(data->uv_input_dev);
	}

	return;
}

static void max86100_uv_irq_handler(struct max86x00_device_data *data)
{
	int err;
	int raw_data[2] = {0, };

	err = max86100_uvfov_read_data(data, &raw_data[0]);
	if (err < 0)
		pr_err("max86000_uv_read_data err : %d\n", err);

	switch (err) {
	case MAX86100_ENHANCED_UV_MODE:
		input_report_rel(data->uv_input_dev, REL_X, 2*raw_data[0] + 1); /* UV Data, AGC is assumed to be 01 */
		input_report_rel(data->uv_input_dev, REL_Y, data->hrm_temp + 1);
		input_report_rel(data->uv_input_dev, REL_Z, 1);
		input_sync(data->uv_input_dev);
		break;
	case MAX86100_ENHANCED_UV_GESTURE_MODE:
	case MAX86100_ENHANCED_UV_HR_MODE:
	case MAX86100_ENHANCED_UV_EOL_VB_MODE:
	case MAX86100_ENHANCED_UV_EOL_SUM_MODE:
	case MAX86100_ENHANCED_UV_EOL_HR_MODE:
		input_report_rel(data->uv_input_dev, REL_X, raw_data[0] + 1); /* UV Data */
		input_report_rel(data->uv_input_dev, REL_Y, -err);
		input_report_rel(data->uv_input_dev, REL_Z, 1);
		input_sync(data->uv_input_dev);
		break;
	default:
		return;
	}
	return;
}

irqreturn_t max86000_irq_handler(int irq, void *device)
{
	struct max86x00_device_data *data = device;

	if (data->part_type < PART_TYPE_MAX86100A) {
		if (atomic_read(&data->hrm_is_enable))
			max86000_hrm_irq_handler(data);
		else if (atomic_read(&data->uv_is_enable))
			max86000_uv_irq_handler(data);
	} else {
		if (atomic_read(&data->hrm_is_enable))
			max86100_hrm_irq_handler(data);
		else if (atomic_read(&data->uv_is_enable))
			max86100_uv_irq_handler(data);
	}

	return IRQ_HANDLED;
}

static void uv_sr_set(struct work_struct *w)
{
	u8 err;
	struct delayed_work *work_queue = container_of(w, struct delayed_work, work);
	struct max86x00_device_data *data = container_of(work_queue,
									struct max86x00_device_data, uv_sr_work_queue);

	schedule_delayed_work(work_queue, msecs_to_jiffies(data->uv_sr_interval / 2));
	/* Ready to Enable UV ADC convert */
	err = max86000_write_reg(data, MAX86000_TEST_ENABLE_PLETH, 0x00);
	if (err != 0) {
		pr_err("%s - error initializing MAX86000_TEST_ENABLE_PLETH!\n",
			__func__);
		return;
	}
	/* Enable UV ADC convert */
	err = max86000_write_reg(data, MAX86000_TEST_ENABLE_PLETH, 0x01);
	if (err != 0) {
		pr_err("%s - error initializing MAX86000_TEST_ENABLE_PLETH!\n",
			__func__);
		return;
	}

	return;
}

static void max86x00_init_agc_settings(struct max86x00_device_data *data)
{
	data->agc_is_enable = true;

	if (data->part_type < PART_TYPE_MAX86100A) {
		data->hrm_led_init = max86000_hrm_led_init;
		data->prox_led_init = max86000_prox_led_init;
		data->update_led = max86000_update_led_current;
		data->agc_led_out_percent = MAX86000_AGC_DEFAULT_LED_OUT_RANGE;
		data->agc_corr_coeff = MAX86000_AGC_DEFAULT_CORRECTION_COEFF;
		data->agc_min_num_samples = MAX86000_AGC_DEFAULT_MIN_NUM_PERCENT;
		data->agc_sensitivity_percent = MAX86000_AGC_DEFAULT_SENSITIVITY_PERCENT;
	} else {
		data->hrm_led_init = max86100_hrm_led_init;
		data->prox_led_init = max86100_prox_led_init;
		data->update_led = max86100_update_led_current;
		data->agc_led_out_percent = MAX86100_AGC_DEFAULT_LED_OUT_RANGE;
		data->agc_corr_coeff = MAX86100_AGC_DEFAULT_CORRECTION_COEFF;
		data->agc_min_num_samples = MAX86100_AGC_DEFAULT_MIN_NUM_PERCENT;
		data->agc_sensitivity_percent = MAX86100_AGC_DEFAULT_SENSITIVITY_PERCENT;
	}
}

static int max86000_get_part_type(struct max86x00_device_data *data)
{
	int err = -ENODEV;
	u8 buffer[2] = {0, };

	data->client->addr = MAX86100_SLAVE_ADDR;
	buffer[0] = MAX86100_WHOAMI_REG_PART;
	err = max86000_read_reg(data, buffer, 1);
	if (err < 0) {
		data->client->addr = MAX86000_SLAVE_ADDR;
		pr_err("%s-%d, err: %d\n", __func__, __LINE__, err);
		buffer[0] = MAX86100_WHOAMI_REG_PART;
		err = max86000_read_reg(data, buffer, 1);
		pr_err("%s-%d, err: %d\n", __func__, __LINE__, err);
		if (err < 0) {
			pr_err("Register read is failed - %s-%d, err: %d\n",
											__func__, __LINE__, err);
			return -ENODEV;
		}
	}

	if (buffer[0] == MAX86100_PART_ID1
		|| buffer[0] == MAX86100_PART_ID2) {

		/* Check for OS25LC */
		err = max86000_write_reg(data, MAX86000_TEST_MODE, MAX86000_TM_CODE1);
		err |= max86000_write_reg(data, MAX86000_TEST_MODE, MAX86000_TM_CODE2);
		buffer[0] = MAX86000_TEST_LC;
		err |= max86000_read_reg(data, buffer, 1);
		err |= max86000_write_reg(data, MAX86000_TEST_MODE, 0x00);

		if (err) {
			pr_err("%s WHOAMI read fail\n", __func__);
			return -ENODEV;
		}

		if (buffer[0] == 0x01) {
			data->part_type = PART_TYPE_MAX86100LC;
		} else if (buffer[0] == 0x02) {
			data->part_type = PART_TYPE_MAX86100LCA;
		} else {
			buffer[0] = MAX86100_WHOAMI_REG_REV;
			err = max86000_read_reg(data, buffer, 1);
			if (err) {
				pr_err("%s Max86902 WHOAMI read fail\n", __func__);
				return -ENODEV;
			}
			if (buffer[0] == MAX86100_REV_ID1)
				data->part_type = PART_TYPE_MAX86100A;
			else if (buffer[0] == MAX86100_REV_ID2)
				data->part_type = PART_TYPE_MAX86100B;
			else if (buffer[0] == MAX86100_REV_ID3)
				data->part_type = PART_TYPE_MAX86100B;
			else {
				pr_err("%s Max86902 WHOAMI read error : REV ID : 0x%02x\n",
						__func__, buffer[0]);
				return -ENODEV;
			}
		}
		data->default_current1 = MAX86100_DEFAULT_CURRENT1;
		data->default_current2 = MAX86100_DEFAULT_CURRENT2;
		data->default_current3 = MAX86100_DEFAULT_CURRENT3;
		data->default_current4 = MAX86100_DEFAULT_CURRENT4;

	} else {
		data->client->addr = MAX86000A_SLAVE_ADDR;
		buffer[0] = MAX86000_WHOAMI_REG;
		err = max86000_read_reg(data, buffer, 2);

		if (buffer[1] == MAX86000C_WHOAMI) {
			/* MAX86000A & MAX86000B */
			switch (buffer[0]) {
			case MAX86000A_REV_ID:
				data->part_type = PART_TYPE_MAX86000A;
				data->default_current =	MAX86000A_DEFAULT_CURRENT;
				break;
			case MAX86000B_REV_ID:
				data->part_type = PART_TYPE_MAX86000B;
				data->default_current = MAX86000A_DEFAULT_CURRENT;
				break;
			case MAX86000C_REV_ID:
				data->part_type = PART_TYPE_MAX86000C;
				data->default_current = MAX86000C_DEFAULT_CURRENT;
				break;
			default:
				pr_err("%s WHOAMI read error : REV ID : 0x%02x\n",
				__func__, buffer[0]);
				return -ENODEV;
			}
			pr_info("%s - MAX86000 - x21(0x%X), REV ID : 0x%02x\n",
				__func__, MAX86000A_SLAVE_ADDR, buffer[0]);
		} else {
			/* MAX86000 */
			data->client->addr = MAX86000_SLAVE_ADDR;
			buffer[0] = MAX86000_WHOAMI_REG;
			err = max86000_read_reg(data, buffer, 2);

			if (err) {
				pr_err("%s WHOAMI read fail\n", __func__);
				return -ENODEV;
			}
			data->part_type = PART_TYPE_MAX86000;
			data->default_current = MAX86000_DEFAULT_CURRENT;
			pr_info("%s - MAX86000 - x20 (0x%X)\n", __func__,
					MAX86000_SLAVE_ADDR);
		}
	}

	pr_err("%s - part_type = %d\n", __func__, data->part_type);
	return 0;
}

#ifdef CONFIG_OF
static int max86000_parse_dt(struct max86x00_device_data *data,
	struct device *dev)
{
	struct device_node *dNode = dev->of_node;
	enum of_gpio_flags flags;

	if (dNode == NULL)
		return -ENODEV;

	data->hrm_int = of_get_named_gpio_flags(dNode,
		"max86x00,hrm_int-gpio", 0, &flags);
	if (data->hrm_int < 0) {
		pr_err("%s - get hrm_int error\n", __func__);
		return -ENODEV;
	}

#ifdef	CONFIG_SENSORS_MAX86X00_LED_POWER
	data->hrm_en = of_get_named_gpio_flags(dNode,
		"max86x00,hrm_en-gpio", 0, &flags);
	if (data->hrm_en < 0) {
		pr_err("%s - get hrm_int error\n", __func__);
		return -ENODEV;
	}
#endif

#if 0
	if (of_property_read_string(dNode, "max86x00,vdd_1p8",
		&data->vdd_1p8) < 0)
		pr_err("%s - get vdd_1p8 error\n", __func__);
#endif
/*
#ifndef CONFIG_SENSORS_MAX86X00_LED_POWER
	if (of_property_read_string(dNode, "max86x00,led_3p3",
		&data->led_3p3) < 0)
		pr_err("%s - get led_3p3 error\n", __func__);
#endif
*/
	return 0;
}
#else
static int max86000_parse_dt(struct max86x00_device_data *sd,
	struct device *dev)
{
	return -ENODEV;
}
#endif

static int max86000_platform_init(struct max86x00_device_data *data)
{
	int err = -EIO;

#if 0
	if (data->vdd_1p8) {
		data->regulator_vdd_1p8 = regulator_get(NULL, data->vdd_1p8);
		if (IS_ERR(data->regulator_vdd_1p8)) {
			pr_err("%s - vdd_1p8 regulator_get fail\n", __func__);
			err = -ENODEV;
			goto done;
		}
	}
#endif

#ifndef CONFIG_SENSORS_MAX86X00_LED_POWER
	if (data->led_3p3) {
		data->regulator_led_3p3 = regulator_get(NULL, data->led_3p3);
		if (IS_ERR(data->regulator_led_3p3)) {
			pr_err("%s - vdd_3p3 regulator_get fail\n", __func__);
			regulator_put(data->regulator_vdd_1p8);
			err = -ENODEV;
			goto err_regulator_get;
		}
	}
#endif

	if (gpio_is_valid(data->hrm_int)) {
		err = gpio_request(data->hrm_int, "hrm_int");
		if (err) {
			pr_err("%s - failed to request hrm_int\n", __func__);
			return err;
		}
	} else {
		pr_err("hrm_int is not valid\n");
		return err;
	}

	err = gpio_direction_input(data->hrm_int);
	if (err) {
		pr_err("%s - failed to set hrm_int as input\n", __func__);
		goto err_gpio_direction_input;
	}
	data->irq = gpio_to_irq(data->hrm_int);

#ifdef CONFIG_SENSORS_MAX86X00_LED_POWER
	if (gpio_is_valid(data->hrm_en)) {
		err = gpio_request(data->hrm_en, "hrm_en");
		if (err) {
			pr_err("%s - failed to request hrm_en\n", __func__);
			return err;
		}
	} else {
		pr_err("hrm_en is not valid\n");
		err = -EIO;
		goto err_gpio_direction_input;
	}

	err = gpio_direction_output(data->hrm_en, 0);
	if (err) {
		pr_err("%s - failed to set hrm_en as output\n", __func__);
		goto err_gpio_hrm_en;
	}
#endif
	goto done;
#ifdef CONFIG_SENSORS_MAX86X00_LED_POWER
err_gpio_hrm_en:
	gpio_free(data->hrm_en);
#endif
err_gpio_direction_input:
	gpio_free(data->hrm_int);

#ifndef CONFIG_SENSORS_MAX86X00_LED_POWER
	regulator_put(data->regulator_led_3p3);
err_regulator_get:
#endif
	regulator_put(data->regulator_vdd_1p8);

done:
	return err;
}

int max86000_setup_irq(struct max86x00_device_data *data)
{
	int errorno = -EIO;

	errorno = request_threaded_irq(data->irq, NULL,
		max86000_irq_handler, IRQF_TRIGGER_FALLING|IRQF_ONESHOT,
		"hrm_sensor_irq", data);

	if (errorno < 0) {
		pr_err("%s - failed for setup irq errono= %d\n",
				__func__, errorno);
		errorno = -ENODEV;
		return errorno;
	}

	atomic_set(&data->irq_enable, 0);
	disable_irq(data->irq);
	return errorno;
}

int max86000_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = -ENODEV;
	struct max86x00_device_data *data;
	struct max86x00_pdata *pdata;

	pr_info("%s - start\n", __func__);
	/* check to make sure that the adapter supports I2C */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s - I2C_FUNC_I2C not supported\n", __func__);
		return -ENODEV;
	}

	/* allocate some memory for the device */
	data = kzalloc(sizeof(struct max86x00_device_data), GFP_KERNEL);
	if (data == NULL) {
		pr_err("%s - couldn't allocate memory\n", __func__);
		return -ENOMEM;
	}

	data->client = client;
	data->dev = &client->dev;
	i2c_set_clientdata(client, data);

	mutex_init(&data->i2clock);
	mutex_init(&data->activelock);

	if (client->dev.of_node) {
		err = max86000_parse_dt(data, &client->dev);
		if (err < 0) {
			pr_err("[SENSOR] %s - of_node error\n", __func__);
			err = -ENODEV;
			goto err_of_node;
		}
	} else {
		pdata = client->dev.platform_data;
		data->hrm_int = pdata->gpio;
	}

	err = max86000_platform_init(data);
	if (err) {
		pr_err("[SENSOR] %s - could not setup gpio\n", __func__);
		goto err_setup_gpio;
	}

	atomic_set(&data->regulator_is_enable, 0);
	err = max86000_regulator_onoff(data, HRM_LDO_ON);
	if (err < 0) {
		pr_err("%s max86000_regulator_on fail err = %d\n",
			__func__, err);
		goto err_regulator_enable;
	}

#if 1
	//	data->vcc_i2c = regulator_get(&data->client->dev, "vcc_i2c");
		data->vcc_i2c = devm_regulator_get(&data->client->dev, "vcc_i2c");
		if (IS_ERR(data->vcc_i2c)) {
			pr_err("%s - vcc-i2c regulator_get fail\n", __func__);
			err = -ENODEV;
		}
	
		err = regulator_set_optimum_mode(data->vcc_i2c, LVS2_I2C11_1P8_HPM_LOAD);
		if (err < 0) {
			pr_err("%s:Unable to set current of vcc_i2c\n",__func__);
		}
	
		err = regulator_enable(data->vcc_i2c);
		if (err) {
			pr_err("%s - enable vcc-i2c failed, err=%d\n",
			__func__, err);
		}
	
	
		usleep_range(1000, 1100);

#endif

	atomic_set(&data->regulator_is_enable, 0);

	usleep_range(1000, 1100);

	err = max86000_get_part_type(data);
	if (err) {
		pr_err("[SENSOR] %s - could not determine part type\n", __func__);
		goto err_of_read_chipid;
	}

	data->hr_range = 0;
	data->uv_sr_interval = MAX86100_DEFAULT_UV_SR_INTERVAL;
	data->led_current = data->default_current;
	data->led_current1 = data->default_current1;
	data->led_current2 = data->default_current2;
	data->led_current3 = data->default_current3;
	data->led_current4 = data->default_current4;

	/* allocate input device for AC Flicker detection */
	data->acfd_input_dev = input_allocate_device();
	if (!data->acfd_input_dev) {
		pr_err("%s - could not allocate input device\n",
			__func__);
		goto err_acfd_input_allocate_device;
	}

	input_set_drvdata(data->acfd_input_dev, data);
	data->acfd_input_dev->name = MODULE_NAME_ACFD;
	input_set_capability(data->acfd_input_dev, EV_REL, REL_X);
	input_set_events_per_packet(data->acfd_input_dev, 128);
	err = input_register_device(data->acfd_input_dev);
	if (err < 0) {
		pr_err("%s - could not register input device\n", __func__);
		goto err_adfd_input_register_device;
	}

	err = sysfs_create_group(&data->acfd_input_dev->dev.kobj,
				 &acfd_attribute_group);
	if (err) {
		pr_err("Unable to create sysfs group for ACFD. %s\n",
			__func__);
		goto err_acfd_sysfs_create_group;
	}

	/* set sysfs for flicker sensor */
	data->acfd_dev  = sensors_register(data, acfd_sensor_attrs,
			MODULE_NAME_ACFD);
	if (!data->acfd_dev) {
		pr_err("[SENSOR] %s - cound not register acfd sensor(%d).\n",
			__func__, err);
		goto acfd_sensor_register_failed;
	}


	/* allocate input device for HRM*/
	data->hrm_input_dev = input_allocate_device();
	if (!data->hrm_input_dev) {
		pr_err("%s - could not allocate input device\n",
			__func__);
		goto err_hrm_input_allocate_device;
	}
	input_set_drvdata(data->hrm_input_dev, data);
	data->hrm_input_dev->name = MODULE_NAME_HRM;
	input_set_capability(data->hrm_input_dev, EV_REL, REL_X);
	input_set_capability(data->hrm_input_dev, EV_REL, REL_Y);
	input_set_capability(data->hrm_input_dev, EV_REL, REL_Z);
	input_set_capability(data->hrm_input_dev, EV_REL, REL_RX);
	input_set_capability(data->hrm_input_dev, EV_REL, REL_RY);
	input_set_events_per_packet(data->hrm_input_dev, 128);
	err = input_register_device(data->hrm_input_dev);
	if (err < 0) {
		input_free_device(data->hrm_input_dev);
		pr_err("%s - could not register input device\n", __func__);
		goto err_hrm_input_register_device;
	}

	err = sensors_create_symlink(data->hrm_input_dev);
	if (err < 0) {
		pr_err("%s - create_symlink error\n", __func__);
		goto err_hrm_sensors_create_symlink;
	}

	err = sysfs_create_group(&data->hrm_input_dev->dev.kobj,
				 &hrm_attribute_group);
	if (err) {
		pr_err("[SENSOR] %s - could not create sysfs group\n",
			__func__);
		goto err_hrm_sysfs_create_group;
	}

	/* set sysfs for hrm sensor */
	data->hrm_dev  = sensors_register(data, hrm_sensor_attrs,
			MODULE_NAME_HRM);
	if (!data->hrm_dev) {
		pr_err("[SENSOR] %s - cound not register hrm_sensor(%d).\n",
			__func__, err);
		goto hrm_sensor_register_failed;
	}

	/* allocate input device for UV*/
	if (data->part_type < PART_TYPE_MAX86100LC) {
		data->uv_input_dev = input_allocate_device();
		if (!data->uv_input_dev) {
			pr_err("%s - could not allocate input device\n",
				__func__);
			goto err_uv_input_allocate_device;
		}

		input_set_drvdata(data->uv_input_dev, data);
		data->uv_input_dev->name = MODULE_NAME_UV;
		input_set_capability(data->uv_input_dev, EV_REL, REL_X);
		input_set_capability(data->uv_input_dev, EV_REL, REL_Y);
		input_set_capability(data->uv_input_dev, EV_REL, REL_Z);

		err = input_register_device(data->uv_input_dev);
		if (err < 0) {
			input_free_device(data->uv_input_dev);
			pr_err("%s - could not register input device\n", __func__);
			goto err_uv_input_register_device;
		}

		err = sensors_create_symlink(data->uv_input_dev);
		if (err < 0) {
			pr_err("%s - create_symlink error\n", __func__);
			goto err_uv_sensors_create_symlink;
		}

		err = sysfs_create_group(&data->uv_input_dev->dev.kobj,
					 &uv_attribute_group);
		if (err) {
			pr_err("[SENSOR] %s - could not create sysfs group\n",
				__func__);
			goto err_uv_sysfs_create_group;
		}

		/* set sysfs for uv sensor */
		data->uv_dev = sensors_register(data, uv_sensor_attrs,
				MODULE_NAME_UV);
		if (!data->uv_dev) {
			pr_err("[SENSOR] %s - cound not register hrm_sensor(%d).\n",
				__func__, err);
			goto uv_sensor_register_failed;
		}
	}

	/* UV and HRM are not active */
	atomic_set(&data->uv_is_enable, 0);
	atomic_set(&data->hrm_is_enable, 0);

	INIT_DELAYED_WORK(&data->uv_sr_work_queue, uv_sr_set);

	/* Init Device */
	if (data->part_type < PART_TYPE_MAX86100A)
		err = max86000_init_device(data);
	else
		err = max86100_init_device(data);

	if (err) {
		pr_err("%s init device fail err = %d", __func__, err);
		goto max86000_init_device_failed;
	}

	dev_set_drvdata(data->dev, data);

	max86x00_init_agc_settings(data);

#ifdef CONFIG_SENSORS_MAX86X00_DEBUG
	pr_info("%s Debugging are enabled\n", __func__);
	max86x00_debug_init(data);
#else
	pr_info("%s Debugging are disabled\n", __func__);
	err = max86000_setup_irq(data);
	if (err) {
		pr_err("[SENSOR] %s - could not setup irq\n", __func__);
		goto max86000_init_device_failed;
	}
#endif
	err = max86000_write_reg(data, MAX86000_LED_CONFIGURATION, MAX86100_SHDN_MASK);
	if(err<0){
		pr_err("[SENSOR] %s - could enter shutdown mode\n", __func__);
	}

	pr_info("%s success\n", __func__);
	goto done;

max86000_init_device_failed:
	if (data->part_type != PART_TYPE_MAX86100LC)
		sensors_unregister(data->dev, uv_sensor_attrs);
uv_sensor_register_failed:
err_uv_sysfs_create_group:
	if (data->part_type != PART_TYPE_MAX86100LC)
		sensors_remove_symlink(data->uv_input_dev);
err_uv_sensors_create_symlink:
	if (data->part_type != PART_TYPE_MAX86100LC)
		input_unregister_device(data->uv_input_dev);
err_uv_input_register_device:
err_uv_input_allocate_device:
	sensors_unregister(data->dev, hrm_sensor_attrs);
hrm_sensor_register_failed:
err_hrm_sysfs_create_group:
	sensors_remove_symlink(data->hrm_input_dev);
err_hrm_sensors_create_symlink:
	input_unregister_device(data->hrm_input_dev);
err_hrm_input_register_device:
err_hrm_input_allocate_device:
	sensors_unregister(data->acfd_dev, acfd_sensor_attrs);
acfd_sensor_register_failed:
err_acfd_sysfs_create_group:
	sysfs_remove_group(&data->hrm_input_dev->dev.kobj,
				&acfd_attribute_group);
err_acfd_input_allocate_device:
err_adfd_input_register_device:
	input_free_device(data->acfd_input_dev);
err_of_read_chipid:
	max86000_regulator_onoff(data, HRM_LDO_OFF);
err_regulator_enable:
	gpio_free(data->hrm_int);
#ifdef CONFIG_SENSORS_MAX86X00_LED_POWER
	gpio_free(data->hrm_en);
#endif
err_setup_gpio:
err_of_node:
	mutex_destroy(&data->i2clock);
	mutex_destroy(&data->activelock);
	kfree(data);
	pr_err("%s failed\n", __func__);
done:
	return err;
}

/*
 *  Remove function for this I2C driver.
 */
int max86000_remove(struct i2c_client *client)
{
	struct max86x00_device_data *data = i2c_get_clientdata(client);
	pr_info("%s\n", __func__);

	if (atomic_read(&data->regulator_is_enable)) {
		if (data->part_type >= PART_TYPE_MAX86100A)
			max86100_disable(data);
		else
			max86000_disable(data);
	}

	if (atomic_read(&data->regulator_is_enable))
		max86000_regulator_onoff(data, HRM_LDO_OFF);
	if (gpio_is_valid(data->hrm_int))
		gpio_free(data->hrm_int);
#ifndef CONFIG_SENSORS_MAX86X00_LED_POWER
	regulator_put(data->regulator_led_3p3);
#endif
	regulator_put(data->regulator_vdd_1p8);
#ifdef CONFIG_SENSORS_MAX86X00_LED_POWER
	if (gpio_is_valid(data->hrm_en))
		gpio_free(data->hrm_en);
#endif

#ifdef CONFIG_SENSORS_MAX86X00_DEBUG
	max86x00_debug_remove();
#else
	free_irq(client->irq, data);
#endif
	sysfs_remove_group(&data->hrm_input_dev->dev.kobj,
				&acfd_attribute_group);
	input_unregister_device(data->acfd_input_dev);
	input_free_device(data->acfd_input_dev);
	sensors_unregister(data->acfd_dev, acfd_sensor_attrs);

	sysfs_remove_group(&data->hrm_input_dev->dev.kobj,
				&acfd_attribute_group);
	sysfs_remove_group(&data->hrm_input_dev->dev.kobj,
				&hrm_attribute_group);
	sensors_remove_symlink(data->hrm_input_dev);
	input_unregister_device(data->hrm_input_dev);
	input_free_device(data->hrm_input_dev);
	sensors_unregister(data->hrm_dev, hrm_sensor_attrs);

	if (data->part_type < PART_TYPE_MAX86100LC) {
		sysfs_remove_group(&data->uv_input_dev->dev.kobj,
					&uv_attribute_group);
		sensors_remove_symlink(data->uv_input_dev);
		input_unregister_device(data->uv_input_dev);
		input_free_device(data->uv_input_dev);
		sensors_unregister(data->uv_dev, uv_sensor_attrs);
	}

	mutex_destroy(&data->i2clock);
	mutex_destroy(&data->activelock);
	kfree(data);
	return 0;
}

static void max86000_shutdown(struct i2c_client *client)
{
	pr_info("%s\n", __func__);
}

static int max86000_pm_suspend(struct device *dev)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);
	if (data->part_type < PART_TYPE_MAX86100A) {
		if (atomic_read(&data->hrm_is_enable)) {
			max86000_hrm_mode_enable(data, HRM_LDO_OFF);
			atomic_set(&data->is_suspend, 1);
		} else if (atomic_read(&data->uv_is_enable)) {
			max86000_uv_mode_enable(data, HRM_LDO_OFF);
			atomic_set(&data->is_suspend, 2);
		}
	} else {
		if (atomic_read(&data->hrm_is_enable)) {
			max86100_hrm_mode_enable(data, HRM_LDO_OFF);
			atomic_set(&data->is_suspend, 1);
		} else if (atomic_read(&data->uv_is_enable)) {
			max86100_uv_mode_enable(data, HRM_LDO_OFF);
			atomic_set(&data->is_suspend, 2);
		}
	}
	pr_info("%s\n", __func__);
	return 0;
}

static int max86000_pm_resume(struct device *dev)
{
	struct max86x00_device_data *data = dev_get_drvdata(dev);
	if (data->part_type < PART_TYPE_MAX86100A) {
		if (atomic_read(&data->is_suspend) == 1) {
			max86000_hrm_mode_enable(data, HRM_LDO_ON);
			atomic_set(&data->is_suspend, 0);
		} else if (atomic_read(&data->is_suspend) == 2) {
			max86000_uv_mode_enable(data, HRM_LDO_ON);
			atomic_set(&data->is_suspend, 0);
		}
	} else {
		if (atomic_read(&data->is_suspend) == 1) {
			max86100_hrm_mode_enable(data, HRM_LDO_ON);
			atomic_set(&data->is_suspend, 0);
		} else if (atomic_read(&data->is_suspend) == 2) {
			max86100_uv_mode_enable(data, HRM_LDO_ON);
			atomic_set(&data->is_suspend, 0);
		}
	}
	pr_info("%s\n", __func__);
	return 0;
}

static const struct dev_pm_ops max86000_pm_ops = {
	.suspend = max86000_pm_suspend,
	.resume = max86000_pm_resume
};

static struct of_device_id max86000_match_table[] = {
	{ .compatible = MAX86X00_CHIP_NAME,},
	{},
};

static const struct i2c_device_id max86000_device_id[] = {
	{ MAX86X00_CHIP_NAME, 0 },
	{ }
};

/* descriptor of the max86000 I2C driver */
static struct i2c_driver max86000_i2c_driver = {
	.driver = {
		.name = MAX86X00_CHIP_NAME,
		.owner = THIS_MODULE,
		.pm = &max86000_pm_ops,
		.of_match_table = max86000_match_table,
	},
	.probe = max86000_probe,
	.shutdown = max86000_shutdown,
	.remove = max86000_remove,
	.id_table = max86000_device_id,
};

/* initialization and exit functions */
static int __init max86000_init(void)
{
	return i2c_add_driver(&max86000_i2c_driver);
}

static void __exit max86000_exit(void)
{
	i2c_del_driver(&max86000_i2c_driver);
}

module_init(max86000_init);
module_exit(max86000_exit);

MODULE_DESCRIPTION("Max86x00 Driver");
MODULE_AUTHOR("Maxim Integrated");
MODULE_LICENSE("GPL");
