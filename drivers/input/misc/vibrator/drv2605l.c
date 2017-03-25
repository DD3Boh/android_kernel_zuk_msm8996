/*
 ** =============================================================================
 ** Copyright (c) 2014  Texas Instruments Inc.
 **
 ** This program is free software; you can redistribute it and/or
 ** modify it under the terms of the GNU General Public License
 ** as published by the Free Software Foundation; either version 2
 ** of the License, or (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with this program; if not, write to the Free Software
 ** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **
 ** File:
 **     drv2605l.c
 **
 ** Description:
 **     DRV2605L chip driver
 **
 ** =============================================================================
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/semaphore.h>
#include <linux/device.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/spinlock_types.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/firmware.h>
#include "drv2605l.h"
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#define SKIP_AUTOCAL 0

static struct reg_default drv260x_reg_defs[] = {
	{ 0x01, 0x05 },
	{ 0x02, 0x00 },
	{ 0x04, 0x00 },
	{ 0x1a, 0xb6 },
	{ 0x1b, 0x80 },
	{ 0x1c, 0x30 },
	{ 0x1d, 0x42 },
	{ 0x1e, 0x30 },
	{ 0x20, 0x33 },
	{ 0x21, 0x00 },
	{ 0x22, 0x00 },
};

static struct drv2605l_data *drv2605l_data = NULL;

static unsigned int drv2605l_reg_read(struct drv2605l_data *haptics, unsigned int reg)
{
	unsigned int val;
	int ret;

	ret = regmap_read(haptics->regmap, reg, &val);

	if (ret < 0)
		return ret;
	else
		return val;
}

static int drv2605l_reg_write(struct drv2605l_data *haptics, unsigned char reg, unsigned int val)
{
	return regmap_write(haptics->regmap, reg, val);
}

static int drv2605l_bulk_read(struct drv2605l_data *haptics, unsigned char reg, unsigned int count, u8 *buf)
{
	return regmap_bulk_read(haptics->regmap, reg, buf, count);
}

static int drv2605l_bulk_write(struct drv2605l_data *haptics, unsigned char reg, unsigned int count, const u8 *buf)
{
	return regmap_bulk_write(haptics->regmap, reg, buf, count);
}

static int drv2605l_set_bits(struct drv2605l_data *haptics, unsigned char reg, unsigned char mask, unsigned int val)
{
	return regmap_update_bits(haptics->regmap, reg, mask, val);
}

static int drv2605l_set_go_bit(struct drv2605l_data *haptics, unsigned int val)
{
	return drv2605l_reg_write(haptics, GO_REG, (val&0x01));
}

#if SKIP_AUTOCAL == 0
static void drv2605l_poll_go_bit(struct drv2605l_data *haptics)
{
	while (drv2605l_reg_read(haptics, GO_REG) == GO)
		schedule_timeout_interruptible(msecs_to_jiffies(GO_BIT_POLL_INTERVAL));
}
#endif

static int drv2605l_set_rtp_val(struct drv2605l_data *haptics, unsigned int value)
{
	/* please be noted: in unsigned mode, maximum is 0xff, in signed mode, maximum is 0x7f */
	return drv2605l_reg_write(haptics, REAL_TIME_PLAYBACK_REG, value);
}

static int drv2605l_set_waveform_sequence(struct drv2605l_data *haptics, unsigned char* seq, unsigned int size)
{
	return drv2605l_bulk_write(haptics, WAVEFORM_SEQUENCER_REG, (size>WAVEFORM_SEQUENCER_MAX)?WAVEFORM_SEQUENCER_MAX:size, seq);
}

static void drv2605l_change_mode(struct drv2605l_data *haptics, char work_mode, char dev_mode)
{
	/* please be noted : LRA open loop cannot be used with analog input mode */
//	printk(KERN_DEBUG"%s:%x,%x\n", __FUNCTION__, (int)work_mode, (int)dev_mode);
	if (dev_mode == DEV_IDLE) {
		haptics->dev_mode = dev_mode;
		haptics->work_mode = work_mode;
	} else if (dev_mode == DEV_STANDBY) {
		if (haptics->dev_mode != DEV_STANDBY) {
			haptics->dev_mode = DEV_STANDBY;
			drv2605l_reg_write(haptics, MODE_REG, MODE_STANDBY);
			schedule_timeout_interruptible(msecs_to_jiffies(WAKE_STANDBY_DELAY));
		}
		haptics->work_mode = WORK_IDLE;
	} else if (dev_mode == DEV_READY) {
		if ((work_mode != haptics->work_mode)
				||(dev_mode != haptics->dev_mode)) {
			haptics->work_mode = work_mode;
			haptics->dev_mode = dev_mode;
			if((haptics->work_mode == WORK_VIBRATOR)
					||(haptics->work_mode == WORK_PATTERN_RTP_ON)
					||(haptics->work_mode == WORK_SEQ_RTP_ON)
					||(haptics->work_mode == WORK_RTP)){
			//	printk(KERN_DEBUG"%s:MODE_REAL_TIME_PLAYBACK\n", __FUNCTION__);
				drv2605l_reg_write(haptics, MODE_REG, MODE_REAL_TIME_PLAYBACK);
			} else if (haptics->work_mode == WORK_CALIBRATION) {
				drv2605l_reg_write(haptics, MODE_REG, AUTO_CALIBRATION);
			} else {
				drv2605l_reg_write(haptics, MODE_REG, MODE_INTERNAL_TRIGGER);
				schedule_timeout_interruptible(msecs_to_jiffies(STANDBY_WAKE_DELAY));
			}
		}
	}
}

