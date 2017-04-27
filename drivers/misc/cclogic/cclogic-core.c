//
// cclogic-core.c
//
// Core of CC-Logic drivers
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
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/cclogic-core.h>

#include "cclogic-core.h"
#include "cclogic-class.h"


static struct cclogic_dev *cclogic_priv;
static struct mutex cclogic_ops_lock;

static int m_plug_state = 0;

#define DRIVER_NAME "cclogic"
/*
 *
 */
static void cclogic_patch_state(struct cclogic_dev *pdata)
{
	struct cclogic_state *state = &pdata->state;

	if(gpio_get_value(pdata->platform_data->irq_plug)){
		state->evt = CCLOGIC_EVENT_DETACHED;
		state->device = CCLOGIC_NO_DEVICE;
		state->vbus = false;
		state->cc = CCLOGIC_CC1;
	}
}
/*
 *
 */
int cclogic_vbus_power_on(struct cclogic_dev *cclogic_dev, bool enable)
{
	struct cclogic_platform *p = cclogic_dev->platform_data;
	int ret;

	pr_debug("[%s][%d] enable=%d\n", __func__, __LINE__,enable);

	if (!p->vbus_reg) {
		return 0;
	}

	if(enable){
		if(cclogic_dev->vbus_on)
			return 0;

		ret = regulator_enable(p->vbus_reg);
                if (ret) {
                        dev_err(&cclogic_dev->i2c_client->dev, "Failed to enable vbus_reg\n");
			return ret;
                }
		cclogic_dev->vbus_on = true;
	}else{
		if(!cclogic_dev->vbus_on)
			return 0;
	        ret = regulator_disable(p->vbus_reg);
                if (ret) {
                        dev_err(&cclogic_dev->i2c_client->dev, "Failed to disable vbus_reg\n");
                        return ret;
                }
		cclogic_dev->vbus_on = false;
	}

	return 0;
}
/*
 *
 */
static int cclogic_reg_set_optimum_mode_check(struct regulator *reg,
		int load_uA)
{
	pr_debug("[%s][%d]\n", __func__, __LINE__);

	return (regulator_count_voltages(reg) > 0) ?
		regulator_set_optimum_mode(reg, load_uA) : 0;
}
/*
 *
 */
static int cclogic_power_on(struct cclogic_dev *cclogic_dev, bool on) 
{
	int ret=0;

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	if (cclogic_dev->platform_data->i2c_pull_up) {	
		if (on == false){
			if(cclogic_dev->regulator_en){
				cclogic_reg_set_optimum_mode_check(
						       cclogic_dev->vcc_i2c, 0);
				regulator_disable(cclogic_dev->vcc_i2c);
				cclogic_dev->regulator_en = false;
			}
		}else{
			if(!cclogic_dev->regulator_en){
				ret = cclogic_reg_set_optimum_mode_check(
				      cclogic_dev->vcc_i2c,CCLOGIC_I2C_LOAD_UA);
				if (ret < 0) {
					dev_err(&cclogic_dev->i2c_client->dev,
						"%s-->Regulator vcc_i2c set_opt"
						" failed rc=%d\n",__func__,ret);
					goto error_reg_opt_i2c;
				}
				ret = regulator_enable(cclogic_dev->vcc_i2c);
				if (ret) {
					dev_err(&cclogic_dev->i2c_client->dev,
						"%s-->Regulator vcc_i2c enable "
						"failed rc=%d\n", __func__,ret);
					goto error_reg_en_vcc_i2c;
				}
				cclogic_dev->regulator_en = true;
			}
		}
	}
	return 0;

error_reg_en_vcc_i2c:
	cclogic_reg_set_optimum_mode_check(cclogic_dev->vcc_i2c, 0);
error_reg_opt_i2c:
	cclogic_dev->regulator_en = false;
	return ret;
}
/*
 *
 */
static int cclogic_regulator_configure(struct cclogic_dev *cclogic_dev, bool on)
{
	int ret=0;

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	if (!cclogic_dev->platform_data->i2c_pull_up) {
		return 0;
	}
	if (on == false && cclogic_dev->vcc_i2c){
		if (regulator_count_voltages(cclogic_dev->vcc_i2c) > 0)
			regulator_set_voltage(cclogic_dev->vcc_i2c,0,
					CCLOGIC_I2C_VTG_MAX_UV);
		regulator_put(cclogic_dev->vcc_i2c);
		cclogic_dev->vcc_i2c = NULL;
	}else if(!cclogic_dev->vcc_i2c){
		cclogic_dev->vcc_i2c = 
			regulator_get(&cclogic_dev->i2c_client->dev,"vcc_i2c");
		if (IS_ERR(cclogic_dev->vcc_i2c)) {
			dev_err(&cclogic_dev->i2c_client->dev,
					"%s: Failed to get i2c regulator\n",
					__func__);
			ret = PTR_ERR(cclogic_dev->vcc_i2c);
			goto err_get_vtg_i2c;
		}

		if (regulator_count_voltages(cclogic_dev->vcc_i2c) > 0) {
			ret = regulator_set_voltage(cclogic_dev->vcc_i2c, 
					CCLOGIC_I2C_VTG_MIN_UV, 
					CCLOGIC_I2C_VTG_MAX_UV);
			if (ret) {
				dev_err(&cclogic_dev->i2c_client->dev,
						"%s-->reg set i2c vtg failed "
						"ret =%d\n", __func__,ret);
				goto err_set_vtg_i2c;
			}
		}
	}

	return 0;

err_set_vtg_i2c:
	regulator_put(cclogic_dev->vcc_i2c);
err_get_vtg_i2c:
	cclogic_dev->vcc_i2c=NULL;
	return ret;

};
/*
 *
 */
static int cclogic_irq_enable(struct cclogic_dev *cclogic_dev, bool enable)
{
	int ret = 0;

	pr_debug("[%s][%d] enable=%d  irq_enabled=%d\n", __func__, __LINE__,
			enable,cclogic_dev->irq_enabled);

	if (enable) {
		if (!cclogic_dev->irq_enabled){
			enable_irq(cclogic_dev->irq_working);
			if (gpio_is_valid(cclogic_dev->platform_data->irq_plug)) {
				enable_irq(cclogic_dev->irq_plug);
			}
			cclogic_dev->irq_enabled = true;
		}
	} else {
		if (cclogic_dev->irq_enabled) {
			disable_irq(cclogic_dev->irq_working);
			if (gpio_is_valid(cclogic_dev->platform_data->irq_plug)) {
				disable_irq(cclogic_dev->irq_plug);
			}
			cclogic_dev->irq_enabled = false;
		}
	}

	return ret;
}
/*
 *
 */
