//
// pi5usb30216.c
//
// Drivers for usb type-C interface's CC-Logic chip of Pericom
//
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
#include <linux/wakelock.h>
#include "cclogic-core.h"


#define DRIVER_NAME "pericom,pi5usb30216"


#define MAX_RETRIES_FOR_PLUGIN_SLOW    4     //about 2 seconds

#define REG_BASE 		1
#define MAX_REG_NUM             4

#define REG_ID			(0x01-REG_BASE)
#define VERSION_ID      	(0x00<<3)
#define VENDOR_ID		(0x00<<0)
#define PIUSB30216_DEVID   	(VERSION_ID|VENDOR_ID)

#define REG_CONTROL 		(0x02-REG_BASE)
#define POWERSAVING_MODE     	(0x01<<7)
#define PORT_SETTING_INTERRUPT_NOMASK   (0x00)
#define CHARGING_CURRENT_MODE_DEFAULT   (0x00<<3)
#define CHARGING_CURRENT_MODE_MEDIUM    (0x01<<3)
#define CHARGING_CURRENT_MODE_HIGH      (0x02<<3)
#define PORT_SETTING_DEVICE		(0x00<<1)
#define PORT_SETTING_HOST		(0x01<<1)
#define PORT_SETTING_DUALROLE           (0x02<<1)
#define PORT_SETTING_INTERRUPT_MASK     (0x01)
#define PORT_SETTING_INTERRUPT_NOMASK   (0x00)

#define REG_INTERRUPT	(0x03-REG_BASE)
#define INTERRUPT_STATE_ATTACHED_BIT    (0x01)
#define INTERRUPT_STATE_DETACHED_BIT	(0x02)

#define REG_CC_STATE	(0x04-REG_BASE)
#define CCSTATUS_VBUS_BIT              	(0x01<<7)
#define CCSTATUS_CHARGING_CURRENT_BIT   (0x03<<5)
#define CCSTATUS_CHARGING_CURRENT_STANDBY   (0x00<<5)
#define CCSTATUS_CHARGING_CURRENT_DEFAULT   (0x01<<5)
#define CCSTATUS_CHARGING_CURRENT_MEDIUM    (0x02<<5)
#define CCSTATUS_CHARGING_CURRENT_HIGH      (0x03<<5)

#define CCSTATUS_ATTACHED_PORT_STATUS_BIT   (0x07<<2)
#define CCSTATUS_ATTACHED_PORT_STATUS_STANDBY   (0x00<<2)
#define CCSTATUS_ATTACHED_PORT_STATUS_DEVICE    (0x01<<2)
#define CCSTATUS_ATTACHED_PORT_STATUS_HOST      (0x02<<2)
#define CCSTATUS_ATTACHED_PORT_STATUS_AUDIO     (0x03<<2)
#define CCSTATUS_ATTACHED_PORT_STATUS_DEBUG     (0x04<<2)
#define CCSTATUS_ATTACHED_PORT_STATUS_VBUS_CC1_CC2     (0x05<<2)

#define CCSTATUS_PLUG_POLARITY_BIT		(0x03)
#define CCSTATUS_PLUG_POLARITY_STANDBY		(0x00)
#define CCSTATUS_PLUG_POLARITY_CC1   		(0x01)
#define CCSTATUS_PLUG_POLARITY_CC2  		(0x02)
#define CCSTATUS_PLUG_POLARITY_CC1_CC2		(0x03)



/**
 * pi5usb30216_write_i2c()
 */
static int pi5usb30216_write_i2c(struct i2c_client *client, u8 reg, u8 data)
{
	int ret = 0;
	char val[MAX_REG_NUM] = {0};

	pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

	val[reg] = data;

	ret = i2c_master_send(client, val, 2); 
	if (ret < 0) {
		dev_err(&client->dev,"cclogic:%s-->i2c send error\n",__func__);
		return ret;
	}

	return 0;

}
/**
 * pi5usb30216_parse_cclogic_state()
 */
static void pi5usb30216_parse_cclogic_state(int reg3, int reg4, 
					struct cclogic_state *result)
{

	pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

	result->vbus = false;
	result->cc = CCLOGIC_CC_UNKNOWN;
	result->evt = CCLOGIC_EVENT_NONE;
	result->device = CCLOGIC_DEVICE_UNKNOWN;
	result->charger = CCLOGIC_CURRENT_NONE;