static void play_effect(struct drv2605l_data *haptics)
{
	switch_set_state(&haptics->sw_dev, SW_STATE_SEQUENCE_PLAYBACK);
	drv2605l_change_mode(haptics, WORK_SEQ_PLAYBACK, DEV_READY);
	drv2605l_set_waveform_sequence(haptics, haptics->sequence, WAVEFORM_SEQUENCER_MAX);
	haptics->is_playing = YES;
	drv2605l_set_go_bit(haptics, GO);

	while ((drv2605l_reg_read(haptics, GO_REG) == GO) && (haptics->should_stop == NO)) {
		schedule_timeout_interruptible(msecs_to_jiffies(GO_BIT_POLL_INTERVAL));
	}

	if (haptics->should_stop == YES) {
		drv2605l_set_go_bit(haptics, STOP);
	}

	drv2605l_change_mode(haptics, WORK_IDLE, DEV_STANDBY);
	switch_set_state(&haptics->sw_dev, SW_STATE_IDLE);
	haptics->is_playing = NO;
	wake_unlock(&haptics->wk_lock);
}

static void play_pattern_rtp(struct drv2605l_data *haptics)
{
	if (haptics->work_mode == WORK_PATTERN_RTP_ON) {
		drv2605l_change_mode(haptics, WORK_PATTERN_RTP_OFF, DEV_READY);
		if (haptics->repeat_times == 0) {
			drv2605l_change_mode(haptics, WORK_IDLE, DEV_STANDBY);
			haptics->is_playing = NO;
			switch_set_state(&haptics->sw_dev, SW_STATE_IDLE);
			wake_unlock(&haptics->wk_lock);
		} else {
			hrtimer_start(&haptics->timer, ns_to_ktime((u64)haptics->silience_time * NSEC_PER_MSEC), HRTIMER_MODE_REL);
		}
	} else if (haptics->work_mode == WORK_PATTERN_RTP_OFF) {
		haptics->repeat_times--;
		drv2605l_change_mode(haptics, WORK_PATTERN_RTP_ON, DEV_READY);
		hrtimer_start(&haptics->timer, ns_to_ktime((u64)haptics->vibration_time * NSEC_PER_MSEC), HRTIMER_MODE_REL);
	}
}

static void play_seq_rtp(struct drv2605l_data *haptics)
{
	if (haptics->rtp_seq.rtp_index < haptics->rtp_seq.rtp_counts) {
		int rtp_time = haptics->rtp_seq.rtp_data[haptics->rtp_seq.rtp_index] >> 8;
		int rtp_val = haptics->rtp_seq.rtp_data[haptics->rtp_seq.rtp_index] & 0x00ff ;

		haptics->is_playing = YES;
		haptics->rtp_seq.rtp_index++;
		drv2605l_change_mode(haptics, WORK_SEQ_RTP_ON, DEV_READY);
		drv2605l_set_rtp_val(haptics,  rtp_val);

		hrtimer_start(&haptics->timer, ns_to_ktime((u64)rtp_time * NSEC_PER_MSEC), HRTIMER_MODE_REL);
	} else {
		drv2605l_change_mode(haptics, WORK_IDLE, DEV_STANDBY);
		haptics->is_playing = NO;
		switch_set_state(&haptics->sw_dev, SW_STATE_IDLE);
		wake_unlock(&haptics->wk_lock);
	}
}

static void vibrator_off(struct drv2605l_data *haptics)
{
	if (haptics->is_playing) {
		haptics->is_playing = NO;
		drv2605l_set_go_bit(haptics, STOP);
		drv2605l_change_mode(haptics, WORK_IDLE, DEV_STANDBY);
		switch_set_state(&haptics->sw_dev, SW_STATE_IDLE);
		wake_unlock(&haptics->wk_lock);
	}
}

static void drv2605l_stop(struct drv2605l_data *haptics)
{
	if (haptics->is_playing) {
		if ((haptics->work_mode == WORK_VIBRATOR)
				||(haptics->work_mode == WORK_PATTERN_RTP_ON)
				||(haptics->work_mode == WORK_PATTERN_RTP_OFF)
				||(haptics->work_mode == WORK_SEQ_RTP_ON)
				||(haptics->work_mode == WORK_SEQ_RTP_OFF)
				||(haptics->work_mode == WORK_RTP)) {
			vibrator_off(haptics);
		} else if (haptics->work_mode == WORK_SEQ_PLAYBACK) {
			//do nothing
		} else {
			printk("%s, err mode=%d \n", __FUNCTION__, haptics->work_mode);
		}
	}
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	struct drv2605l_data *haptics = container_of(dev, struct drv2605l_data, to_dev);

	if (hrtimer_active(&haptics->timer)) {
		ktime_t r = hrtimer_get_remaining(&haptics->timer);
		return ktime_to_ms(r);
	}

	return 0;
}