static int cclogic_parse_dt(struct device *dev, struct cclogic_platform *pdata)
{
	struct device_node *np = dev->of_node;
	struct device_node *temp;
	int idx = 0;
	int ret;
	unsigned int val;

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	pdata->chip_num = 0;

	pdata->irq_working = of_get_named_gpio_flags(np, "cc_logic,irq-working", 0, 
			&pdata->irq_working_flags);
	pdata->irq_plug = of_get_named_gpio_flags(np, "cc_logic,irq-plug", 0, 
			&pdata->irq_plug_flags);
	pdata->function_switch_gpio1 = of_get_named_gpio(np,
			"cc_logic,function-switch-gpio1", 0);
	pdata->function_switch_gpio10 = of_get_named_gpio(np,
			"cc_logic,function-switch-gpio10", 0);
	pdata->function_switch_gpio2 = of_get_named_gpio(np,
			"cc_logic,function-switch-gpio2", 0);
	pdata->usb_ss_gpio = of_get_named_gpio(np, "cc_logic,usb-ss-gpio", 0);
	pdata->enb_gpio = of_get_named_gpio(np, "cc_logic,power-control", 0);
	pdata->i2c_pull_up = of_property_read_bool(np,"cc_logic,i2c-pull-up");

	pdata->ccchip_power_gpio = of_get_named_gpio_flags(np,
			"cc_logic,bypass-power-control", 0, NULL);

        if (of_get_property(np, "vcc_otg-supply", NULL)) {
		pdata->vbus_reg = devm_regulator_get(dev, "vcc_otg");
        	if (IS_ERR(pdata->vbus_reg)) {
                        dev_err(dev, "Failed to get vbus regulator\n");
                        ret = PTR_ERR(pdata->vbus_reg);
			return ret;
                }
	}

	for_each_child_of_node(np, temp) {
		if(idx>CCLOGIC_MAX_SUPPORT_CHIP){
			dev_err(dev,"%s-->too many devices\n",__func__);
			break;
		}
		ret = of_property_read_string(temp, "chip-name",
				&pdata->chip[idx].chip_name);
		if (ret)
			return ret;

		ret = of_property_read_u32(temp, "chip-address", &val);
		if(ret)
			return ret;

		pdata->chip[idx].address = val;

		pdata->chip[idx].enb = of_property_read_bool(temp,"cc_logic,power-active-high");

		pr_debug("%s--> chip:%s, address:0x%02x, enb=%d\n",__func__,
				pdata->chip[idx].chip_name,val,pdata->chip[idx].enb);

		idx++;
	}

	pdata->chip_num = idx;

	return 0;
}


/*
 *
 */
static irqreturn_t cclogic_irq(int irq, void *data)
{
	struct cclogic_dev *cclogic_dev = (struct cclogic_dev *)data;

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	if(!cclogic_dev || !cclogic_dev->i2c_client){
		return IRQ_HANDLED;
	}

	if (!wake_lock_active(&cclogic_dev->wakelock)){
		wake_lock(&cclogic_dev->wakelock);
	}
	cancel_delayed_work(&cclogic_dev->work);
	schedule_delayed_work(&cclogic_dev->work, 0);

	return IRQ_HANDLED;
}

/*
 *
 */
static irqreturn_t cclogic_plug_irq(int irq, void *data)
{
	struct cclogic_dev *cclogic_dev = (struct cclogic_dev *)data;

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	if(!cclogic_dev || !cclogic_dev->i2c_client){
		return IRQ_HANDLED;
	}

	if (!wake_lock_active(&cclogic_dev->wakelock_plug)){
		pm_runtime_get(cclogic_dev->dev);
		wake_lock(&cclogic_dev->wakelock_plug);
	}

	m_plug_state = 1;

	schedule_delayed_work(&cclogic_dev->plug_work, 0);

	return IRQ_HANDLED;
}

#ifdef DEV_STAGE_DEBUG 
/*
 *
 */
static ssize_t cclogic_show_real_status(struct cclogic_state *pdata, char *buf)
{
	char * vbus=NULL;
	char * charging=NULL;
	char * port=NULL;
	char * polarity=NULL;
	char * evt=NULL;

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	switch(pdata->evt){
		case CCLOGIC_EVENT_NONE:
			evt="No Event";
			break;
		case CCLOGIC_EVENT_DETACHED:
			evt = "Detached Event";
			break;
		case CCLOGIC_EVENT_ATTACHED:
			evt = "Attached Event";
			break;
		default:
			evt = "unknown event";
	}

	if(pdata->vbus){
		vbus = "vbus=1";
	}else{
		vbus = "vbus=0";
	}

	switch(pdata->cc){
		case CCLOGIC_CC1:
			polarity = "polarity=cc1";
			break;
		case CCLOGIC_CC2:
			polarity = "polarity=cc2";
			break;
		case CCLOGIC_CC_UNKNOWN:
			polarity = "polarity=unknown";
			break;
		case CCLOGIC_CC1_CC2:
			polarity = "polarity=cc1_cc2";
			break;
	}

	switch(pdata->device){
		case CCLOGIC_NO_DEVICE:
			port = "attached=nothing";
			break;
		case CCLOGIC_USB_DEVICE:
			port = "attached=device";
			break;
		case CCLOGIC_USB_HOST:
			port = "attached=host";
			break;
		case CCLOGIC_DEBUG_DEVICE:
			if(pdata->vbus){
				port = "attached=debug_vbus";	
			}else{
				port = "attached=debug_novbus";	
			}
			break;
		case CCLOGIC_AUDIO_DEVICE:
			port = "attached=audio";	
			break;
		case CCLOGIC_DEVICE_UNKNOWN:
			port = "attached=unknown";
			break;
	}

	switch(pdata->charger){
		case CCLOGIC_CURRENT_NONE:
			charging = "charging=none";
			break;
		case CCLOGIC_CURRENT_DEFAULT:
			charging = "charging=default";
			break;
		case CCLOGIC_CURRENT_MEDIUM:
			charging = "charging=medium";
			break;
		case CCLOGIC_CURRENT_ACCESSORY:
			charging = "charging=from accessory";
			break;
		case CCLOGIC_CURRENT_HIGH:
			charging = "charging=high";
			break;
	}

	return sprintf(buf, " %15s:%8s;%25s;%23s;%15s\n",
			evt,vbus,charging,port,polarity);

}

/*
 *
 */
static ssize_t cclogic_set_chip_power_enable(struct device *dev, 
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct cclogic_dev *cclogic_dev = dev_get_drvdata(dev);
        int value;

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	if (gpio_is_valid(cclogic_dev->platform_data->ccchip_power_gpio)) {
		if (1 != sscanf(buf, "%d", &value)) {
			dev_err(dev, "Failed to parse integer: <%s>\n", buf);
			return -EINVAL;
		}

		gpio_set_value_cansleep(cclogic_dev->platform_data->ccchip_power_gpio,value);
	}

	return count;
}

/*
 *
 */
