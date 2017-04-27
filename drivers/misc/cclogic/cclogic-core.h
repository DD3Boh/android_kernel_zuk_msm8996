#ifndef __CCLOGIC_COMMON_H
#define __CCLOGIC_COMMON_H 

#include <linux/wakelock.h>
#include "cclogic-class.h"

#define DEBUG

#define DEV_STAGE_DEBUG 
#define CCLOGIC_UPDATE_REAL_STATUS

#ifdef DEBUG
#undef	pr_debug
#undef  pr_info
#define pr_debug(fmt, args...) pr_err(fmt, ##args) 
#define pr_info(fmt, args...) pr_err(fmt, ##args) 
#endif


#define CCLOGIC_I2C_VTG_MIN_UV		1800000
#define CCLOGIC_I2C_VTG_MAX_UV		1800000
#define CCLOGIC_I2C_LOAD_UA      	1800

#define CCLOGIC_MAX_SUPPORT_CHIP    2
//#define CCLOGIC_MAX_RETRIES         100
#define CCLOGIC_MAX_RETRIES         5

struct cclogic_of_chip {
	const char * chip_name;
	int  enb;
	int  address;
};

#define CCLOGIC_SUPPORT_MODE_DFP   (1<<0)
#define CCLOGIC_SUPPORT_MODE_UFP   (1<<1)
#define CCLOGIC_SUPPORT_MODE_DUAL  ((1<<0)|(1<<1))
#define CCLOGIC_SUPPORT_POWER_SOURCE (1<<2)
#define CCLOGIC_SUPPORT_POWER_SINK   (1<<3)
#define CCLOGIC_SUPPORT_POWER_SWAP   ((1<<2)|(1<<3))
#define CCLOGIC_SUPPORT_DATA_HOST    (1<<4)
#define CCLOGIC_SUPPORT_DATA_DEVICE  (1<<5)
#define CCLOGIC_SUPPORT_DATA_SWAP   ((1<<4)|(1<<5))

#define CCLOGIC_MODE_NONE     0
#define CCLOGIC_MODE_DFP      1
#define CCLOGIC_MODE_UFP      2
#define CCLOGIC_MODE_DRP      3
#define CCLOGIC_POWER_NONE    0
#define CCLOGIC_POWER_SOURCE  1
#define CCLOGIC_POWER_SINK    2
#define CCLOGIC_DATA_NONE     0
#define CCLOGIC_DATA_HOST     1
#define CCLOGIC_DATA_DEVICE   2

struct cclogic_platform {
	unsigned int irq_working_flags;
	unsigned int irq_working;
	unsigned int irq_plug_flags;
	unsigned int irq_plug;
	bool 	     i2c_pull_up;
	unsigned int function_switch_gpio1;
	unsigned int function_switch_gpio10;
	unsigned int function_switch_gpio2;
	unsigned int usb_ss_gpio;
	unsigned int enb_gpio;
	unsigned int ccchip_power_gpio;
	int	     chip_num;
	struct regulator *vbus_reg;
	struct cclogic_of_chip chip[CCLOGIC_MAX_SUPPORT_CHIP];
};


enum cclogic_attached_type {
	CCLOGIC_DEVICE_UNKNOWN,
	CCLOGIC_NO_DEVICE,
	CCLOGIC_USB_DEVICE,
	CCLOGIC_USB_HOST,
	CCLOGIC_DEBUG_DEVICE,
	CCLOGIC_AUDIO_DEVICE,
};

enum cclogic_current_type {
	CCLOGIC_CURRENT_NONE,
	CCLOGIC_CURRENT_DEFAULT,
	CCLOGIC_CURRENT_MEDIUM,
	CCLOGIC_CURRENT_ACCESSORY,
	CCLOGIC_CURRENT_HIGH,
};

enum cclogic_event_type {
	CCLOGIC_EVENT_NONE,
	CCLOGIC_EVENT_DETACHED,
	CCLOGIC_EVENT_ATTACHED,
};

enum cclogic_cc_type {
	CCLOGIC_CC_UNKNOWN,
	CCLOGIC_CC1,
	CCLOGIC_CC2,	
	CCLOGIC_CC1_CC2,
};

struct cclogic_state {
	bool vbus;
	enum cclogic_cc_type  cc;
	enum cclogic_event_type evt;
	enum cclogic_attached_type device;
	enum cclogic_current_type charger;
};

struct cclogic_chip;
struct cclogic_dev	{
	struct device		*dev;
	struct i2c_client	*i2c_client;
	unsigned int		irq_working;
	unsigned int		irq_plug;
	bool			irq_enabled;
	struct regulator 	*vcc_i2c;
	bool 			regulator_en;
	struct delayed_work	work;
	struct delayed_work	plug_work;
	struct cclogic_platform *platform_data;
	struct wake_lock 	wakelock;
	struct wake_lock 	wakelock_plug;
	bool			vbus_on;
	struct cclogic_chip *ops;
	struct cclogic_state state;
	struct pinctrl *pin;
	struct pinctrl_state *pin_active;
	struct pinctrl_state *pin_suspend;	
	struct cclogic_class_dev cdev;
	int          enb;
	unsigned int typec_version;
};

struct cclogic_chip {
	const char * chip_name;
	unsigned char addr;
	unsigned int typec_version;
	unsigned int support;
	int (*get_state)(struct i2c_client *client,struct cclogic_state *result);
	int (*ack_irq)(struct i2c_client *client);
	int (*chip_config)(struct i2c_client *client);
	int (*chip_reset)(struct i2c_client *client);
	int (*chip_check)(struct i2c_client *client);
	int (*chip_trymode)(struct i2c_client *client, int mode);
	struct list_head  chip_list;
};

enum cclogic_func_type {
	CCLOGIC_FUNC_HIZ,
	CCLOGIC_FUNC_USB,
	CCLOGIC_FUNC_AUDIO,
	CCLOGIC_FUNC_UART,
};



extern int cclogic_register(struct cclogic_chip *c);
extern void cclogic_unregister(struct cclogic_chip *c);

extern int cclogic_get_connected_mode(struct cclogic_state *state);
extern int cclogic_set_mode(struct cclogic_dev *pdata, int mode);
extern int cclogic_get_power_role(struct cclogic_state *state);
extern int cclogic_set_power_role(struct cclogic_dev *pdata, int role);
extern int cclogic_get_data_role(struct cclogic_state *state);
extern int cclogic_set_data_role(struct cclogic_dev *pdata, int role);

#endif