static void vibrator_enable( struct timed_output_dev *dev, int value)
{
	struct drv2605l_data *haptics = container_of(dev, struct drv2605l_data, to_dev);

	//printk(KERN_DEBUG"%s:Enter\n", __FUNCTION__);
	haptics->should_stop = YES;
	hrtimer_cancel(&haptics->timer);
	cancel_work_sync(&haptics->vibrator_work);

	mutex_lock(&haptics->lock);

	drv2605l_stop(haptics);
//	printk(KERN_DEBUG"%s:value:%d\n", __FUNCTION__, value);
	if (value > 0) {
		wake_lock(&haptics->wk_lock);

		drv2605l_change_mode(haptics, WORK_VIBRATOR, DEV_READY);
		haptics->is_playing = YES;
		switch_set_state(&haptics->sw_dev, SW_STATE_RTP_PLAYBACK);
		gpiod_set_value(haptics->platform_data.enable_gpio, 1);
		value = (value>MAX_TIMEOUT)?MAX_TIMEOUT:value;
		hrtimer_start(&haptics->timer, ns_to_ktime((u64)value * NSEC_PER_MSEC), HRTIMER_MODE_REL);
	}

	mutex_unlock(&haptics->lock);
//	printk(KERN_DEBUG"%s:Exit\n", __FUNCTION__);
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	struct drv2605l_data *haptics = container_of(timer, struct drv2605l_data, timer);

 	//printk(KERN_DEBUG"%s:Enter\n", __FUNCTION__);
	schedule_work(&haptics->vibrator_work);
	//printk(KERN_DEBUG"%s:Exit\n", __FUNCTION__);

	return HRTIMER_NORESTART;
}

static void vibrator_work_routine(struct work_struct *work)
{
	struct drv2605l_data *haptics = container_of(work, struct drv2605l_data, vibrator_work);

//	printk(KERN_DEBUG"%s:Enter\n", __FUNCTION__);

//	printk(KERN_DEBUG"%s:work_mode:%x\n", __FUNCTION__, (int)(haptics->work_mode));
	mutex_lock(&haptics->lock);
	if ((haptics->work_mode == WORK_VIBRATOR)
			||(haptics->work_mode == WORK_RTP)) {
		vibrator_off(haptics);
	} else if (haptics->work_mode == WORK_SEQ_PLAYBACK) {
		play_effect(haptics);
	} else if ((haptics->work_mode == WORK_PATTERN_RTP_ON)
			||(haptics->work_mode == WORK_PATTERN_RTP_OFF)) {
		play_pattern_rtp(haptics);
	} else if ((haptics->work_mode == WORK_SEQ_RTP_ON)
			||(haptics->work_mode == WORK_SEQ_RTP_OFF)) {
		play_seq_rtp(haptics);
	}
	mutex_unlock(&haptics->lock);

	//printk(KERN_DEBUG"%s:Exit\n", __FUNCTION__);
}

static int fw_chksum(const struct firmware *fw){
	int sum = 0;
	int i=0;
	int size = fw->size;
	const unsigned char *pbuf = fw->data;

	for (i=0; i< size; i++) {
		if((i>11) && (i<16)) {

		} else {
			sum += pbuf[i];
		}
	}

	return sum;
}

/* drv2605l_firmware_load:   This function is called by the
 *		request_firmware_nowait function as soon
 *		as the firmware has been loaded from the file.
 *		The firmware structure contains the data and$
 *		the size of the firmware loaded.
 * @fw: pointer to firmware file to be dowloaded
 * @context: pointer variable to drv2605l_data
 *
 *
 */
static void drv2605l_firmware_load(const struct firmware *fw, void *context)
{
	struct drv2605l_data *haptics = context;
	int size = 0, fwsize = 0, i=0;
	const unsigned char *pbuf = NULL;

	if (fw != NULL) {
		pbuf = fw->data;
		size = fw->size;

		memcpy(&(haptics->fw_header), pbuf, sizeof(struct drv2605l_fw_header));
		if ((haptics->fw_header.fw_magic != DRV2605L_MAGIC)
				||(haptics->fw_header.fw_size != size)
				||(haptics->fw_header.fw_chksum != fw_chksum(fw))) {
			printk("%s, ERROR!! firmware not right:Magic=0x%x,Size=%d,chksum=0x%x\n",
					__FUNCTION__, haptics->fw_header.fw_magic,
					haptics->fw_header.fw_size, haptics->fw_header.fw_chksum);
		} else {
			printk("%s, firmware good\n", __FUNCTION__);

			drv2605l_change_mode(haptics, WORK_IDLE, DEV_READY);

			pbuf += sizeof(struct drv2605l_fw_header);

			drv2605l_reg_write(haptics, RAM_ADDR_UPPER_BYTE_REG, 0);
			drv2605l_reg_write(haptics, RAM_ADDR_LOWER_BYTE_REG, 0);

			fwsize = size - sizeof(struct drv2605l_fw_header);
			for (i = 0; i < fwsize; i++) {
				drv2605l_reg_write(haptics, RAM_DATA_REG, pbuf[i]);
			}

			drv2605l_change_mode(haptics, WORK_IDLE, DEV_STANDBY);
		}
	} else {
		printk("%s, ERROR!! firmware not found\n", __FUNCTION__);
	}
}

static int dev2605l_open (struct inode * i_node, struct file * filp)
{
	if(drv2605l_data == NULL) {
		return -ENODEV;
	}

	filp->private_data = drv2605l_data;
	return 0;
}