static ssize_t cclogic_get_chip_power_enable(struct device *dev, 
		struct device_attribute *attr, char *buf)
{
	struct cclogic_dev *cclogic_dev = dev_get_drvdata(dev);
	int ret;

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	if (gpio_is_valid(cclogic_dev->platform_data->ccchip_power_gpio)) {
		ret = gpio_get_value_cansleep(cclogic_dev->platform_data->ccchip_power_gpio);
		if(!ret){
			return sprintf(buf, "disabled\n"); 
		}else{
			return sprintf(buf, "enabled\n"); 
		}
	}else{
		return sprintf(buf, "No chip power control pin\n"); 
	}
}

static DEVICE_ATTR(chip_power, S_IRUGO|S_IWUSR, cclogic_get_chip_power_enable, cclogic_set_chip_power_enable);


/*
 *
 */
static ssize_t cclogic_chip_store_enable(struct device *dev, 
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct cclogic_dev *cclogic_dev = dev_get_drvdata(dev);
        int value;

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	if (gpio_is_valid(cclogic_dev->platform_data->enb_gpio)) {
		if (1 != sscanf(buf, "%d", &value)) {
			dev_err(dev, "Failed to parse integer: <%s>\n", buf);
			return -EINVAL;
		}

		gpio_set_value_cansleep(cclogic_dev->platform_data->enb_gpio,cclogic_dev->enb == value);
	}

	return count;
}

/*
 *
 */
static ssize_t cclogic_chip_show_enable(struct device *dev, 
		struct device_attribute *attr, char *buf)
{
	struct cclogic_dev *cclogic_dev = dev_get_drvdata(dev);
	int ret;

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	if (gpio_is_valid(cclogic_dev->platform_data->enb_gpio)) {
		ret = gpio_get_value_cansleep(cclogic_dev->platform_data->enb_gpio);
		if(ret == cclogic_dev->enb){
			return sprintf(buf, "enabled\n"); 
		}else{
			return sprintf(buf, "disabled\n"); 
		}
	}else{
		return sprintf(buf, "No enabled pin\n"); 
	}
}

static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR, cclogic_chip_show_enable, cclogic_chip_store_enable);

/*
 *
 */
static ssize_t cclogic_show_status(struct device *dev, 
		struct device_attribute *attr, char *buf)
{
	struct cclogic_dev *cclogic_dev = dev_get_drvdata(dev);
	int ret;

	pr_debug("[%s][%d]\n", __func__, __LINE__);

#ifdef CCLOGIC_UPDATE_REAL_STATUS
	pm_runtime_get_sync(dev);
	mutex_lock(&cclogic_ops_lock);
	if(cclogic_dev->ops && cclogic_dev->ops->get_state){
		ret = cclogic_dev->ops->get_state(cclogic_dev->i2c_client,
				&cclogic_dev->state);
		if(ret){
			mutex_unlock(&cclogic_ops_lock);
			pm_runtime_put(dev);
			return sprintf(buf, "error\n"); 
		}
	}else{
		mutex_unlock(&cclogic_ops_lock);
		pm_runtime_put(dev);
		return sprintf(buf, "no chip\n");	
	}
	mutex_unlock(&cclogic_ops_lock);
	pm_runtime_put(dev);

#endif
	cclogic_patch_state(cclogic_dev);
	return cclogic_show_real_status(&cclogic_dev->state, buf);
}

static DEVICE_ATTR(status, S_IRUGO, cclogic_show_status, NULL);

#endif

/*
 *
 */
static ssize_t cclogic_show_cc(struct device *dev, 
		struct device_attribute *attr, char *buf)
{
	struct cclogic_dev *cclogic_dev = dev_get_drvdata(dev);

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	return sprintf(buf, "%d\n", cclogic_dev->state.cc);

}

static DEVICE_ATTR(cc,S_IRUGO, cclogic_show_cc, NULL);

/*
 *
 */
static void cclogic_func_set(struct cclogic_platform *p,enum cclogic_func_type func)
{
	switch(func){
	case CCLOGIC_FUNC_HIZ:
		gpio_set_value_cansleep(p->function_switch_gpio2,0);
		if (gpio_is_valid(p->function_switch_gpio1)){
			gpio_set_value_cansleep(p->function_switch_gpio1,0);
		}
		if (gpio_is_valid(p->function_switch_gpio10)){
			gpio_set_value_cansleep(p->function_switch_gpio10,1);
		}
		break;
	case CCLOGIC_FUNC_USB:
		gpio_set_value_cansleep(p->function_switch_gpio2,1);
		if (gpio_is_valid(p->function_switch_gpio1)){
			gpio_set_value_cansleep(p->function_switch_gpio1,0);
		}
		if (gpio_is_valid(p->function_switch_gpio10)){
			gpio_set_value_cansleep(p->function_switch_gpio10,0);
		}
		break;
	case CCLOGIC_FUNC_AUDIO:
		gpio_set_value_cansleep(p->function_switch_gpio2,0);
		if (gpio_is_valid(p->function_switch_gpio1)){
#ifdef CONFIG_PRODUCT_Z2_PLUS
			gpio_set_value_cansleep(p->function_switch_gpio1,0);
#else
			gpio_set_value_cansleep(p->function_switch_gpio1,1);
#endif
		}
		if (gpio_is_valid(p->function_switch_gpio10)){
			gpio_set_value_cansleep(p->function_switch_gpio10,0);
		}
		break;
	case CCLOGIC_FUNC_UART:
		gpio_set_value_cansleep(p->function_switch_gpio2,1);
		if (gpio_is_valid(p->function_switch_gpio1)){
			gpio_set_value_cansleep(p->function_switch_gpio1,0);
		}
		if (gpio_is_valid(p->function_switch_gpio10)){
			gpio_set_value_cansleep(p->function_switch_gpio10,0);
		}
		break;
	}
}
/*
 *
 */
int cclogic_get_connected_mode(struct cclogic_state *state)
{
	if( state->evt == CCLOGIC_EVENT_DETACHED)
		return CCLOGIC_MODE_NONE;
	
	if(state->device == CCLOGIC_NO_DEVICE){
		return CCLOGIC_MODE_NONE;
	}else if( state->device == CCLOGIC_USB_DEVICE ){
		return CCLOGIC_MODE_DFP;
	}else if(state->device == CCLOGIC_USB_HOST){
		return CCLOGIC_MODE_UFP;
	}else if(state->vbus){
		return CCLOGIC_MODE_UFP;
	}else
		return CCLOGIC_MODE_DFP;

	return CCLOGIC_MODE_NONE;
}
EXPORT_SYMBOL(cclogic_get_connected_mode);

/*
 *
 */
int cclogic_set_mode(struct cclogic_dev *pdata, int mode)
{
	int ret=0;
	if(pdata->ops->chip_trymode)
		ret = pdata->ops->chip_trymode(pdata->i2c_client,mode);
	return ret;
}
EXPORT_SYMBOL(cclogic_set_mode);

/*
 *
 */
int cclogic_get_power_role(struct cclogic_state *state)
{
	if(cclogic_get_connected_mode(state)==CCLOGIC_MODE_NONE){
		return CCLOGIC_POWER_NONE;
	}

	if(state->vbus){
		return CCLOGIC_POWER_SINK;
	}else
		return CCLOGIC_POWER_SOURCE;
}
EXPORT_SYMBOL(cclogic_get_power_role);

