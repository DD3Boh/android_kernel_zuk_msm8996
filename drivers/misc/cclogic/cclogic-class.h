#ifndef __CCLOGIC_CLASS_H__
#define __CCLOGIC_CLASS_H__

struct cclogic_class_dev{
        const char      *name;
        struct device   *dev;
        int             index;
	unsigned int    support;
	struct device_attribute device_supported_modes_attr;
	struct device_attribute device_mode_attr;
	struct device_attribute device_power_role_attr;
	struct device_attribute device_data_role_attr;
};

extern int cclogic_class_register(struct cclogic_class_dev *dev);
extern void cclogic_class_unregister(struct cclogic_class_dev *dev);
extern void cclogic_class_update_state(struct cclogic_class_dev *cdev);

#endif