static ssize_t dev2605l_read(struct file* filp, char* buff, size_t length, loff_t* offset)
{
	struct drv2605l_data *haptics = (struct drv2605l_data *)filp->private_data;
	int ret = 0;

	if (haptics->r_len > 0) {
		ret = copy_to_user(buff, haptics->r_buff, haptics->r_len);
		if (ret != 0) {
			printk("%s, copy_to_user err=%d \n", __FUNCTION__, ret);
		} else {
			ret = haptics->r_len;
		}
		haptics->r_len = 0;
	} else {
		printk("%s, nothing to read\n", __FUNCTION__);
	}

	return ret;
}

static bool is_for_debug(int cmd){
	return ((cmd == HAPTIC_CMDID_REG_WRITE)
			||(cmd == HAPTIC_CMDID_REG_READ)
			||(cmd == HAPTIC_CMDID_REG_SETBIT));
}

static ssize_t dev2605l_write(struct file* filp, const char* buff, size_t len, loff_t* off)
{
	struct drv2605l_data *haptics = (struct drv2605l_data *)filp->private_data;

	if (is_for_debug(buff[0])) {
		//do nothing
	} else {
		haptics->should_stop = YES;
		hrtimer_cancel(&haptics->timer);
		cancel_work_sync(&haptics->vibrator_work);
	}

	mutex_lock(&haptics->lock);

	if (is_for_debug(buff[0])) {
		//do nothing
	} else {
		drv2605l_stop(haptics);
	}

	switch (buff[0]) {
	case HAPTIC_CMDID_PLAY_SINGLE_EFFECT:
	case HAPTIC_CMDID_PLAY_EFFECT_SEQUENCE: {
		memset(&haptics->sequence, 0, WAVEFORM_SEQUENCER_MAX);
		if (!copy_from_user(&haptics->sequence, &buff[1], len - 1))
		{
			wake_lock(&haptics->wk_lock);

			haptics->should_stop = NO;
			drv2605l_change_mode(haptics, WORK_SEQ_PLAYBACK, DEV_IDLE);
			schedule_work(&haptics->vibrator_work);
		}
		break;
	}
	case HAPTIC_CMDID_PLAY_TIMED_EFFECT: {
		unsigned int value = 0;
		value = buff[2];
		value <<= 8;
		value |= buff[1];

		if (value > 0) {
			wake_lock(&haptics->wk_lock);
			switch_set_state(&haptics->sw_dev, SW_STATE_RTP_PLAYBACK);
			haptics->is_playing = YES;
			value = (value > MAX_TIMEOUT)?MAX_TIMEOUT:value;
			drv2605l_change_mode(haptics, WORK_RTP, DEV_READY);

			hrtimer_start(&haptics->timer, ns_to_ktime((u64)value * NSEC_PER_MSEC), HRTIMER_MODE_REL);
		}
		break;
	}
	case HAPTIC_CMDID_PATTERN_RTP: {
		unsigned char strength = 0;

		haptics->vibration_time = (int)((((int)buff[2])<<8) | (int)buff[1]);
		haptics->silience_time = (int)((((int)buff[4])<<8) | (int)buff[3]);
		strength = buff[5];
		haptics->repeat_times = buff[6];

		if (haptics->vibration_time > 0) {
			wake_lock(&haptics->wk_lock);
			switch_set_state(&haptics->sw_dev, SW_STATE_RTP_PLAYBACK);
			haptics->is_playing = YES;
			if (haptics->repeat_times > 0)
				haptics->repeat_times--;
			if (haptics->vibration_time > MAX_TIMEOUT)
				haptics->vibration_time = MAX_TIMEOUT;
			drv2605l_change_mode(haptics, WORK_PATTERN_RTP_ON, DEV_READY);
			drv2605l_set_rtp_val(haptics, strength);

			hrtimer_start(&haptics->timer, ns_to_ktime((u64)haptics->vibration_time * NSEC_PER_MSEC), HRTIMER_MODE_REL);
		}
		break;
	}
	case HAPTIC_CMDID_RTP_SEQUENCE: {
		memset(&haptics->rtp_seq, 0, sizeof(struct rtp_seq));
		if (((len-1)%2) == 0) {
			haptics->rtp_seq.rtp_counts = (len-1)/2;
			if ((haptics->rtp_seq.rtp_counts <= MAX_RTP_SEQ)
				&& (haptics->rtp_seq.rtp_counts>0)) {
				if (copy_from_user(haptics->rtp_seq.rtp_data, &buff[1], haptics->rtp_seq.rtp_counts*2) != 0) {
					printk("%s, rtp_seq copy seq err\n", __FUNCTION__);
					break;
				}

				wake_lock(&haptics->wk_lock);
				switch_set_state(&haptics->sw_dev, SW_STATE_RTP_PLAYBACK);
				drv2605l_change_mode(haptics, WORK_SEQ_RTP_OFF, DEV_IDLE);
				schedule_work(&haptics->vibrator_work);
			} else {
				printk("%s, rtp_seq count error,maximum=%d\n", __FUNCTION__,MAX_RTP_SEQ);
			}
		} else {
			printk("%s, rtp_seq len error\n", __FUNCTION__);
		}
		break;
	}
	case HAPTIC_CMDID_STOP: {
		break;
	}
	case HAPTIC_CMDID_UPDATE_FIRMWARE: {
		struct firmware fw;
		unsigned char *fw_buffer = (unsigned char *)kzalloc(len-1, GFP_KERNEL);
		int result = -1;

		if (fw_buffer != NULL) {
			fw.size = len-1;

			wake_lock(&haptics->wk_lock);
			result = copy_from_user(fw_buffer, &buff[1], fw.size);
			if (result == 0) {
				printk("%s, fwsize=%d, f:%x, l:%x\n", __FUNCTION__, (int)fw.size, buff[1], buff[len-1]);
				fw.data = (const unsigned char *)fw_buffer;
				drv2605l_firmware_load(&fw, (void *)haptics);
			}
			wake_unlock(&haptics->wk_lock);

			kfree(fw_buffer);
		}
		break;
	}

	case HAPTIC_CMDID_READ_FIRMWARE: {
		int i;
		if (len == 3) {
			haptics->r_len = 1;
			drv2605l_reg_write(haptics, RAM_ADDR_UPPER_BYTE_REG, buff[2]);
			drv2605l_reg_write(haptics, RAM_ADDR_LOWER_BYTE_REG, buff[1]);
			haptics->r_buff[0] = drv2605l_reg_read(haptics, RAM_DATA_REG);
		} else if (len == 4) {
			drv2605l_reg_write(haptics, RAM_ADDR_UPPER_BYTE_REG, buff[2]);
			drv2605l_reg_write(haptics, RAM_ADDR_LOWER_BYTE_REG, buff[1]);
			haptics->r_len = (buff[3]>MAX_READ_BYTES)?MAX_READ_BYTES:buff[3];
			for(i=0; i < haptics->r_len; i++){
				haptics->r_buff[i] = drv2605l_reg_read(haptics, RAM_DATA_REG);
			}
		} else {
			printk("%s, read fw len error\n", __FUNCTION__);
		}
		break;
	}
	case HAPTIC_CMDID_REG_READ: {
		if (len == 2) {
			haptics->r_len = 1;
			haptics->r_buff[0] = drv2605l_reg_read(haptics, buff[1]);
		} else if (len == 3) {
			haptics->r_len = (buff[2]>MAX_READ_BYTES)?MAX_READ_BYTES:buff[2];
			drv2605l_bulk_read(haptics, buff[1], haptics->r_len, haptics->r_buff);
		} else {
			printk("%s, reg_read len error\n", __FUNCTION__);
		}
		break;
	}
	case HAPTIC_CMDID_REG_WRITE: {
		if ((len-1) == 2) {
			drv2605l_reg_write(haptics, buff[1], buff[2]);
		} else if ((len-1)>2) {
			unsigned char *data = (unsigned char *)kzalloc(len-2, GFP_KERNEL);
			if (data != NULL) {
				if (copy_from_user(data, &buff[2], len-2) != 0) {
					printk("%s, reg copy err\n", __FUNCTION__);
				} else {
					drv2605l_bulk_write(haptics, buff[1], len-2, data);
				}
				kfree(data);
			}
		} else {
			printk("%s, reg_write len error\n", __FUNCTION__);
		}
		break;
	}

	case HAPTIC_CMDID_REG_SETBIT: {
		int i=1;
		for (i=1; i< len; ) {
			drv2605l_set_bits(haptics, buff[i], buff[i+1], buff[i+2]);
			i += 3;
		}
		break;
	}
	default:
		printk("%s, unknown HAPTIC cmd\n", __FUNCTION__);
		break;
	}

	mutex_unlock(&haptics->lock);

	return len;
}