/*
 *
 */
int cclogic_set_power_role(struct cclogic_dev *pdata, int role)
{
	return 0;
}
EXPORT_SYMBOL(cclogic_set_power_role);

/*
 *
 */
int cclogic_get_data_role(struct cclogic_state *state)
{
	if(cclogic_get_connected_mode(state)==CCLOGIC_MODE_NONE){
		return CCLOGIC_DATA_NONE;
	}

	if( state->device == CCLOGIC_USB_DEVICE ){
		return CCLOGIC_DATA_HOST;
	}else if( state->device == CCLOGIC_USB_HOST ){
		return CCLOGIC_DATA_DEVICE;
	}

	return CCLOGIC_DATA_NONE;
}
EXPORT_SYMBOL(cclogic_get_data_role);

/*
 *
 */
int cclogic_set_data_role(struct cclogic_dev *pdata, int role)
{
	return 0;
}
EXPORT_SYMBOL(cclogic_set_data_role);

static int cc_otg_state = 0; 
int cclogic_get_otg_state(void)
{
	return cc_otg_state;
}
EXPORT_SYMBOL(cclogic_get_otg_state);

/*
 *
 */
static int cclogic_do_real_work(struct cclogic_state *state, 
				struct cclogic_dev *pdata)
{
	int ret=0;
	struct cclogic_platform *p = pdata->platform_data;

	pr_debug("[%s][%d]\n", __func__, __LINE__);
	cc_otg_state = 0;
	switch(state->evt){
	case CCLOGIC_EVENT_DETACHED:
		pr_debug("%s-->cable detached\n",__func__);
		cclogic_vbus_power_on(pdata,false);
		cclogic_func_set(p,CCLOGIC_FUNC_UART);
		ret = pdata->ops->chip_config(pdata->i2c_client);
		goto out;
	case CCLOGIC_EVENT_NONE: 
		pr_debug("%s-->No event\n",__func__);
		break;
	case CCLOGIC_EVENT_ATTACHED:
		pr_debug("%s-->cable attached\n",__func__);
		break;
	}

	if(state->device == CCLOGIC_USB_DEVICE){//in order to disable usb3.0 when in host mode
		switch(state->cc){
		case CCLOGIC_CC1:
			pr_debug("%s-->usb_ss signal to cc2\n",__func__);
			if (gpio_is_valid(p->usb_ss_gpio))
				gpio_set_value_cansleep(p->usb_ss_gpio,0);
			break;
		case CCLOGIC_CC2:
			pr_debug("%s-->usb_ss signal to cc1\n",__func__);
			if (gpio_is_valid(p->usb_ss_gpio))
				gpio_set_value_cansleep(p->usb_ss_gpio,1);
			break;
		case CCLOGIC_CC_UNKNOWN:
		default:
			pr_debug("%s-->usb_ss signal to unknown\n",__func__);
			ret = -1;
			break;
		}

	}else{
		switch(state->cc){
		case CCLOGIC_CC1:
			pr_debug("%s-->usb_ss signal to cc1\n",__func__);
			if (gpio_is_valid(p->usb_ss_gpio))
				gpio_set_value_cansleep(p->usb_ss_gpio,1);
			break;
		case CCLOGIC_CC2:
			pr_debug("%s-->usb_ss signal to cc2\n",__func__);
			if (gpio_is_valid(p->usb_ss_gpio))
				gpio_set_value_cansleep(p->usb_ss_gpio,0);
			break;
		case CCLOGIC_CC_UNKNOWN:
		default:
			pr_debug("%s-->usb_ss signal to unknown\n",__func__);
			ret = -1;
			break;
		}
	}

	/*
	   if(state->vbus){
	   switch(state->charger){
	   case CCLOGIC_CURRENT_NONE:
	   case CCLOGIC_CURRENT_DEFAULT:
	   case CCLOGIC_CURRENT_MEDIUM:
	   case CCLOGIC_CURRENT_ACCESSORY:
	   case CCLOGIC_CURRENT_HIGH:
	   }
	   }
	 */

	switch(state->device){
	case CCLOGIC_NO_DEVICE://nothing attached
		pr_debug("%s-->nothing attached,switch to UART\n",__func__);
		cclogic_vbus_power_on(pdata,false);
		cclogic_func_set(p,CCLOGIC_FUNC_UART);
		ret = pdata->ops->chip_config(pdata->i2c_client);
		break;
	case CCLOGIC_USB_DEVICE:
		pr_debug("%s-->function switch set to usb host\n",__func__);
		cc_otg_state = 1;
		cclogic_func_set(p,CCLOGIC_FUNC_HIZ);
		cclogic_vbus_power_on(pdata,true);
		mdelay(300);
		cclogic_func_set(p,CCLOGIC_FUNC_USB);
		break;
	case CCLOGIC_USB_HOST:
		pr_debug("%s-->function switch set to usb device\n",__func__);
		cclogic_vbus_power_on(pdata,false);
		cclogic_func_set(p,CCLOGIC_FUNC_USB);
		break;
	case CCLOGIC_DEBUG_DEVICE:
		pr_debug("%s-->function switch set to debug device,vbus=%d\n", __func__,state->vbus);
		cclogic_func_set(p,CCLOGIC_FUNC_HIZ);
		break;
	case CCLOGIC_AUDIO_DEVICE:
		pr_debug("%s-->function switch set to audio device(HiZ)\n",__func__);
		cclogic_func_set(p,CCLOGIC_FUNC_HIZ);
		break;
	case CCLOGIC_DEVICE_UNKNOWN:
	default:
		dev_err(&pdata->i2c_client->dev,"%s-->function unknown,"
				" switch to HiZ\n",__func__);
		cclogic_func_set(p,CCLOGIC_FUNC_HIZ);
		ret = -1;
	}

out:
	if(pdata->typec_version==11){
		cclogic_class_update_state(&pdata->cdev);
	}

	return ret;
}

/*
 *
 */
static void cclogic_do_work(struct work_struct *w)
{
	struct cclogic_dev *pdata = container_of(w, 
					struct cclogic_dev, work.work);
	int ret=0;
	static int retries = 0;

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	if(pm_runtime_suspended(pdata->dev)){
		return;
	}

	mutex_lock(&cclogic_ops_lock);

	ret = pdata->ops->get_state(pdata->i2c_client,&pdata->state);
	if(ret){
		goto work_end;
	}

	if(pdata->ops->ack_irq){
		ret = pdata->ops->ack_irq(pdata->i2c_client);
		if(ret){
			goto work_end;
		}
	}

	cclogic_patch_state(pdata);
	ret = cclogic_do_real_work(&pdata->state,pdata);

work_end:
	mutex_unlock(&cclogic_ops_lock);

	if(ret || !gpio_get_value(cclogic_priv->platform_data->irq_working)){
		retries++;
		if(retries <= CCLOGIC_MAX_RETRIES){
			schedule_delayed_work(&pdata->work, msecs_to_jiffies(100));
			return;
		}else
			pr_err("[%s][%d] still in error,more than %d retries\n", __func__, __LINE__,CCLOGIC_MAX_RETRIES);
	}


	if(wake_lock_active(&pdata->wakelock)){
		wake_unlock(&pdata->wakelock);
	}
	retries = 0;
}