	if((reg3&INTERRUPT_STATE_DETACHED_BIT) && 
		(reg3&INTERRUPT_STATE_ATTACHED_BIT)){	
		pr_err("cclogic:%s-->detach and attach in the same time\n",
				__func__);
	}else{
		if(reg3&INTERRUPT_STATE_DETACHED_BIT){
			result->evt = CCLOGIC_EVENT_DETACHED;
		}
		if(reg3&INTERRUPT_STATE_ATTACHED_BIT){
			result->evt = CCLOGIC_EVENT_ATTACHED;
		}
	}	

	if(reg4&CCSTATUS_VBUS_BIT){
		result->vbus = true;
	}

	switch(reg4&CCSTATUS_CHARGING_CURRENT_BIT){
		case CCSTATUS_CHARGING_CURRENT_STANDBY:
			result->charger = CCLOGIC_CURRENT_NONE;
			break;
		case CCSTATUS_CHARGING_CURRENT_DEFAULT:
			result->charger = CCLOGIC_CURRENT_DEFAULT;
			break;
		case CCSTATUS_CHARGING_CURRENT_MEDIUM:
			result->charger = CCLOGIC_CURRENT_MEDIUM;
			break;
		case CCSTATUS_CHARGING_CURRENT_HIGH:
			result->charger = CCLOGIC_CURRENT_HIGH;
			break;
	}

	switch(reg4&CCSTATUS_PLUG_POLARITY_BIT){
		case CCSTATUS_PLUG_POLARITY_CC1:
			result->cc = CCLOGIC_CC1;
			break;
		case CCSTATUS_PLUG_POLARITY_CC2:
			result->cc = CCLOGIC_CC2;
			break;
		case CCSTATUS_PLUG_POLARITY_STANDBY:
		case CCSTATUS_PLUG_POLARITY_CC1_CC2:
		default:
			break;
	}

	switch(reg4&CCSTATUS_ATTACHED_PORT_STATUS_BIT){
		case CCSTATUS_ATTACHED_PORT_STATUS_STANDBY: 
			result->device = CCLOGIC_NO_DEVICE;
			break;
		case CCSTATUS_ATTACHED_PORT_STATUS_DEVICE: 
			result->device = CCLOGIC_USB_DEVICE;
			break;
		case CCSTATUS_ATTACHED_PORT_STATUS_HOST:
			result->device = CCLOGIC_USB_HOST;
			break;
		case CCSTATUS_ATTACHED_PORT_STATUS_DEBUG:
			result->device = CCLOGIC_DEBUG_DEVICE;
			break;
		case CCSTATUS_ATTACHED_PORT_STATUS_AUDIO://mode audio
			result->device = CCLOGIC_AUDIO_DEVICE;
			break;
		default:
			result->device = CCLOGIC_DEVICE_UNKNOWN;
			break;
	}

}

/**
 * pi5usb30216_get_state()
 */
static int pi5usb30216_get_state(struct i2c_client *client,
					struct cclogic_state *state)
{
	char reg[MAX_REG_NUM] = {0};
	static int reg3,reg4;
	static int repeat_times = 0;
	static int audio_accessory_flag = 0;
	static int debug_accessory_flag = 0;
	int ret = 0;
	

	pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

	state->vbus = false;
	state->cc = CCLOGIC_CC_UNKNOWN;
	state->evt = CCLOGIC_EVENT_DETACHED;
	state->device = CCLOGIC_DEVICE_UNKNOWN;
	state->charger = CCLOGIC_CURRENT_NONE;

	mdelay(10);

	ret = i2c_master_recv(client, reg, MAX_REG_NUM); 
	if (ret < 0) {
		dev_err(&client->dev,"cclogic:%s-->i2c recv error\n", __func__);
		return ret;
	}	
	pr_debug("cclogic:%s-->i2c reg value: 0x%02x 0x%02x 0x%02x 0x%02x\n",
			__func__,reg[0],reg[1],reg[2],reg[3]);

	reg3 = reg[REG_INTERRUPT];
	reg4 = reg[REG_CC_STATE];

	if(repeat_times > 3){
		repeat_times = 0;
	}
	