static struct file_operations fops =
{
	.open = dev2605l_open,
	.read = dev2605l_read,
	.write = dev2605l_write,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
void drv2605l_early_suspend(struct early_suspend *h){
	struct drv2605l_data *haptics = container_of(h, struct drv2605l_data, early_suspend);

	haptics->should_stop = YES;
	hrtimer_cancel(&haptics->timer);
	cancel_work_sync(&haptics->vibrator_work);

	mutex_lock(&haptics->lock);

	drv2605l_stop(haptics);

	mutex_unlock(&haptics->lock);
	return ;
}

void drv2605l_late_resume(struct early_suspend *h) {
	struct drv2605l_data *haptics = container_of(h, struct drv2605l_data, early_suspend);

	mutex_lock(&haptics->lock);
	mutex_unlock(&haptics->lock);
	return ;
}
#endif

static int drv2605l_init(struct drv2605l_data *haptics)
{
	int reval = -ENOMEM;

	haptics->version = MKDEV(0,0);
	reval = alloc_chrdev_region(&haptics->version, 0, 1, "drv2605l");
	if (reval < 0) {
		printk(KERN_ALERT"drv2605: error getting major number %d\n", reval);
		goto fail0;
	}

	haptics->class = class_create(THIS_MODULE, "drv260x");
	if (!haptics->class) {
		printk(KERN_ALERT"drv2605: error creating class\n");
		goto fail1;
	}

	haptics->device = device_create(haptics->class, NULL, haptics->version, NULL, "drv2605l");
	if (!haptics->device) {
		printk(KERN_ALERT"drv2605: error creating device 2605\n");
		goto fail2;
	}

	cdev_init(&haptics->cdev, &fops);
	haptics->cdev.owner = THIS_MODULE;
	haptics->cdev.ops = &fops;
	reval = cdev_add(&haptics->cdev, haptics->version, 1);
	if (reval) {
		printk(KERN_ALERT"drv2605: fail to add cdev\n");
		goto fail3;
	}

	haptics->sw_dev.name = "haptics";
	reval = switch_dev_register(&haptics->sw_dev);
	if (reval < 0) {
		printk(KERN_ALERT"drv2605: fail to register switch\n");
		goto fail4;
	}

	haptics->to_dev.name = "vibrator";
	haptics->to_dev.get_time = vibrator_get_time;
	haptics->to_dev.enable = vibrator_enable;

	if (timed_output_dev_register(&(haptics->to_dev)) < 0) {
		printk(KERN_ALERT"drv2605: fail to create timed output dev\n");
		goto fail3;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	haptics->early_suspend.suspend = drv2605l_early_suspend;
	haptics->early_suspend.resume = drv2605l_late_resume;
	haptics->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1;
	register_early_suspend(&haptics->early_suspend);
#endif

	hrtimer_init(&haptics->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	haptics->timer.function = vibrator_timer_func;
	INIT_WORK(&haptics->vibrator_work, vibrator_work_routine);

	wake_lock_init(&haptics->wk_lock, WAKE_LOCK_SUSPEND, "vibrator");
	mutex_init(&haptics->lock);

	return 0;

fail4:
	switch_dev_unregister(&haptics->sw_dev);
fail3:
	device_destroy(haptics->class, haptics->version);
fail2:
	class_destroy(haptics->class);
fail1:
	unregister_chrdev_region(haptics->version, 1);
fail0:
	return reval;
}

static void dev_init_platform_data(struct drv2605l_data *haptics)
{
	struct drv2605l_platform_data *pdata = &haptics->platform_data;
	struct actuator_data actuator = pdata->actuator;
	unsigned char ctrlx_temp = 0;
	//otp memory saves data from 0x16 to 0x1a
	if (haptics->otp == 0) {
		if (actuator.rated_voltage != 0) {
			drv2605l_reg_write(haptics, RATED_VOLTAGE_REG, actuator.rated_voltage);
		} else {
			printk("%s, ERROR Rated ZERO\n", __FUNCTION__);
		}

		if(actuator.overdrive_voltage != 0) {
			drv2605l_reg_write(haptics, OVERDRIVE_CLAMP_VOLTAGE_REG, actuator.overdrive_voltage);
		} else {
			printk("%s, ERROR OverDriveVol ZERO\n", __FUNCTION__);
		}

		drv2605l_set_bits(haptics,
				FEEDBACK_CONTROL_REG,
				FEEDBACK_CONTROL_DEVICE_TYPE_MASK
				|FEEDBACK_CONTROL_FB_BRAKE_MASK
				|FEEDBACK_CONTROL_LOOP_GAIN_MASK,
				(((actuator.device_type == LRA)?FEEDBACK_CONTROL_MODE_LRA:FEEDBACK_CONTROL_MODE_ERM)
				 |FB_BRAKE_FACTOR
				 |LOOP_GAIN)
				);
	} else {
		printk("%s, otp programmed\n", __FUNCTION__);
	}

	if (actuator.device_type == LRA) {
		unsigned char drive_time = 5*(1000 - actuator.lra_freq)/actuator.lra_freq;
		drv2605l_set_bits(haptics,
				CTRL1_REG,
				CTRL1_REG_DRIVE_TIME_MASK,
				drive_time);
		printk("%s, LRA = %d, drive_time=0x%x\n", __FUNCTION__, actuator.lra_freq, drive_time);
	}

	if (pdata->loop_mode == OPEN_LOOP) {
		ctrlx_temp = BIDIR_INPUT_BIDIRECTIONAL;
	} else {
		if(pdata->input_mode == UNIDIRECTIONAL) {
			ctrlx_temp = BIDIR_INPUT_UNIDIRECTIONAL;
		} else {
			ctrlx_temp = BIDIR_INPUT_BIDIRECTIONAL;
		}
	}

	drv2605l_set_bits(haptics,
			CTRL2_REG,
			CTRL2_REG_BIDIR_INPUT_MASK|BLANKING_TIME_MASK|IDISS_TIME_MASK,
			ctrlx_temp|BLANKING_TIME|IDISS_TIME);

	if ((pdata->loop_mode == CLOSE_LOOP)&&(actuator.device_type == LRA)) {
		drv2605l_set_bits(haptics,
				CTRL2_REG,
				AUTO_RES_SAMPLE_TIME_MASK,
				AUTO_RES_SAMPLE_TIME_300us);
	}

	if ((pdata->loop_mode == OPEN_LOOP)&&(actuator.device_type == LRA)) {
		ctrlx_temp = LRA_OpenLoop_Enabled;
	} else if ((pdata->loop_mode == OPEN_LOOP)&&(actuator.device_type == ERM)) {
		ctrlx_temp = ERM_OpenLoop_Enabled;
	} else {
		ctrlx_temp = ERM_OpenLoop_Disable|LRA_OpenLoop_Disable;
	}

	if ((pdata->loop_mode == CLOSE_LOOP)
		&& (pdata->input_mode == UNIDIRECTIONAL)) {
		ctrlx_temp |= RTP_FORMAT_UNSIGNED;
		drv2605l_reg_write(haptics, REAL_TIME_PLAYBACK_REG, 0xff);
	} else {
		if(pdata->rtp_format == SIGNED) {
			ctrlx_temp |= RTP_FORMAT_SIGNED;
			drv2605l_reg_write(haptics, REAL_TIME_PLAYBACK_REG, 0x7f);
		} else {
			ctrlx_temp |= RTP_FORMAT_UNSIGNED;
			drv2605l_reg_write(haptics, REAL_TIME_PLAYBACK_REG, 0xff);
		}
	}

	drv2605l_set_bits(haptics,
			CTRL3_REG,
			CTRL3_REG_LOOP_MASK|CTRL3_REG_FORMAT_MASK,
			ctrlx_temp);

	drv2605l_set_bits(haptics,
			CTRL4_REG,
			CTRL4_REG_CAL_TIME_MASK|CTRL4_REG_ZC_DET_MASK,
			AUTO_CAL_TIME|ZC_DET_TIME
			);

	drv2605l_set_bits(haptics,
			CTRL5_REG,
			BLANK_IDISS_MSB_MASK,
			BLANK_IDISS_MSB_CLEAR
			);

	if (actuator.device_type == LRA) {
		/* please refer to the equations in DRV2605L data sheet */
		unsigned int ctrlx_temp = 9846 * actuator.lra_freq;
		unsigned int reg_20 = (unsigned int)(100000000 / ctrlx_temp);
		drv2605l_reg_write(haptics, LRA_OPENLOOP_PERIOD_REG, reg_20);
	}
}

#if SKIP_AUTOCAL == 0
static int dev_auto_calibrate(struct drv2605l_data *haptics)
{
	int err = 0, status=0;

	drv2605l_change_mode(haptics, WORK_CALIBRATION, DEV_READY);
	drv2605l_set_go_bit(haptics, GO);

	/* Wait until the procedure is done */
	drv2605l_poll_go_bit(haptics);
	/* Read status */
	status = drv2605l_reg_read(haptics, STATUS_REG);

	printk("%s, calibration status =0x%x\n", __FUNCTION__, status);

	/* Read calibration results */
	drv2605l_reg_read(haptics, AUTO_CALI_RESULT_REG);
	drv2605l_reg_read(haptics, AUTO_CALI_BACK_EMF_RESULT_REG);
	drv2605l_reg_read(haptics, FEEDBACK_CONTROL_REG);

	return err;
}
#endif

#define DRV260X_MAX_REG 0x23
static struct regmap_config drv2605l_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = DRV260X_MAX_REG,
	.reg_defaults = drv260x_reg_defs,
	.num_reg_defaults = ARRAY_SIZE(drv260x_reg_defs),
	.cache_type = REGCACHE_NONE,
};
/*
static void HapticsFirmwareLoad(const struct firmware *fw, void *context)
{
	drv2605l_firmware_load(fw, context);
	release_firmware(fw);
}
*/

/**
 * Rated and Overdriver Voltages:
 * Calculated using the formula r = v * 255 / 5.6
 * where r is what will be written to the register
 * and v is the rated or overdriver voltage of the actuator
 **/
static int drv2605l_calculate_voltage(unsigned int voltage)
{
	return (voltage * 255 / 5600);
}

#ifdef CONFIG_OF
static int drv2605l_parse_dt(struct device *dev,
		struct drv2605l_data *haptics)
{
	struct device_node *np = dev->of_node;
	struct drv2605l_platform_data *pdata = &haptics->platform_data;
	unsigned int voltage;
	int error;

	error = of_property_read_u32(np, "input-mode", &pdata->input_mode);
	if (error) {
		dev_err(dev, "%s: No entry for input mode\n", __func__);
		return error;
	}

	error = of_property_read_u32(np, "loop-mode", &pdata->loop_mode);
	if (error) {
		dev_err(dev, "%s: No entry for loop mode\n", __func__);
		return error;
	}

	error = of_property_read_u32(np, "rtp-format", &pdata->rtp_format);
	if (error) {
		dev_err(dev, "%s: No entry for mode\n", __func__);
		return error;
	}

	error = of_property_read_u32(np, "vib-rated-mv", &voltage);
	if (!error)
		pdata->actuator.rated_voltage = drv2605l_calculate_voltage(voltage);

	error = of_property_read_u32(np, "vib-overdrive-mv", &voltage);
	if (!error)
		pdata->actuator.overdrive_voltage= drv2605l_calculate_voltage(voltage);

	error = of_property_read_u32(np, "frequency", &pdata->actuator.lra_freq);
	if (error) {
		dev_err(dev, "%s: No entry for frequency\n", __func__);
		return error;
	}

	error = of_property_read_u32(np, "actuator-type", &pdata->actuator.device_type);
	if (error) {
		dev_err(dev, "%s: No entry for actuator type\n", __func__);
		return error;
	}

	return 0;
}
#else
static inline int drv2605l_parse_dt(struct device *dev,
		struct drv2605l_data *haptics)
{
	dev_err(dev, "no platform data defined\n");

	return -EINVAL;
}
#endif

static int drv2605l_probe(struct i2c_client* client, const struct i2c_device_id* id)
{
	struct drv2605l_data *haptics;
	struct drv2605l_platform_data *pdata = dev_get_platdata(&client->dev);
	unsigned int reg_val = 0;
	int err = 0;

	printk(KERN_DEBUG"%s:Enter\n", __FUNCTION__);

	haptics = devm_kzalloc(&client->dev, sizeof(struct drv2605l_data), GFP_KERNEL);
	if (haptics == NULL) {
		printk(KERN_ERR"%s:no memory\n", __FUNCTION__);
		return -ENOMEM;
	}

	if (pdata) {
		memcpy(&haptics->platform_data, pdata, sizeof(struct drv2605l_platform_data));
	}
	else if (client->dev.of_node) {
		err = drv2605l_parse_dt(&client->dev, haptics);
		if (err)
			return err;
	}

	haptics->platform_data.enable_gpio = devm_gpiod_get(&client->dev, "enable");
	if (IS_ERR(haptics->platform_data.enable_gpio)) {
		err = PTR_ERR(haptics->platform_data.enable_gpio);
		if (err != -ENOENT && err != -ENOSYS)
			return err;
		haptics->platform_data.enable_gpio = NULL;
	} else {
		gpiod_direction_output(haptics->platform_data.enable_gpio, 1);
		udelay(250);
	}

	haptics->regmap = devm_regmap_init_i2c(client, &drv2605l_i2c_regmap);
	if (IS_ERR(haptics->regmap)) {
		err = PTR_ERR(haptics->regmap);
		printk(KERN_ERR"%s:Failed to allocate register map: %d\n",__FUNCTION__,err);
		return err;
	}

	i2c_set_clientdata(client,haptics);

	reg_val = drv2605l_reg_read(haptics, STATUS_REG);
	printk("%s, status=%d\n", __FUNCTION__, reg_val);

	/* Read device ID */
	haptics->device_id = (reg_val & DEV_ID_MASK);
	switch (haptics->device_id)	{
		case DRV2605_VER_1DOT1:
			printk("drv2604 driver found: drv2605 v1.1.\n");
			break;
		case DRV2605_VER_1DOT0:
			printk("drv2604 driver found: drv2605 v1.0.\n");
			break;
		case DRV2604:
			printk(KERN_ALERT"drv2604 driver found: drv2604.\n");
			break;
		case DRV2604L:
			printk(KERN_ALERT"drv2604 driver found: drv2604L.\n");
			break;
		case DRV2605L:
			printk(KERN_ALERT"drv2604 driver found: drv2605L.\n");
			break;
		default:
			printk(KERN_ERR"drv2604 driver found: unknown.\n");
			break;
	}
/*
	if(haptics->device_id != DRV2604L){
		printk("%s, status(0x%x),device_id(%d) fail\n",
				__FUNCTION__, status, haptics->device_id);
		goto exit_gpio_request_failed;
	}else{
		err = request_firmware_nowait(THIS_MODULE,
				FW_ACTION_HOTPLUG,
				"drv2604.bin",
				&(client->dev),
				GFP_KERNEL,
				haptics,
				HapticsFirmwareLoad);
	}
*/
	drv2605l_change_mode(haptics, WORK_IDLE, DEV_READY);
	schedule_timeout_interruptible(msecs_to_jiffies(STANDBY_WAKE_DELAY));

	haptics->otp = drv2605l_reg_read(haptics, CTRL4_REG) & CTRL4_REG_OTP_MASK;

	dev_init_platform_data(haptics);

#if SKIP_AUTOCAL == 0
	if (haptics->otp == 0) {
		err = dev_auto_calibrate(haptics);
		if (err < 0) {
			printk("%s, ERROR, calibration fail\n",	__FUNCTION__);
		}
	}
#endif

	/* Put hardware in standby */
	drv2605l_change_mode(haptics, WORK_IDLE, DEV_STANDBY);

	drv2605l_init(haptics);

	drv2605l_data = haptics;
	printk("drv2605 probe succeeded\n");

	return 0;
}

static int drv2605l_remove(struct i2c_client* client)
{
	struct drv2605l_data *haptics = i2c_get_clientdata(client);

	device_destroy(haptics->class, haptics->version);
	class_destroy(haptics->class);
	unregister_chrdev_region(haptics->version, 1);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&haptics->early_suspend);
#endif
	gpiod_set_value(haptics->platform_data.enable_gpio, 0);
	printk(KERN_ALERT"drv2605 remove");

	return 0;
}

static struct i2c_device_id drv2605l_id_table[] =
{
	{ HAPTICS_DEVICE_NAME, 0 },
	{}
};

MODULE_DEVICE_TABLE(i2c, drv2605l_id_table);

#ifdef CONFIG_OF
static const struct of_device_id drv260x_of_match[] = {
	{ .compatible = "ti,drv2605l", },
	{ }
};
MODULE_DEVICE_TABLE(of, drv260x_of_match);
#endif

static struct i2c_driver drv2605l_driver =
{
	.driver = {
		.name = "drv2605l-haptics",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(drv260x_of_match),
	},
	.id_table = drv2605l_id_table,
	.probe = drv2605l_probe,
	.remove = drv2605l_remove,
};

static int __init drv2605x_init(void)
{
	return i2c_add_driver(&drv2605l_driver);
}

static void __exit drv2605x_exit(void)
{
	i2c_del_driver(&drv2605l_driver);
}

module_init(drv2605x_init);
module_exit(drv2605x_exit);

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("Driver for "HAPTICS_DEVICE_NAME);
