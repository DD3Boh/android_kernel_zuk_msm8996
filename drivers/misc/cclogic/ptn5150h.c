//
// ptn5150.c
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


#define DRIVER_NAME "nxp,ptn5150h"

#define PTN5150_DEVID    0xb

/**
 * ptn5150_read_i2c()
 */
static int ptn5150_read_i2c(struct i2c_client *client, u8 reg)
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
 * ptn5150_recv_i2c()
 */
static int ptn5150_recv_i2c(struct i2c_client *client, char baseaddr, 
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
 * ptn5150_write_i2c()
 */
static int ptn5150_write_i2c(struct i2c_client *client, u8 reg, u8 data)
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
 * ptn5150_parse_cclogic_state()
 */
static void ptn5150_parse_cclogic_state(int reg3, int reg4, int reg19, 
					struct cclogic_state *result)
{
	pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

	result->vbus = false;
	result->cc = CCLOGIC_CC_UNKNOWN;
	result->evt = CCLOGIC_EVENT_NONE;
	result->device = CCLOGIC_NO_DEVICE;
	result->charger = CCLOGIC_CURRENT_NONE;
	
	if(reg3&0x2){//No interrupt
		result->evt = CCLOGIC_EVENT_DETACHED;
	}else if(reg3&0x1){
		result->evt = CCLOGIC_EVENT_ATTACHED;
	}else
		result->evt = CCLOGIC_EVENT_NONE;

	switch(reg4&0x3){
	case 0x0:
		result->cc = CCLOGIC_CC_UNKNOWN;break;
	case 0x1:
		result->cc = CCLOGIC_CC1;break;
	case 0x2:
		result->cc = CCLOGIC_CC2;break;
	case 0x3:
		result->cc = CCLOGIC_CC1_CC2;break;
	}

	switch(reg4&0x60){
		case 0x00:
			result->charger = CCLOGIC_CURRENT_NONE;
			break;
		case 0x20:
			result->charger = CCLOGIC_CURRENT_DEFAULT;
			break;
		case 0x40:
			result->charger = CCLOGIC_CURRENT_MEDIUM;
			break;
		case 0x60:
			result->charger = CCLOGIC_CURRENT_HIGH;
			break;
	}

	if(reg4&0x80)
		result->vbus = true;

	switch(reg4&0x1C){
	case 0x00:
		result->device = CCLOGIC_NO_DEVICE;break;
	case 0x04:
		result->device = CCLOGIC_USB_HOST;break;
	case 0x08:
		result->device = CCLOGIC_USB_DEVICE;break;
	case 0x0C:
		result->device = CCLOGIC_AUDIO_DEVICE;break;
	case 0x10:
		result->device = CCLOGIC_DEBUG_DEVICE;break;
	}
}

/**
 * ptn5150_reset_chip()
 */
static int ptn5150_reset_chip(struct i2c_client *client)
{
	int ret;

	pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

	ret = ptn5150_write_i2c(client,0x10,0x1);
	if(ret){
		dev_err(&client->dev,"cclogic:%s: i2c write error\n", __func__);
		return ret;
	}

	mdelay(20);

	return ret;
}

#if 0
/**
 * ptn5150_trymode()
 */
static int ptn5150_trymode(struct i2c_client *client, int mode)
{
	int ret;
	int regval;

	pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

	regval = ptn5150_read_i2c(client,0xa);
	if(regval<0){
		dev_err(&client->dev,"cclogic:%s: i2c read error\n", 
				__func__);
		return regval;
	}

	if(mode == CCLOGIC_MODE_UFP){
		regval = (regval&0xf9) | 0x02;
	}else{
		regval = (regval&0xf9) | 0x06;
	}
	ret = ptn5150_write_i2c(client,0xa,regval);
	if(ret){
		dev_err(&client->dev,"cclogic:%s: i2c write error\n", __func__);
		return ret;
	}

	return ret;

}
#endif


/**
 * ptn5150_config_chip()
 */
static int ptn5150_config_chip(struct i2c_client *client)
{
	int ret;

	pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

	ret = ptn5150_write_i2c(client,0x09,0x1);//disable con_det output
	if(ret){
		dev_err(&client->dev,"cclogic:%s: i2c write error\n", __func__);
		return ret;
	}
	ret = ptn5150_write_i2c(client,0x43,0x40);
	if(ret){
		dev_err(&client->dev,"cclogic:%s: i2c write error\n", __func__);
		return ret;
	}
	ret = ptn5150_write_i2c(client,0x4c,0x34);
	if(ret){
		dev_err(&client->dev,"cclogic:%s: i2c write error\n", __func__);
		return ret;
	}
	ret = ptn5150_write_i2c(client,0x2,0x4);//charger:default unmask interrupt
	if(ret){
		dev_err(&client->dev,"cclogic:%s: i2c write error\n", __func__);
		return ret;
	}
	ret = ptn5150_write_i2c(client,0x18,0x00);//unmask interrupt
	if(ret){
		dev_err(&client->dev,"cclogic:%s: i2c write error\n", __func__);
		return ret;
	}

	return ret;

}

/**
 * ptn5150_check_chip()
 */
static int ptn5150_check_chip(struct i2c_client *client)
{
	char buf;
	int ret;

	pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

	/* ID check */
        ret = ptn5150_recv_i2c(client, 1, &buf, 1); 
	if(ret){
		dev_err(&client->dev,"cclogic:%s: i2c recv error\n", __func__);
		return -ENODEV;
	}

	if(buf != PTN5150_DEVID){		
		dev_err(&client->dev,"cclogic:%s: device version mismatch (%x != %x)\n", 
				__func__,buf,PTN5150_DEVID);
		return -ENODEV;
	}

	return 0;
}

/**
 * ptn5150_get_state()
 */
static int ptn5150_get_state(struct i2c_client *client,
			     struct cclogic_state *state)
{
	int reg3,reg4,reg19;

	pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

	reg3 = ptn5150_read_i2c(client,0x3);
	if(reg3<0){
		dev_err(&client->dev,"cclogic:%s-->i2c read error,reg3=%d\n", 
				__func__,reg3);
		return reg3;
	}

	reg4 = ptn5150_read_i2c(client,0x4);
	if(reg4<0){
		dev_err(&client->dev,"cclogic:%s-->i2c read error,reg4=%d\n", 
				__func__,reg4);
		return reg4;
	}

	reg19 = ptn5150_read_i2c(client,0x19);
	if(reg19<0){
		dev_err(&client->dev,"cclogic:%s-->i2c read error,reg19=%d\n", 
				__func__,reg19);
		return reg19;
	}
	

	pr_debug("cclogic:%s-->i2c register value: reg3=0x%02x reg4=0x%02x reg19=0x%02x\n",
				__func__,reg3,reg4,reg19);

	ptn5150_parse_cclogic_state(reg3,reg4,reg19,state);

	return 0;
}


static struct cclogic_chip ptn5150_chip = {
	.chip_name		= DRIVER_NAME,
	.get_state		= ptn5150_get_state,
	.ack_irq  		= NULL,
	.chip_config		= ptn5150_config_chip,
	.chip_reset		= ptn5150_reset_chip,
	.chip_check		= ptn5150_check_chip,
#if 0
	.chip_trymode           = ptn5150_trymode,
	.typec_version          = 11,//spec 1.1
	.support		= CCLOGIC_SUPPORT_MODE_DUAL,
#else
	.typec_version          = 10,//spec 1.0
#endif
};

/**
 * ptn5150_init()
 */
static int __init ptn5150_init(void)
{
	pr_info("cclogic:[%s][%d]\n", __func__, __LINE__);

	return cclogic_register(&ptn5150_chip);
}

/**
 * ptn5150_exit()
 */
static void __exit ptn5150_exit(void)
{
	pr_info("cclogic:[%s][%d]\n", __func__, __LINE__);

	cclogic_unregister(&ptn5150_chip);
	return;
}

late_initcall(ptn5150_init);
module_exit(ptn5150_exit);


MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Yang ShaoYing <yangsy2@lenovo.com>");
MODULE_DESCRIPTION("Drivers for usb type-C CC-Logic chip of NXP5150H");
MODULE_ALIAS("platform:cc-logic");