	switch(reg3){
	case 0x01:
		switch(reg4){
		case 0x00:
			repeat_times = 0;
			audio_accessory_flag = 0;
			debug_accessory_flag = 0;
			ret = 0;
			break;
		case 0x05:
		case 0x06:
			if(repeat_times>=2){
				repeat_times = 0;
				mdelay(50);
				//Device Plug in
				pi5usb30216_parse_cclogic_state(reg3,reg4,state);
				ret = 0;
			}else{
				repeat_times++;
				mdelay(240);
				
				ret = pi5usb30216_write_i2c(client,REG_CONTROL,
						PORT_SETTING_INTERRUPT_MASK);//0x01
				if(ret){
					dev_err(&client->dev,"cclogic:%s-->i2c write error\n",
							__func__);
					return ret;
				}
				mdelay(100);

				ret = pi5usb30216_write_i2c(client,REG_CONTROL,\
						CHARGING_CURRENT_MODE_DEFAULT|\
						PORT_SETTING_DUALROLE|\
						PORT_SETTING_INTERRUPT_NOMASK);//0x04
				if(ret){
					dev_err(&client->dev,"cclogic:%s-->i2c write error\n",
							__func__);
					return ret;
				}
				ret = -1;
			}
			break;
		case 0x13:
			if(repeat_times >= 1){
				repeat_times = 0;
				mdelay(50);
				debug_accessory_flag = 1;
				//Debug Plug in
				pi5usb30216_parse_cclogic_state(reg3,reg4,state);
				ret = 0;
			}else{
				repeat_times++;
				//p2_3 = 0;
				debug_accessory_flag = 0;
				mdelay(240);
	
				ret = pi5usb30216_write_i2c(client,REG_CONTROL,
						PORT_SETTING_INTERRUPT_MASK);//0x01
				if(ret){
					dev_err(&client->dev,"cclogic:%s-->i2c write error\n",
							__func__);
					return ret;
				}
				mdelay(100);

				ret = pi5usb30216_write_i2c(client,REG_CONTROL,\
						CHARGING_CURRENT_MODE_DEFAULT|\
						PORT_SETTING_DUALROLE|\
						PORT_SETTING_INTERRUPT_NOMASK);//0x04
				if(ret){
					dev_err(&client->dev,"cclogic:%s-->i2c write error\n",
							__func__);
					return ret;
				}
				ret = 0;
			}
			break;
		case 0xa8:
			ret = 0;
			break;
		case 0x0f:
			audio_accessory_flag=1;
			mdelay(50);
			//Audio Plug in
			pi5usb30216_parse_cclogic_state(reg3,reg4,state);
			ret = 0;
			break;
		case 0x93:
			if(debug_accessory_flag){
				reg4 &= 0x7f;
			}
			pi5usb30216_parse_cclogic_state(reg3,reg4,state);
			ret = 0;
			break;
		case 0x8f:
			if(audio_accessory_flag){
				reg4 &= 0x7f;
			}
			pi5usb30216_parse_cclogic_state(reg3,reg4,state);
			ret = 0;
			break;
		case 0xa9:
		case 0xaa:
		case 0xc9:
		case 0xca:
		case 0xe9:
		case 0xea:
			//Host plug in
			pi5usb30216_parse_cclogic_state(reg3,reg4,state);
			ret = 0;
			break;
		default:
			ret = -1;
			break;
		}
		break;
	case 0x00:
		switch(reg4){
		case 0x00:
			repeat_times = 0;
			audio_accessory_flag = 0;
			debug_accessory_flag = 0;
			ret = 0;
			break;
		case 0x97:
			if(debug_accessory_flag || audio_accessory_flag){
				mdelay(100);
				ret = 0;
			}else if(repeat_times>=3){
				repeat_times = 0;
				ret = 0;
			}else{
				repeat_times++;	
				mdelay(240);
	
				ret = pi5usb30216_write_i2c(client,REG_CONTROL,
						PORT_SETTING_INTERRUPT_MASK);//0x01
				if(ret){
					dev_err(&client->dev,"cclogic:%s-->i2c write error\n",
							__func__);
					return ret;
				}
				mdelay(100);

				ret = pi5usb30216_write_i2c(client,REG_CONTROL,\
						CHARGING_CURRENT_MODE_DEFAULT|\
						PORT_SETTING_DUALROLE|\
						PORT_SETTING_INTERRUPT_NOMASK);//0x04
				if(ret){
					dev_err(&client->dev,"cclogic:%s-->i2c write error\n",
							__func__);
					return ret;
				}
				ret = 0;
			}
			break;
		case 0x93:
			if(debug_accessory_flag){
				reg4 &= 0x7f;
			}
			pi5usb30216_parse_cclogic_state(reg3,reg4,state);
			ret = 0;
			break;
		case 0x8f:
			if(audio_accessory_flag){
				reg4 &= 0x7f;
			}
			pi5usb30216_parse_cclogic_state(reg3,reg4,state);
			ret = 0;
			break;
		case 0xa9:
		case 0xaa:
		case 0xc9:
		case 0xca:
		case 0xe9:
		case 0xea:
			pi5usb30216_parse_cclogic_state(reg3,reg4,state);
			ret = 0;
			break;
		case 0x04:
		case 0x05:
		case 0x06:
		default:
			ret = -1;
			break;
		}
		break;
	case 0x02:
		audio_accessory_flag = 0;
		debug_accessory_flag = 0;
		repeat_times=0;
		pi5usb30216_parse_cclogic_state(reg3,reg4,state);
		ret = 0;
		break;
	default:
		audio_accessory_flag = 0;
		debug_accessory_flag = 0;
		repeat_times=0;
		ret = 0;
		break;
	}