/*
 *
 */
static void cclogic_do_plug_work(struct work_struct *w)
{
	struct cclogic_dev *pdata = container_of(w, 
					struct cclogic_dev, plug_work.work);
	struct cclogic_platform *p = pdata->platform_data;
	static int retries = 0;

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	if(m_plug_state){
		if(gpio_get_value(pdata->platform_data->irq_plug)){
			if(retries<10){
				retries++;
				schedule_delayed_work(&pdata->plug_work, msecs_to_jiffies(100));
			}else{
				m_plug_state = 0;
				cancel_delayed_work(&cclogic_priv->work);
				flush_delayed_work(&cclogic_priv->work);	
				cclogic_vbus_power_on(pdata,false);
				cclogic_func_set(p,CCLOGIC_FUNC_UART);
				retries = 0;
				if (wake_lock_active(&cclogic_priv->wakelock_plug)){
					pm_runtime_put(pdata->dev);
					wake_unlock(&cclogic_priv->wakelock_plug);
				}
			}
		}else{
			retries = 0;
			if (wake_lock_active(&cclogic_priv->wakelock_plug)){
				wake_unlock(&cclogic_priv->wakelock_plug);
			}
		}
	}else{
		retries = 0;
		if (wake_lock_active(&cclogic_priv->wakelock_plug)){
			pm_runtime_put(pdata->dev);
			wake_unlock(&cclogic_priv->wakelock_plug);
		}
	}
}


/*
 *
 */
static int cclogic_init_gpio(struct cclogic_dev *cclogic_dev)
{
	struct i2c_client *client = cclogic_dev->i2c_client;
	struct cclogic_platform *pdata = cclogic_dev->platform_data;
	int ret = 0;

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	if (gpio_is_valid(pdata->function_switch_gpio1)) {
		ret = gpio_request(pdata->function_switch_gpio1, 
					"cclogic_func_gpio1");
		if (ret) {
			dev_err(&client->dev, 
					"%s-->unable to request gpio [%d]\n",
					__func__,pdata->function_switch_gpio1);
			goto err_gpio1;
		}
		ret = gpio_direction_output(pdata->function_switch_gpio1,0);
		if (ret) {
			dev_err(&client->dev,
				"%s-->unable to set direction for gpio [%d]\n",
				__func__,pdata->function_switch_gpio1);
			goto err_gpio1_dir;
		}
	}

	if (gpio_is_valid(pdata->function_switch_gpio10)) {
		ret = gpio_request(pdata->function_switch_gpio10, 
					"cclogic_func_gpio10");
		if (ret) {
			dev_err(&client->dev, 
					"%s-->unable to request gpio [%d]\n",
					__func__,pdata->function_switch_gpio10);
			goto err_gpio1_dir;
		}
		ret = gpio_direction_output(pdata->function_switch_gpio10,0);
		if (ret) {
			dev_err(&client->dev,
				"%s-->unable to set direction for gpio [%d]\n",
				__func__,pdata->function_switch_gpio10);
			goto err_gpio10_dir;
		}
	}

	if (gpio_is_valid(pdata->function_switch_gpio2)) {
		ret = gpio_request(pdata->function_switch_gpio2, 
					"cclogic_func_gpio2");
		if (ret) {
			dev_err(&client->dev,
				 "%s-->unable to request gpio [%d]\n",
				__func__,pdata->function_switch_gpio2);
			goto err_gpio10_dir;
		}
		ret = gpio_direction_output(pdata->function_switch_gpio2,1);
		if (ret) {
			dev_err(&client->dev,
				"%s-->unable to set direction for gpio [%d]\n",
				__func__,pdata->function_switch_gpio2);
			goto err_gpio2_dir;
		}
	} else {
		ret = -ENODEV;
		dev_err(&client->dev,
			 "%s-->function_switch_gpio2 not provided\n",__func__);
		goto err_gpio10_dir;
	}

	if (gpio_is_valid(pdata->usb_ss_gpio)) {
		ret = gpio_request(pdata->usb_ss_gpio, "usb_ss_gpio");
		if (ret) {
			dev_err(&client->dev, 
					"%s-->unable to request gpio [%d]\n",
					__func__,pdata->usb_ss_gpio);
			goto err_gpio2_dir;
		}
		ret = gpio_direction_output(pdata->usb_ss_gpio,0);
		if (ret) {
			dev_err(&client->dev,
				"%s-->unable to set direction for gpio [%d]\n",
				__func__,pdata->usb_ss_gpio);
			goto err_ss_gpio;
		}
	}

	if (gpio_is_valid(pdata->enb_gpio)) {
		ret = gpio_request(pdata->enb_gpio, "enb_gpio");
		if (ret) {
			dev_err(&client->dev, 
					"%s-->unable to request gpio [%d]\n",
					__func__,pdata->enb_gpio);
			goto err_ss_gpio;
		}
		ret = gpio_direction_output(pdata->enb_gpio,!cclogic_dev->enb);
		if (ret) {
			dev_err(&client->dev, 
				"%s-->unable to set direction for gpio [%d]\n",
				__func__,pdata->enb_gpio);
			goto err_enb_gpio;
		}
	}

	if (gpio_is_valid(pdata->ccchip_power_gpio)) {
		ret = gpio_request(pdata->ccchip_power_gpio, "chip_power_gpio");
		if (ret) {
			dev_err(&client->dev, 
					"%s-->unable to request gpio [%d]\n",
					__func__,pdata->ccchip_power_gpio);
			goto err_enb_gpio;
		}
		ret = gpio_direction_output(pdata->ccchip_power_gpio,1);
		if (ret) {
			dev_err(&client->dev, 
				"%s-->unable to set direction for gpio [%d]\n",
				__func__,pdata->ccchip_power_gpio);
			goto err_cc_power_gpio;
		}
	}

	return ret;

err_cc_power_gpio:
	if (gpio_is_valid(pdata->ccchip_power_gpio))
		gpio_free(pdata->ccchip_power_gpio);	

err_enb_gpio:
	if (gpio_is_valid(pdata->enb_gpio))
		gpio_free(pdata->enb_gpio);	

err_ss_gpio:
	if (gpio_is_valid(pdata->usb_ss_gpio))
		gpio_free(pdata->usb_ss_gpio);

err_gpio2_dir:
	if (gpio_is_valid(pdata->function_switch_gpio2))
		gpio_free(pdata->function_switch_gpio2);

err_gpio10_dir:
	if (gpio_is_valid(pdata->function_switch_gpio10))
		gpio_free(pdata->function_switch_gpio10);

err_gpio1_dir:
	if (gpio_is_valid(pdata->function_switch_gpio1))
		gpio_free(pdata->function_switch_gpio1);
err_gpio1:
	return ret;
}

