//
// pi5usb30216d.c
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


#define DRIVER_NAME "pericom,pi5usb30216d"


/**
 * pi5usb30216d_write_i2c()   
 * only modified 0x2 register value.
 */
static int pi5usb30216d_write_i2c(struct i2c_client *client, u8 reg, u8 data)
{
	int ret = 0;
	char val[4] = {0};

	pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

	val[reg-1] = data;

	ret = i2c_master_send(client, val, 2); 
	if (ret != 2) {
		dev_err(&client->dev,"cclogic:%s-->i2c send error\n",__func__);
		return ret;
	}

	return 0;

}
/**
 * pi5usb30216d_parse_cclogic_state()
 */
static void pi5usb30216d_parse_cclogic_state(int reg3, int reg4, 
					struct cclogic_state *result)
{
	pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

	result->vbus = false;
	result->cc = CCLOGIC_CC_UNKNOWN;
	result->evt = CCLOGIC_EVENT_NONE;
	result->device = CCLOGIC_DEVICE_UNKNOWN;
	result->charger = CCLOGIC_CURRENT_NONE;

	if(reg3 == 0x3){	
		pr_err("cclogic:%s-->detach and attach in the same time\n",
				__func__);
	}else{
		if(reg3&0x2){
			result->evt = CCLOGIC_EVENT_DETACHED;
		}
		if(reg3&0x1){
			result->evt = CCLOGIC_EVENT_ATTACHED;
		}
	}	

	if(reg4&0x80){
		result->vbus = true;
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

	switch(reg4&0x3){
	case 0x01:
		result->cc = CCLOGIC_CC1;
		break;
	case 0x02:
		result->cc = CCLOGIC_CC2;
		break;
	default:
		break;
	}

	switch(reg4&0x1C){
	case 0x00: 
		result->device = CCLOGIC_NO_DEVICE;
		break;
	case 0x04: 
		result->device = CCLOGIC_USB_DEVICE;
		break;
	case 0x08:
		result->device = CCLOGIC_USB_HOST;
		break;
	case 0x0C:
		result->device = CCLOGIC_AUDIO_DEVICE;
		break;
	case 0x10:
		result->device = CCLOGIC_DEBUG_DEVICE;
		break;
	default:
		result->device = CCLOGIC_DEVICE_UNKNOWN;
		break;
	}

}

/**
 * pi5usb30216d_get_state()
 */
static int pi5usb30216d_get_state(struct i2c_client *client,
					struct cclogic_state *state)
{
	char reg[4] = {0};
	int ret = 0;
	
	pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

	ret = i2c_master_recv(client, reg, 4); 
	if (ret != 4) {
		dev_err(&client->dev,"cclogic:%s-->i2c recv error\n", __func__);
		return ret;
	}	
	pr_debug("cclogic:%s-->i2c reg value: 0x%02x 0x%02x 0x%02x 0x%02x\n",
			__func__,reg[0],reg[1],reg[2],reg[3]);

	pi5usb30216d_parse_cclogic_state(reg[2],reg[3],state);

	return 0;
}

/**
 * pi5usb30216d_check_chip()
 */
static int pi5usb30216d_check_chip(struct i2c_client *client)
{
	int ret;
	char reg_test_r = {0};

	pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

	/* ID check */
	ret = i2c_master_recv(client, &reg_test_r, 1); 
	if(ret!=1){
		dev_err(&client->dev,"cclogic:%s: i2c read error\n", __func__);
		return ret;
	}

	if(reg_test_r != 0x20){		
		dev_err(&client->dev,"cclogic:%s: devid mismatch"
				" (0x%02x!=0x20)\n", __func__,reg_test_r);
		return  -ENODEV;
	}

	return 0;
}
/**
 * pi5usb30216d_trymode()
 */
static int pi5usb30216d_trymode(struct i2c_client *client, int mode)
{
	int ret;
	char reg;

	pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

	if(mode == CCLOGIC_MODE_UFP){
		reg = 0x60;
		pr_debug("cclogic:trymode sink\n");
	}else{
		reg = 0x62;
		pr_debug("cclogic:trymode source\n");
	}
	ret = pi5usb30216d_write_i2c(client,0x2,reg);
	if(ret){
		dev_err(&client->dev,"cclogic:%s: i2c write error\n", __func__);
		return ret;
	}

	return ret;

}
/**
 * pi5usb30216d_config_chip()
 */
static int pi5usb30216d_config_chip(struct i2c_client *client)
{
	int ret;

	pr_debug("cclogic:[%s][%d]\n", __func__, __LINE__);

	ret = pi5usb30216d_write_i2c(client,0x2, 0x66);
	if(ret){
		dev_err(&client->dev,"cclogic:%s: i2c write error\n", __func__);
		return ret;
	}

	return 0;
}

static struct cclogic_chip pi5usb30216d_chip = {
	.chip_name		= DRIVER_NAME,
	.get_state		= pi5usb30216d_get_state,
	.ack_irq  		= NULL,
	.chip_config		= pi5usb30216d_config_chip,
	.chip_reset		= NULL,
	.chip_check		= pi5usb30216d_check_chip,
	.chip_trymode           = pi5usb30216d_trymode,
	.typec_version          = 11,//spec 1.1
	.support		= CCLOGIC_SUPPORT_MODE_DUAL,
};

/**
 * pi5usb30216d_init()
 */
static int __init pi5usb30216d_init(void)
{
	pr_info("cclogic:[%s][%d]\n", __func__, __LINE__);

	return cclogic_register(&pi5usb30216d_chip);
}

/**
 * pi5usb30216d_exit()
 */
static void __exit pi5usb30216d_exit(void)
{
	pr_info("cclogic:[%s][%d]\n", __func__, __LINE__);

	cclogic_unregister(&pi5usb30216d_chip);
	return;
}

late_initcall(pi5usb30216d_init);
module_exit(pi5usb30216d_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Yang ShaoYing <yangsy2@lenovo.com>");
MODULE_DESCRIPTION("Drivers for Type-C CC-Logic chip of Pericom pi5usb30216dd");
MODULE_ALIAS("platform:cc-logic");