	return ret;
}

/**
 * pi5usb30216_check_chip()
 */
static int pi5usb30216_check_chip(struct i2c_client *client)
{
	int ret;
	char reg_test_w[MAX_REG_NUM] = {0};
	char reg_test_r[MAX_REG_NUM] = {0};

	pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

	/* ID check */
	ret = i2c_master_recv(client, reg_test_r, 2); 
	if(ret<0){
		dev_err(&client->dev,"cclogic:%s: i2c read error\n", __func__);
		return ret;
	}

	if(reg_test_r[REG_ID] != PIUSB30216_DEVID){		
		dev_err(&client->dev,"cclogic:%s: devid mismatch"
				" (0x%02x!=0x%02x)\n", __func__,reg_test_r[REG_ID],
				PIUSB30216_DEVID);
		return  -ENODEV;
	}

	/* i2c R/W test */
	reg_test_w[REG_CONTROL] = 0x55;
	ret = i2c_master_send(client, reg_test_w, 2); 
	if (ret < 0) {
		dev_err(&client->dev,"cclogic:%s:i2c write error\n", __func__);
		return ret;
	}

	ret = i2c_master_recv(client, reg_test_r, 2); 
	if (ret < 0) {
		dev_err(&client->dev,"cclogic:%s:i2c read error\n", __func__);
		return ret;
	}
	if(reg_test_r[REG_CONTROL]!=0x55){
		dev_err(&client->dev,"cclogic:%s:i2c reg r/w test failed\n",
				__func__);
		return -ENODEV;
	}

	return 0;

}

/**
 * pi5usb30216_reset_chip()
 */
static int pi5usb30216_reset_chip(struct i2c_client *client)
{
	pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

	return 0;
}

/**
 * pi5usb30216_config_chip()
 */
static int pi5usb30216_config_chip(struct i2c_client *client)
{
	int ret;

	pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

	ret = pi5usb30216_write_i2c(client,REG_CONTROL,
			CHARGING_CURRENT_MODE_DEFAULT|PORT_SETTING_DUALROLE|\
			PORT_SETTING_INTERRUPT_NOMASK);
	if(ret){
		dev_err(&client->dev,"cclogic:%s: i2c write error\n", __func__);
		return ret;
	}

	return 0;
}

static struct cclogic_chip pi5usb30216_chip = {
	.chip_name		= DRIVER_NAME,
	.get_state		= pi5usb30216_get_state,
	.ack_irq  		= NULL,
	.chip_config		= pi5usb30216_config_chip,
	.chip_reset		= pi5usb30216_reset_chip,
	.chip_check		= pi5usb30216_check_chip,
	.typec_version          = 10,//spec 1.0
	.support                = 0,
};

/**
 * pi5usb30216_init()
 */
static int __init pi5usb30216_init(void)
{
	pr_info("cclogic:[%s][%d]\n", __func__, __LINE__);

	return cclogic_register(&pi5usb30216_chip);
}

/**
 * pi5usb30216_exit()
 */
static void __exit pi5usb30216_exit(void)
{
	pr_info("cclogic:[%s][%d]\n", __func__, __LINE__);

	cclogic_unregister(&pi5usb30216_chip);
	return;
}

late_initcall(pi5usb30216_init);
module_exit(pi5usb30216_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Yang ShaoYing <yangsy2@lenovo.com>");
MODULE_DESCRIPTION("Drivers for Type-C CC-Logic chip of Pericom Pi5usb30216");
MODULE_ALIAS("platform:cc-logic");