/*
 *
 */
static int cclogic_remove_gpio(struct cclogic_dev *cclogic_dev)
{
	struct cclogic_platform *pdata = cclogic_dev->platform_data;

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	if (gpio_is_valid(pdata->enb_gpio))
		gpio_free(pdata->enb_gpio); 

	if (gpio_is_valid(pdata->usb_ss_gpio))
		gpio_free(pdata->usb_ss_gpio);

	if (gpio_is_valid(pdata->function_switch_gpio2))
		gpio_free(pdata->function_switch_gpio2);

	if (gpio_is_valid(pdata->function_switch_gpio1))
		gpio_free(pdata->function_switch_gpio1);

	if (gpio_is_valid(pdata->function_switch_gpio10))
		gpio_free(pdata->function_switch_gpio10);

	if (gpio_is_valid(pdata->ccchip_power_gpio))
		gpio_free(pdata->ccchip_power_gpio);	

	return 0;

}

/**
 * cclogic_match_id()
 */
static int cclogic_match_id(const char *name)
{
	int i=0;

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	if(unlikely(!name)){
		pr_err("%s-->No name\n",__func__);
		return -ENODEV;
	}

	if(cclogic_priv && cclogic_priv->platform_data){
		for(i=0;i<cclogic_priv->platform_data->chip_num;i++){
			struct cclogic_of_chip *chip = 
				&cclogic_priv->platform_data->chip[i];

			if(!chip->chip_name){
				pr_err("%s-->%s mismatch\n",__func__,name);
				return -ENODEV;
			}
			if(!strcmp(chip->chip_name, name))
				break;
		}
	}else{
		pr_err("%s-->%s mismatch\n",__func__,name);
		return -ENODEV;
	}

	if(i >= cclogic_priv->platform_data->chip_num){
		pr_err("%s-->%s mismatch\n",__func__,name);
		return -ENODEV;
	}

	return i;

}

/**
 * cclogic_register()
 */
int cclogic_register(struct cclogic_chip *c)
{
	int ret = 0;

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	mutex_lock(&cclogic_ops_lock);
	if(cclogic_priv->ops){
		mutex_unlock(&cclogic_ops_lock);
		return -EINVAL;
	}
	mutex_unlock(&cclogic_ops_lock);

	ret = cclogic_match_id(c->chip_name);
	if(ret<0){
		return ret;
	}

	c->addr = cclogic_priv->platform_data->chip[ret].address;
	cclogic_priv->enb = cclogic_priv->platform_data->chip[ret].enb;

	if (!c->chip_check) {
		return -ENODEV;
	}

	pm_runtime_get_sync(cclogic_priv->dev);
	wake_lock(&cclogic_priv->wakelock_plug);
	mdelay(100);
	
	mutex_lock(&cclogic_ops_lock);

	cclogic_priv->i2c_client->addr = c->addr;

	if(c->chip_reset){
		ret = c->chip_reset(cclogic_priv->i2c_client);
		if(ret){
			goto err_ret;
		}
	}
	ret = c->chip_check(cclogic_priv->i2c_client);
	if(ret){
		goto err_ret;
	}
	ret = c->chip_config(cclogic_priv->i2c_client);
	if(ret){
		goto err_ret;
	}

	pr_info("%s select chip:%s\n",__func__,c->chip_name);

	cclogic_priv->ops = c;
	cclogic_priv->typec_version = c->typec_version;

	mutex_unlock(&cclogic_ops_lock);

	if(cclogic_priv->typec_version==11){
		//cclogic_priv->cdev.name = c->chip_name;
		cclogic_priv->cdev.name = "otg_default";
		ret = cclogic_class_register(&cclogic_priv->cdev);
		if (ret) {
			dev_err(cclogic_priv->dev,
					"Failed to setup class dev for cclogic driver");
			goto err_ret1;
		}
	}

	cclogic_irq_enable(cclogic_priv,true);

	m_plug_state = 1;
	schedule_delayed_work(&cclogic_priv->plug_work, 0);

	return 0;

err_ret:
	mutex_unlock(&cclogic_ops_lock);
err_ret1:
	pm_runtime_put(cclogic_priv->dev);
	return ret;
}
EXPORT_SYMBOL(cclogic_register);

/**
 * cclogic_unregister()
 */
void cclogic_unregister(struct cclogic_chip *c)
{
	pr_debug("[%s][%d]\n", __func__, __LINE__);

	if(cclogic_priv->ops != c){
		return;
	}

	cancel_delayed_work(&cclogic_priv->work);
	flush_delayed_work(&cclogic_priv->work);	
	cancel_delayed_work(&cclogic_priv->plug_work);
	flush_delayed_work(&cclogic_priv->plug_work);	

	cclogic_irq_enable(cclogic_priv,false);

	if (wake_lock_active(&cclogic_priv->wakelock)){
		wake_unlock(&cclogic_priv->wakelock);
	}
	pm_runtime_put(cclogic_priv->dev);

	mutex_lock(&cclogic_ops_lock);
	cclogic_priv->ops = NULL;
	mutex_unlock(&cclogic_ops_lock);

	if(cclogic_priv->typec_version==11){
		cclogic_priv->typec_version = 10;
		cclogic_class_unregister(&cclogic_priv->cdev);
	}

	return;
}
EXPORT_SYMBOL(cclogic_unregister);


static struct attribute *cclogic_attrs[] = {
#ifdef DEV_STAGE_DEBUG 
	&dev_attr_chip_power.attr,
	&dev_attr_status.attr,
	&dev_attr_enable.attr,
#endif
	&dev_attr_cc.attr,
	NULL,
};

static struct attribute_group cclogic_attr_group = {
	.attrs = cclogic_attrs,
};

/**
 * cclogic_probe()
 */
