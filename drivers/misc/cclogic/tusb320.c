//
// tusb320.c
//
// Drivers for usb type-C interface's CC-Logic chip of TI
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


#define DRIVER_NAME "ti,tusb320"

#define MAX_REG_NUM     11

#define TUSB320_DEVID   "023BSUT"

/**
 * tusb320_read_i2c()
 */
static int tusb320_read_i2c(struct i2c_client *client, u8 reg)
{
        struct i2c_msg msg[2];
        int data=0;
        int ret;

        pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

        if (!client->adapter)
          return -ENODEV;

        msg[0].addr = client->addr;
        msg[0].flags = 0;
        msg[0].buf = &reg;
        msg[0].len = sizeof(reg);
        msg[1].addr = client->addr;
        msg[1].flags = I2C_M_RD;
        msg[1].buf = (char *)&data;
        msg[1].len = 1;


        ret = i2c_transfer(client->adapter, msg, 2);
        if (ret != 2){
                dev_err(&client->dev,"cclogic:%s-->i2c_transfer error\n",
					__func__);
                return ret;
        }

        return data;
}

/**
 * tusb320_recv_i2c()
 */
static int tusb320_recv_i2c(struct i2c_client *client, char baseaddr, 
			    char * buf, int len)
{
        struct i2c_msg msg[2];
        int ret;

        pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

        if (!client->adapter)
          return -ENODEV;

        msg[0].addr = client->addr;
        msg[0].flags = 0;
        msg[0].buf = &baseaddr;
        msg[0].len = 1;
        msg[1].addr = client->addr;
        msg[1].flags = I2C_M_RD;
        msg[1].buf = buf;
        msg[1].len = len;


        ret = i2c_transfer(client->adapter, msg, 2);
        if (ret != 2){
		dev_err(&client->dev,"cclogic:%s-->i2c_transfer error\n",
				__func__);
		return ret;
        }

        return 0;
}
/**
 * tusb320_write_i2c()
 */
static int tusb320_write_i2c(struct i2c_client *client, u8 reg, u8 data)
{
	int ret = 0;
	struct i2c_msg msg;
	u8 buf[2];

	pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

	if (!client->adapter)
	  return -ENODEV;

	buf[0] = reg;
	buf[1] = data;
	msg.addr = client->addr;
	msg.flags = 0;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret != 1){
		dev_err(&client->dev,"cclogic:%s-->i2c_transfer error\n",
				__func__);
		return ret;
	}

	return 0;

}

/**
 * tusb320_parse_cclogic_state()
 */
static void tusb320_parse_cclogic_state(int reg8, int reg9, 
					struct cclogic_state *result)
{
	pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

	result->vbus = false;
	result->cc = CCLOGIC_CC_UNKNOWN;
	result->evt = CCLOGIC_EVENT_NONE;
	result->device = CCLOGIC_NO_DEVICE;
	result->charger = CCLOGIC_CURRENT_NONE;
	
	if(!(reg9&0x10)){//No interrupt
		result->evt = CCLOGIC_EVENT_NONE;
	}else{
		result->evt = CCLOGIC_EVENT_ATTACHED;
	}

	if(reg9&0x20){
		result->cc = CCLOGIC_CC2;
	}else{
		result->cc = CCLOGIC_CC1;
	}

	switch(reg9&0xc0){
	case 0x00://nothing attached
		if(reg9&0x10){
			result->evt = CCLOGIC_EVENT_DETACHED;
		}
		result->vbus = false;
		result->device = CCLOGIC_NO_DEVICE;
		break;
	case 0x40:
		result->device = CCLOGIC_USB_DEVICE;
		result->vbus = false;
		break;
	case 0x80:
		result->device = CCLOGIC_USB_HOST;
		result->vbus = true;
		break;
	case 0xC0://accessory
		switch(reg8&0x0e){
			case 0x00: //No Accessory attached
				if(reg9&0x10){
					result->evt = CCLOGIC_EVENT_DETACHED;
				}
				result->vbus = false;
				result->device = CCLOGIC_NO_DEVICE;
				break;
			case 0x08: //Audio Accessory DFP
				result->vbus = false;
				result->device = CCLOGIC_AUDIO_DEVICE;
				break;
			case 0x0a: //Audio Accessory UFP
				result->vbus = true;
				result->device = CCLOGIC_AUDIO_DEVICE;
				break;
			case 0x0e:// Debug Accessory UFP
				result->vbus = true;
				result->device = CCLOGIC_DEBUG_DEVICE;
				break;
			case 0x0c:// Debug Accessory DFP     set to Hi-Z
				result->vbus = false;
				result->device = CCLOGIC_DEBUG_DEVICE;
				break;
			default://Reserve
				result->vbus = false;
				result->device = CCLOGIC_NO_DEVICE;
				break;
		}

		break;
	}


	switch(reg8&0x30){
		case 0x00:
			result->charger = CCLOGIC_CURRENT_DEFAULT;
			break;
		case 0x10:
			result->charger = CCLOGIC_CURRENT_MEDIUM;
			break;
		case 0x20:
			result->charger = CCLOGIC_CURRENT_ACCESSORY;
			//result->vbus = false;
			break;
		case 0x30:
			result->charger = CCLOGIC_CURRENT_HIGH;
			break;
	}

}

