#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/err.h>

#include "cclogic-class.h"
#include "cclogic-core.h"

struct class *cclogic_class;
static atomic_t device_count;


// USB port's supported modes.  (read-only)
// Contents: "", "ufp", "dfp", or "ufp dfp".
static ssize_t cclogic_supported_modes_show(struct device *dev, 
		struct device_attribute *attr, char *buf)
{
	struct cclogic_class_dev *cdev = (struct cclogic_class_dev *) dev_get_drvdata(dev);

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	if(cdev->support & (CCLOGIC_SUPPORT_MODE_DUAL))
		return sprintf(buf, "ufp dfp");
	else if (cdev->support & CCLOGIC_SUPPORT_MODE_UFP)
		return sprintf(buf, "ufp");
	else if (cdev->support & CCLOGIC_SUPPORT_MODE_DFP)
		return sprintf(buf, "dfp");
	else{
		*buf='\0';
		return 0;
	}
}

// USB port's current mode.  (read-write if configurable)
// Contents: "", "ufp", or "dfp".
static ssize_t cclogic_mode_show(struct device *dev, 
		struct device_attribute *attr, char *buf)
{
	struct cclogic_class_dev *cdev = (struct cclogic_class_dev *) dev_get_drvdata(dev);
	struct cclogic_dev *cclogic_dev = container_of(cdev, struct cclogic_dev, cdev);
	struct cclogic_state *pstate = &cclogic_dev->state;
	int mode;

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	mode = cclogic_get_connected_mode(pstate);
	if(mode == CCLOGIC_MODE_UFP)
		return sprintf(buf,"ufp");
	else if (mode == CCLOGIC_MODE_DFP)
		return sprintf(buf,"dfp");
	else{
		*buf = '\0';
		return 0;
	}
}

// USB port's current mode.  (read-write if configurable)
// Contents: "", "ufp", or "dfp".
static ssize_t cclogic_mode_store(struct device *dev, 
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct cclogic_class_dev *cdev = (struct cclogic_class_dev *) dev_get_drvdata(dev);
	struct cclogic_dev *cclogic_dev = container_of(cdev, struct cclogic_dev, cdev);

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	if(!strcmp(buf,"ufp")){
		cclogic_set_mode(cclogic_dev, CCLOGIC_MODE_UFP);
	}else if(!strcmp(buf,"dfp")){
		cclogic_set_mode(cclogic_dev, CCLOGIC_MODE_DFP);
	}

	return count;
}

// USB port's current power role.  (read-write if configurable)
// Contents: "", "source", or "sink".
static ssize_t cclogic_power_role_show(struct device *dev, 
		struct device_attribute *attr, char *buf)
{
	struct cclogic_class_dev *cdev = (struct cclogic_class_dev *) dev_get_drvdata(dev);
	struct cclogic_dev *cclogic_dev = container_of(cdev, struct cclogic_dev, cdev);
	struct cclogic_state *pstate = &cclogic_dev->state;

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	if(cclogic_get_power_role(pstate)==CCLOGIC_POWER_SINK)
		return sprintf(buf,"sink");
	else if(cclogic_get_power_role(pstate)==CCLOGIC_POWER_SOURCE)
		return sprintf(buf,"source");
	else{
		*buf = '\0';
		return 0;
	}
}

// USB port's current power role.  (read-write if configurable)
// Contents: "", "source", or "sink".
static ssize_t cclogic_power_role_store(struct device *dev, 
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct cclogic_class_dev *cdev = (struct cclogic_class_dev *) dev_get_drvdata(dev);
	struct cclogic_dev *cclogic_dev = container_of(cdev, struct cclogic_dev, cdev);

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	if(!strcmp(buf,"sink")){
		cclogic_set_power_role(cclogic_dev, CCLOGIC_POWER_SINK);
	}else if(!strcmp(buf,"source")){
		cclogic_set_power_role(cclogic_dev, CCLOGIC_POWER_SOURCE);
	}

	return count;
}

// USB port's current data role.  (read-write if configurable)
// Contents: "", "host", or "device".
static ssize_t cclogic_data_role_show(struct device *dev, 
		struct device_attribute *attr, char *buf)
{
	struct cclogic_class_dev *cdev = (struct cclogic_class_dev *) dev_get_drvdata(dev);
	struct cclogic_dev *cclogic_dev = container_of(cdev, struct cclogic_dev, cdev);
	struct cclogic_state *pstate = &cclogic_dev->state;

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	if(cclogic_get_data_role(pstate)==CCLOGIC_DATA_HOST)
		return sprintf(buf,"host");
	else if(cclogic_get_data_role(pstate)==CCLOGIC_DATA_DEVICE)
		return sprintf(buf,"device");
	else{
		*buf = '\0';
		return 0;
	}
}

// USB port's current data role.  (read-write if configurable)
// Contents: "", "host", or "device".
static ssize_t cclogic_data_role_store(struct device *dev, 
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct cclogic_class_dev *cdev = (struct cclogic_class_dev *) dev_get_drvdata(dev);
	struct cclogic_dev *cclogic_dev = container_of(cdev, struct cclogic_dev, cdev);

	pr_debug("[%s][%d]\n", __func__, __LINE__);

	if(!strcmp(buf,"host")){
		cclogic_set_data_role(cclogic_dev, CCLOGIC_DATA_HOST);
	}else if(!strcmp(buf,"device")){
		cclogic_set_data_role(cclogic_dev, CCLOGIC_DATA_DEVICE);
	}

	return count;
}