static int cclogic_probe(struct i2c_client *client,
				   const struct i2c_device_id *dev_id)
{
	int ret = 0;
	struct cclogic_dev *cclogic_dev = cclogic_priv;
	struct cclogic_platform *platform_data;

	pr_info("%s start\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev,"%s: i2c check failed\n", __func__);
		ret = -ENODEV;
		goto err_i2c;
	}

	if(!cclogic_dev){
		ret = -ENODEV;
		goto err_i2c;
	}

	memset(cclogic_dev,0,sizeof(*cclogic_dev));		

	if (client->dev.of_node) {
		platform_data = devm_kzalloc(&client->dev,
					sizeof(*platform_data),GFP_KERNEL);
		if (!platform_data) {
			dev_err(&client->dev,"%s-->Failed to allocate memory\n",
						__func__);
			ret = -ENOMEM;
			goto err_i2c;
		}

		ret = cclogic_parse_dt(&client->dev, platform_data);
		if (ret){
			dev_err(&client->dev,"%s-->Failed parse dt\n",__func__);
			goto err_i2c;
		}
	} else {
		platform_data = client->dev.platform_data;
	}

	cclogic_dev->pin = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(cclogic_dev->pin)) {
		ret = -ENODEV;
		goto err_i2c;
	}

	cclogic_dev->pin_active = pinctrl_lookup_state(cclogic_dev->pin, "cc_active");
	if (IS_ERR(cclogic_dev->pin_active)) {
		dev_err(&client->dev, "Unable find cc_active\n");
		ret = PTR_ERR(cclogic_dev->pin_active);
		goto err_i2c;
	}

	cclogic_dev->pin_suspend = pinctrl_lookup_state(cclogic_dev->pin, "cc_sleep");
	if (IS_ERR(cclogic_dev->pin_suspend)) {
		dev_err(&client->dev, "Unable find cc_sleep\n");
		ret = PTR_ERR(cclogic_dev->pin_suspend);
		goto err_i2c;
	}

	ret = pinctrl_select_state(cclogic_dev->pin, cclogic_dev->pin_active);
	if (ret < 0) {
		dev_err(&client->dev, "Unable select active state in pinctrl\n");
		goto err_i2c;
	}


	cclogic_dev->platform_data = platform_data;
	cclogic_dev->i2c_client   = client;
	cclogic_dev->dev = &client->dev;

	i2c_set_clientdata(client,cclogic_dev);

	pm_runtime_set_suspended(&client->dev);

	ret = cclogic_regulator_configure(cclogic_dev, true);
	if (ret < 0) {
		dev_err(&client->dev, "%s-->Failed to configure regulators\n",
					__func__);
		goto err_i2c;
	}

	ret = cclogic_power_on(cclogic_dev, true);
	if (ret < 0) {
		dev_err(&client->dev, "%s-->Failed to power on\n",__func__);
		goto err_regulator_conf;
	}

	ret = cclogic_init_gpio(cclogic_dev);
	if(ret){
		dev_err(&client->dev,"%s-->error in set gpio\n",__func__);
		goto err_regulator_on;
	}

	pm_runtime_enable(&client->dev);

	if (gpio_is_valid(platform_data->irq_working)) {
		/* configure cclogic irq working */
		ret = gpio_request(platform_data->irq_working, "cclogic_irq_working");
		if (ret) {
			dev_err(&client->dev, 
					"%s-->unable to request gpio [%d]\n",
					__func__,platform_data->irq_working);
			goto err_set_gpio;
		}
		ret = gpio_direction_input(platform_data->irq_working);
		if (ret) {
			dev_err(&client->dev, 
				"%s-->unable to set direction for gpio [%d]\n",
				__func__,platform_data->irq_working);
			goto err_irq_working_dir;
		}
	} else {
		dev_err(&client->dev, "%s-->irq gpio not provided\n",__func__);
		goto err_set_gpio;
	}
	cclogic_dev->irq_working = gpio_to_irq(platform_data->irq_working);

	if (gpio_is_valid(platform_data->irq_plug)) {
		/* configure cclogic irq plug */
		ret = gpio_request(platform_data->irq_plug, "cclogic_irq_plug");
		if (ret) {
			dev_err(&client->dev, 
					"%s-->unable to request gpio [%d]\n",
					__func__,platform_data->irq_plug);
			goto err_irq_working_dir;
		}
		ret = gpio_direction_input(platform_data->irq_plug);
		if (ret) {
			dev_err(&client->dev, 
				"%s-->unable to set direction for gpio [%d]\n",
				__func__,platform_data->irq_plug);
			goto err_irq_plug_dir;
		}
		cclogic_dev->irq_plug = gpio_to_irq(platform_data->irq_plug);
	} else {
		dev_err(&client->dev, "%s-->irq plug gpio not provided\n",__func__);
		goto err_irq_working_dir;
	}


	wake_lock_init(&cclogic_dev->wakelock, WAKE_LOCK_SUSPEND,
				"cclogic_wakelock");
	wake_lock_init(&cclogic_dev->wakelock_plug, WAKE_LOCK_SUSPEND,
				"cclogic_wakelock_plug");

	device_init_wakeup(cclogic_dev->dev, 1);

	INIT_DELAYED_WORK(&cclogic_dev->work, cclogic_do_work);
	INIT_DELAYED_WORK(&cclogic_dev->plug_work, cclogic_do_plug_work);

	ret = sysfs_create_group(&client->dev.kobj, &cclogic_attr_group);
	if (ret) {
		dev_err(&client->dev,
				"%s-->Unable to create sysfs for cclogic,"
				" errors: %d\n", __func__, ret);
		goto err_chip_check;
	}

	ret = request_threaded_irq(cclogic_dev->irq_working,NULL, cclogic_irq, 
			cclogic_dev->platform_data->irq_working_flags|IRQF_ONESHOT, DRIVER_NAME, 
			cclogic_dev);
	if (ret) {
		dev_err(&client->dev, 
				"%s: Failed to create working-irq thread\n", __func__);
		goto err_irq_req;
	}

	if (gpio_is_valid(platform_data->irq_plug)) {
		ret = request_threaded_irq(cclogic_dev->irq_plug,NULL, cclogic_plug_irq, 
				cclogic_dev->platform_data->irq_plug_flags|IRQF_ONESHOT, DRIVER_NAME, 
				cclogic_dev);
		if (ret) {
			dev_err(&client->dev, 
					"%s: Failed to create plug-irq thread\n", __func__);
			goto err_irq_enable;
		}
	}
	disable_irq(cclogic_dev->irq_working);
	if (gpio_is_valid(cclogic_dev->platform_data->irq_plug)) {
		disable_irq(cclogic_dev->irq_plug);
	}

	pr_info("%s Success\n", __func__);

	return 0;

	cancel_delayed_work_sync(&cclogic_dev->work);
	cancel_delayed_work_sync(&cclogic_dev->plug_work);
	cclogic_irq_enable(cclogic_dev,false);

	if (gpio_is_valid(platform_data->irq_plug))
		free_irq(cclogic_dev->irq_plug,cclogic_dev);
err_irq_enable:
	free_irq(cclogic_dev->irq_working,cclogic_dev);
err_irq_req:
	sysfs_remove_group(&client->dev.kobj, &cclogic_attr_group);
err_chip_check:	
	device_init_wakeup(cclogic_dev->dev, 0);
	wake_lock_destroy(&cclogic_dev->wakelock);
	wake_lock_destroy(&cclogic_dev->wakelock_plug);
err_irq_plug_dir:
	if (gpio_is_valid(platform_data->irq_plug))
		gpio_free(platform_data->irq_plug);
err_irq_working_dir:
	if (gpio_is_valid(platform_data->irq_working))
		gpio_free(platform_data->irq_working);
err_set_gpio:
	cclogic_remove_gpio(cclogic_dev);
err_regulator_on:
	cclogic_power_on(cclogic_dev, false);
err_regulator_conf:
	cclogic_regulator_configure(cclogic_dev, false);