/**
 * tusb320_reset_chip()
 */
static int tusb320_reset_chip(struct i2c_client *client)
{
	int retries = 10;
	int regval;

	pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

	regval = tusb320_write_i2c(client,0xa,0x8);
	if(regval){
		dev_err(&client->dev,"cclogic:%s: i2c write error\n", __func__);
		return regval;
	}

	while(retries--){	
		regval = tusb320_read_i2c(client,0xa);
		if(regval<0){
			dev_err(&client->dev,"cclogic:%s: i2c read error\n", 
					__func__);
			return regval;
		}
		if(!(regval&0x08)){
			return 0;
		}
		mdelay(100);
	}
	if(regval&0x08){
		return -1;
	}
	return 0;

}

/**
 * tusb320_config_chip()
 */
static int tusb320_config_chip(struct i2c_client *client)
{
	int ret;

	pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

	ret = tusb320_write_i2c(client,0x8,0);//charger:default
	if(ret){
		dev_err(&client->dev,"cclogic:%s: i2c write error\n", __func__);
		return ret;
	}
	ret = tusb320_write_i2c(client,0xa,0x30);//DRP
	if(ret){
		dev_err(&client->dev,"cclogic:%s: i2c write error\n", __func__);
		return ret;
	}

	return ret;

}

/**
 * tusb320_check_chip()
 */
static int tusb320_check_chip(struct i2c_client *client)
{
	char buf[8];
	int ret;

	pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

	/* ID check */
        ret = tusb320_recv_i2c(client, 0, buf, 8); 
	if(ret){
		dev_err(&client->dev,"cclogic:%s: i2c recv error\n", __func__);
		return -ENODEV;
	}

	if(memcmp(buf,TUSB320_DEVID,sizeof(TUSB320_DEVID))){		
		dev_err(&client->dev,"cclogic:%s: devid mismatch (%s != %s)\n", 
				__func__,buf,TUSB320_DEVID);
		return -ENODEV;
	}

	return 0;
}

/**
 * tusb320_ack_irq()
 */
static int tusb320_ack_irq(struct i2c_client *client)
{
	int reg9,ret;
	
	pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

	reg9 = tusb320_read_i2c(client,0x9);
	if(reg9<0){
		dev_err(&client->dev,"cclogic:%s-->i2c read error\n", __func__);
		return reg9;
	}

	/* clear interrupt */
	ret = tusb320_write_i2c(client,0x9,reg9);
	if(ret){
		dev_err(&client->dev,"cclogic:%s-->i2c write error\n",__func__);
		return ret;
	}
	
	return 0;
}

/**
 * tusb320_get_state()
 */
static int tusb320_get_state(struct i2c_client *client,
			     struct cclogic_state *state)
{
	int reg8,reg9;
	static int flag = 0;

	pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

	reg9 = tusb320_read_i2c(client,0x9);
	if(reg9<0){
		dev_err(&client->dev,"cclogic:%s-->i2c read error,reg9=%d\n", 
				__func__,reg9);
		return reg9;
	}

	reg8 = tusb320_read_i2c(client,0x8);
	if(reg8<0){
		dev_err(&client->dev,"cclogic:%s-->i2c read error,reg8=%d\n", 
				__func__,reg8);
		return reg8;
	}

	pr_debug("cclogic:%s-->i2c register value: reg8=0x%02x reg9=0x%02x\n",
				__func__,reg8,reg9);

	tusb320_parse_cclogic_state(reg8,reg9,state);

#if 1   //for chip bug
	if(state->evt == CCLOGIC_EVENT_DETACHED){//skip detach event
		return 0;
	}

	if(!flag && (state->device == CCLOGIC_USB_DEVICE)){
		tusb320_reset_chip(client);
		flag = 1;
		return -1;
	}

	flag = 0;
#endif

	return 0;
}


static struct cclogic_chip tusb320_chip = {
	.chip_name		= DRIVER_NAME,
	.get_state		= tusb320_get_state,
	.ack_irq  		= tusb320_ack_irq,
	.chip_config		= tusb320_config_chip,
	.chip_reset		= tusb320_reset_chip,
	.chip_check		= tusb320_check_chip,
	.typec_version          = 10,//spec 1.0
	.support                = 0,
};

/**
 * tusb320_init()
 */
static int __init tusb320_init(void)
{
	pr_info("cclogic:[%s][%d]\n", __func__, __LINE__);

	return cclogic_register(&tusb320_chip);
}

/**
 * tusb320_exit()
 */
static void __exit tusb320_exit(void)
{
	pr_info("cclogic:[%s][%d]\n", __func__, __LINE__);

	cclogic_unregister(&tusb320_chip);
	return;
}

late_initcall(tusb320_init);
module_exit(tusb320_exit);


MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Yang ShaoYing <yangsy2@lenovo.com>");
MODULE_DESCRIPTION("Drivers for usb type-C CC-Logic chip of TI Tusb320");
MODULE_ALIAS("platform:cc-logic");