void cclogic_class_update_state(struct cclogic_class_dev *cdev)
{
	kobject_uevent(&cdev->dev->kobj, KOBJ_CHANGE);
}
EXPORT_SYMBOL(cclogic_class_update_state);

static int create_cclogic_class(void)
{
	if (!cclogic_class) {
		cclogic_class = class_create(THIS_MODULE, "dual_role_usb");
		if (IS_ERR(cclogic_class))
			return PTR_ERR(cclogic_class);
	}

	return 0;
}

int cclogic_class_register(struct cclogic_class_dev *cdev)
{
	int ret;
	struct cclogic_dev *cclogic_dev = container_of(cdev, struct cclogic_dev, cdev);

	if (!cclogic_class) {
		ret = create_cclogic_class();
		if (ret < 0)
			return ret;
	}

	cdev->index = atomic_inc_return(&device_count);
	cdev->dev = device_create(cclogic_class, NULL,
		MKDEV(0, cdev->index), NULL, cdev->name);
	if (IS_ERR(cdev->dev))
		return PTR_ERR(cdev->dev);

	cdev->support = cclogic_dev->ops->support;

	cdev->device_supported_modes_attr.attr.name = "supported_modes";
	cdev->device_supported_modes_attr.attr.mode = S_IRUGO;
	cdev->device_supported_modes_attr.show = cclogic_supported_modes_show;
	sysfs_attr_init(&cdev->device_supported_modes_attr.attr);
	ret = device_create_file(cdev->dev, &cdev->device_supported_modes_attr);
	if (ret < 0)
		goto err_create_file_1;

	cdev->device_mode_attr.attr.name = "mode";
	cdev->device_mode_attr.attr.mode = S_IRUGO;
	cdev->device_mode_attr.show = cclogic_mode_show;
	if((cdev->support & CCLOGIC_SUPPORT_MODE_DUAL) == CCLOGIC_SUPPORT_MODE_DUAL){
		cdev->device_mode_attr.attr.mode |= S_IWUSR;
		cdev->device_mode_attr.store = cclogic_mode_store;
	}
        sysfs_attr_init(&cdev->device_mode_attr.attr);
	ret = device_create_file(cdev->dev, &cdev->device_mode_attr);
	if (ret < 0)
		goto err_create_file_2;

	cdev->device_power_role_attr.attr.name = "power_role";
	cdev->device_power_role_attr.attr.mode = S_IRUGO;
	cdev->device_power_role_attr.show = cclogic_power_role_show;
	if((cdev->support & CCLOGIC_SUPPORT_POWER_SWAP) == CCLOGIC_SUPPORT_POWER_SWAP){
		cdev->device_power_role_attr.attr.mode |= S_IWUSR;
		cdev->device_power_role_attr.store = cclogic_power_role_store;
	}
        sysfs_attr_init(&cdev->device_power_role_attr.attr);
	ret = device_create_file(cdev->dev, &cdev->device_power_role_attr);
	if (ret < 0)
		goto err_create_file_3;

	cdev->device_data_role_attr.attr.name = "data_role";
	cdev->device_data_role_attr.attr.mode = S_IRUGO;
	cdev->device_data_role_attr.show = cclogic_data_role_show;
	if((cdev->support & CCLOGIC_SUPPORT_DATA_SWAP) == CCLOGIC_SUPPORT_DATA_SWAP){
		cdev->device_data_role_attr.attr.mode |= S_IWUSR;
		cdev->device_data_role_attr.store = cclogic_data_role_store;
	}
        sysfs_attr_init(&cdev->device_data_role_attr.attr);
	ret = device_create_file(cdev->dev, &cdev->device_data_role_attr);
	if (ret < 0)
		goto err_create_file_4;

	dev_set_drvdata(cdev->dev, cdev);

	return 0;

err_create_file_4:
	device_remove_file(cdev->dev, &cdev->device_power_role_attr);
err_create_file_3:
	device_remove_file(cdev->dev, &cdev->device_mode_attr);
err_create_file_2:
	device_remove_file(cdev->dev, &cdev->device_supported_modes_attr);
err_create_file_1:
	device_destroy(cclogic_class, MKDEV(0, cdev->index));
	printk(KERN_ERR "cclogic-class: Failed to register driver %s\n", cdev->name);

	return ret;
}
EXPORT_SYMBOL(cclogic_class_register);

void cclogic_class_unregister(struct cclogic_class_dev *cdev)
{
	device_remove_file(cdev->dev, &cdev->device_supported_modes_attr);
	device_remove_file(cdev->dev, &cdev->device_mode_attr);
	device_remove_file(cdev->dev, &cdev->device_power_role_attr);
	device_remove_file(cdev->dev, &cdev->device_data_role_attr);
	dev_set_drvdata(cdev->dev, NULL);
	device_destroy(cclogic_class, MKDEV(0, cdev->index));
}
EXPORT_SYMBOL(cclogic_class_unregister);

static int __init cclogic_class_init(void)
{
	return create_cclogic_class();
}

static void __exit cclogic_class_exit(void)
{
	class_destroy(cclogic_class);
}

module_init(cclogic_class_init);
module_exit(cclogic_class_exit);

MODULE_AUTHOR("yangshaoying <yangsy2@zuk.com>");
MODULE_DESCRIPTION("cclogic class driver");
MODULE_LICENSE("GPL");