err_i2c:
	dev_err(&client->dev,"%s Failed\n", __func__);

	return ret;
}



/**
 * cclogic_remove()
 */
static int cclogic_remove(struct i2c_client *client)
{
	struct cclogic_dev *cclogic_dev;

	pr_info("%s\n", __func__);

	cclogic_dev = i2c_get_clientdata(client);

	cclogic_irq_enable(cclogic_dev,false);	

	cclogic_vbus_power_on(cclogic_dev,false);

	sysfs_remove_group(&client->dev.kobj, &cclogic_attr_group);

	device_init_wakeup(cclogic_dev->dev, 0);
	wake_lock_destroy(&cclogic_dev->wakelock);
	wake_lock_destroy(&cclogic_dev->wakelock_plug);

	cancel_delayed_work_sync(&cclogic_dev->work);	
	cancel_delayed_work_sync(&cclogic_dev->plug_work);

	free_irq(cclogic_dev->irq_working,cclogic_dev);
	if (gpio_is_valid(cclogic_dev->platform_data->irq_plug))
		free_irq(cclogic_dev->irq_plug,cclogic_dev);

	if (gpio_is_valid(cclogic_dev->platform_data->ccchip_power_gpio)) 
		gpio_set_value(cclogic_dev->platform_data->ccchip_power_gpio,0);

	disable_irq_wake(cclogic_dev->irq_working);
	if (gpio_is_valid(cclogic_dev->platform_data->irq_plug)){
		disable_irq_wake(cclogic_dev->irq_plug);
	}

	cclogic_remove_gpio(cclogic_dev);

	if (gpio_is_valid(cclogic_dev->platform_data->irq_working))
		gpio_free(cclogic_dev->platform_data->irq_working);
	if (gpio_is_valid(cclogic_dev->platform_data->irq_plug))
		gpio_free(cclogic_dev->platform_data->irq_plug);

	pinctrl_select_state(cclogic_dev->pin, cclogic_dev->pin_suspend);

	cclogic_power_on(cclogic_dev, false);

	cclogic_regulator_configure(cclogic_dev, false);

        pm_runtime_disable(&client->dev);
        pm_runtime_set_suspended(&client->dev);

	return 0;
}

#ifdef CONFIG_PM_RUNTIME
/*
 *
 */
static int cclogic_runtime_suspend(struct device *dev)
{
	struct cclogic_dev *cclogic_dev = dev_get_drvdata(dev);
	struct cclogic_platform *pdata = cclogic_dev->platform_data;

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	if (gpio_is_valid(pdata->enb_gpio)){
		gpio_direction_output(pdata->enb_gpio,!cclogic_dev->enb);
	}

        return 0;
}

/*
 *
 */
static int cclogic_runtime_resume(struct device *dev)
{
	struct cclogic_dev *cclogic_dev = dev_get_drvdata(dev);
	struct cclogic_platform *pdata = cclogic_dev->platform_data;

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	if (gpio_is_valid(pdata->enb_gpio)){
		gpio_direction_output(pdata->enb_gpio,cclogic_dev->enb);
	}

        return 0;
}

#endif

#ifdef CONFIG_PM_SLEEP
/*
 *
 */
static int cclogic_suspend(struct device *dev)
{
	struct cclogic_dev *cclogic_dev = dev_get_drvdata(dev);
	struct cclogic_platform *pdata = cclogic_dev->platform_data;

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	disable_irq(cclogic_dev->irq_working);
	enable_irq_wake(cclogic_dev->irq_working);
	if (gpio_is_valid(pdata->irq_plug)){
		disable_irq(cclogic_dev->irq_plug);
		enable_irq_wake(cclogic_dev->irq_plug);
	}

	//if(!gpio_get_value(pdata->platform_data->irq_plug)){
	//}
	cclogic_power_on(cclogic_dev, false);
	pinctrl_select_state(cclogic_dev->pin, cclogic_dev->pin_suspend);
	//if (gpio_is_valid(pdata->ccchip_power_gpio)) 
	//	gpio_set_value(pdata->ccchip_power_gpio,0);
	
	return 0;
}

/*
 *
 */
static int cclogic_resume(struct device *dev)
{
	struct cclogic_dev *cclogic_dev = dev_get_drvdata(dev);
	struct cclogic_platform *pdata = cclogic_dev->platform_data;

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	pinctrl_select_state(cclogic_dev->pin, cclogic_dev->pin_active);

	//if (gpio_is_valid(pdata->ccchip_power_gpio)) 
	//	gpio_set_value(pdata->ccchip_power_gpio,1);

	cclogic_power_on(cclogic_dev, true);

	disable_irq_wake(cclogic_dev->irq_working);
	enable_irq(cclogic_dev->irq_working);
	if (gpio_is_valid(pdata->irq_plug)){
		disable_irq_wake(cclogic_dev->irq_plug);
		enable_irq(cclogic_dev->irq_plug);
	}

	return 0;
}
#endif

void cclogic_shutdown(struct i2c_client *client)
{
	cclogic_remove(client);
}

static const struct dev_pm_ops cclogic_pm_ops = {
        SET_SYSTEM_SLEEP_PM_OPS(cclogic_suspend, cclogic_resume)
        SET_RUNTIME_PM_OPS(cclogic_runtime_suspend, cclogic_runtime_resume, NULL)
};

static const struct i2c_device_id cclogic_id_table[] = {
	{DRIVER_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, cclogic_id_table);

#ifdef CONFIG_OF
static struct of_device_id cclogic_match_table[] = {
	{ .compatible = "typec,cclogic", },
	{ },
};
MODULE_DEVICE_TABLE(of, cclogic_match_table);
#else
#define cclogic_match_table NULL
#endif

static struct i2c_driver cclogic_driver = {
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
		.pm     = &cclogic_pm_ops,
		.of_match_table = cclogic_match_table,
	},
	.probe	 = cclogic_probe,
	.remove	 = cclogic_remove,
	.shutdown = cclogic_shutdown,
	.id_table = cclogic_id_table,
};

static int __init cclogic_init(void)
{
	pr_debug("[%s][%d]\n", __func__, __LINE__);

	mutex_init(&cclogic_ops_lock);

	cclogic_priv = kzalloc(sizeof(*cclogic_priv), GFP_KERNEL);
	if (cclogic_priv == NULL) {
		pr_err("%s-->failed to allocate memory for module data\n",
				__func__);
		return -ENOMEM;
	}

	return i2c_add_driver(&cclogic_driver);
}

static void __exit cclogic_exit(void)
{
	pr_debug("[%s][%d]\n", __func__, __LINE__);

	i2c_del_driver(&cclogic_driver);

	if(cclogic_priv)
		kfree(cclogic_priv);
	cclogic_priv = NULL;

	return;
}

module_init(cclogic_init);
module_exit(cclogic_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Yang ShaoYing <yangsy2@lenovo.com>");
MODULE_DESCRIPTION("Drivers core for CC-Logic");
MODULE_ALIAS("platform:cc-logic");
