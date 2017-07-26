#ifndef _MAX86X00_H_
#define _MAX86X00_H_

#include <linux/max86x00_platform.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/leds.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/timer.h>

#define MAX86100_DEBUG

#define MAX86000_SLAVE_ADDR			0x51
#define MAX86000A_SLAVE_ADDR		0x57

#define MAX86100_SLAVE_ADDR			0x57

/* MAX86000 Registers */
#define MAX86000_INTERRUPT_STATUS	0x00
#define MAX86000_INTERRUPT_ENABLE	0x01

#define MAX86000_FIFO_WRITE_POINTER	0x02
#define MAX86000_OVF_COUNTER		0x03
#define MAX86000_FIFO_READ_POINTER	0x04
#define MAX86000_FIFO_DATA			0x05
#define MAX86000_MODE_CONFIGURATION	0x06
#define MAX86000_SPO2_CONFIGURATION	0x07
#define MAX86000_UV_CONFIGURATION	0x08
#define MAX86000_LED_CONFIGURATION	0x09
#define MAX86000_UV_DATA			0x0F
#define MAX86000_TEMP_INTEGER		0x16
#define MAX86000_TEMP_FRACTION		0x17

#define MAX86000_WHOAMI_REG			0xFE

#define MAX86000_FIFO_SIZE				16

/* MAX86100 Registers */

#define MAX_UV_DATA		2

#define MODE_UV				1
#define MODE_HR				2
#define MODE_SPO2			3
#define MODE_FLEX			7
#define MODE_GEST			8

#define IR_LED_CH			1
#define RED_LED_CH			2
#define NA_LED_CH			3
#define VIOLET_LED_CH		4

#define NUM_BYTES_PER_SAMPLE			3
#define MAX_LED_NUM						4

#define MAX86100_INTERRUPT_STATUS		0x00
#define PWR_RDY_MASK					0x01
#define UV_INST_OVF_MASK				0x02
#define UV_ACCUM_OVF_MASK				0x04
#define UV_RDY_MASK						0x08
#define UV_RDY_OFFSET					0x03
#define PROX_INT_MASK					0x10
#define ALC_OVF_MASK					0x20
#define PPG_RDY_MASK					0x40
#define A_FULL_MASK						0x80

#define MAX86100_INTERRUPT_STATUS_2			0x01
#define THERM_RDY						0x01
#define DIE_TEMP_RDY					0x02

#define MAX86100_INTERRUPT_ENABLE			0x02
#define MAX86100_INTERRUPT_ENABLE_2			0x03

#define MAX86100_FIFO_WRITE_POINTER			0x04
#define MAX86100_OVF_COUNTER				0x05
#define MAX86100_FIFO_READ_POINTER			0x06
#define MAX86100_FIFO_DATA					0x07

#define MAX86100_FIFO_CONFIG				0x08
#define MAX86100_FIFO_A_FULL_MASK		0x0F
#define MAX86100_FIFO_A_FULL_OFFSET		0x00
#define MAX86100_FIFO_ROLLS_ON_MASK		0x10
#define MAX86100_FIFO_ROLLS_ON_OFFSET	0x04
#define MAX86100_SMP_AVE_MASK			0xE0
#define MAX86100_SMP_AVE_OFFSET			0x05


#define MAX86100_MODE_CONFIGURATION		0x09
#define MAX86100_MODE_MASK				0x07
#define MAX86100_MODE_OFFSET			0x00
#define MAX86100_GESTURE_EN_MASK		0x08
#define MAX86100_GESTURE_EN_OFFSET		0x03
#define MAX86100_RESET_MASK				0x40
#define MAX86100_RESET_OFFSET			0x06
#define MAX86100_SHDN_MASK				0x80
#define MAX86100_SHDN_OFFSET			0x07

#define MAX86100_MODE_DIS		0x00
#define MAX86100_MODE_UV		0x01
#define MAX86100_MODE_HR		0x02
#define MAX86100_MODE_HRSPO2	0x03
#define MAX86100_MODE_FLEX		0x07

#define MAX86100_SPO2_CONFIGURATION			0x0A
#define MAX86100_LED_PW_MASK			0x03
#define MAX86100_LED_PW_OFFSET			0x00
#define MAX86100_SPO2_SR_MASK			0x1C
#define MAX86100_SPO2_SR_OFFSET			0x02
#define MAX86100_SPO2_ADC_RGE_MASK		0x60
#define MAX86100_SPO2_ADC_RGE_OFFSET	0x05
#define MAX86100_SPO2_EN_DACX_MASK		0x80
#define MAX86100_SPO2_EN_DACX_OFFSET	0x07

#define MAX86100_UV_CONFIGURATION			0x0B
#define MAX86100_UV_ADC_RGE_MASK		0x03
#define MAX86100_UV_ADC_RGE_OFFSET		0x00
#define MAX86100_UV_SR_MASK				0x04
#define MAX86100_UV_SR_OFFSET			0x02
#define MAX86100_UV_TC_ON_MASK			0x08
#define MAX86100_UV_TC_ON_OFFSET		0x03
#define MAX86100_UV_ACC_CLR_MASK		0x80
#define MAX86100_UV_ACC_CLR_OFFSET		0x07

#define MAX86100_LED1_PA					0x0C
#define MAX86100_LED2_PA					0x0D
#define MAX86100_LED3_PA					0x0E
#define MAX86100_LED4_PA					0x0F
#define MAX86100_PILOT_PA					0x10

/*THIS IS A PLACEHOLDER ENTRY. KEPT HERE TO KEEP LEGACY CODE COMPUILING*/
#define MAX86100_LED_CONFIGURATION			0x10

#define MAX86100_LED_FLEX_CONTROL_1			0x11
#define MAX86100_S1_MASK				0x07
#define MAX86100_S1_OFFSET				0x00
#define MAX86100_S2_MASK				0x70
#define MAX86100_S2_OFFSET				0x04

#define MAX86100_LED_FLEX_CONTROL_2			0x12
#define MAX86100_S3_MASK				0x07
#define MAX86100_S3_OFFSET				0x00
#define MAX86100_S4_MASK				0x70
#define MAX86100_S4_OFFSET				0x04

#define MAX86100_UV_HI_THRESH				0x13
#define MAX86100_UV_LOW_THRESH			0x14
#define MAX86100_UV_ACC_THRESH_HI		0x15
#define MAX86100_UV_ACC_THRESH_MID		0x16
#define MAX86100_UV_ACC_THRESH_LOW		0x17

#define MAX86100_UV_DATA_HI					0x18
#define MAX86100_UV_DATA_LOW				0x19
#define MAX86100_UV_ACC_DATA_HI				0x1A
#define MAX86100_UV_ACC_DATA_MID			0x1B
#define MAX86100_UV_ACC_DATA_LOW			0x1C

#define MAX86100_UV_COUNTER_HI				0x1D
#define MAX86100_UV_COUNTER_LOW				0x1E

#define MAX86100_TEMP_INTEGER				0x1F
#define MAX86100_TEMP_FRACTION				0x20

#define MAX86100_TEMP_CONFIG				0x21
#define MAX86100_TEMP_EN_MASK			0x01
#define MAX86100_TEMP_EN_OFFSET			0x00

#define MAX86100_WHOAMI_REG_REV		0xFE
#define MAX86100_WHOAMI_REG_PART	0xFF

#define MAX86100_FIFO_SIZE				64

/* Self Test */
#define MAX86000_TEST_MODE					0xFF
#define MAX86000_TM_CODE1					0x54
#define MAX86000_TM_CODE2					0x4D
#define MAX86000_TEST_GTST					0x80
#define MAX86000_TEST_ENABLE_IDAC			0x81
#define MAX86000_TEST_ENABLE_PLETH			0x82
#define MAX86000_TEST_LC					0x8B
#define MAX86000_TEST_ALC					0x8F
#define MAX86000_TEST_ROUTE_MODULATOR		0x97
#define MAX86000_TEST_LOOK_MODE_RED			0x98
#define MAX86000_TEST_LOOK_MODE_IR			0x99
#define MAX86000_TEST_IDAC_GAIN				0x9C

#define MAX86000_I2C_RETRY_DELAY	10
#define MAX86000_I2C_MAX_RETRIES	5
#define MAX86000_COUNT_MAX		65532
#define MAX86100_COUNT_MAX		65532

#define MAX86000C_WHOAMI		0x11
#define MAX86000A_REV_ID		0x00
#define MAX86000B_REV_ID		0x04
#define MAX86000C_REV_ID		0x05

#define MAX86100_PART_ID1			0xFF
#define MAX86100_PART_ID2			0x15
#define MAX86100_REV_ID1			0xFE
#define MAX86100_REV_ID2			0x03
#define MAX86100_REV_ID3			0x04

#define MAX86000_DEFAULT_CURRENT	0x55
#define MAX86000A_DEFAULT_CURRENT	0xFF
#define MAX86000C_DEFAULT_CURRENT	0x0F

#define MAX86100_DEFAULT_CURRENT1	0x60 /* RED */
#define MAX86100_DEFAULT_CURRENT2	0x60 /* IR */
#define MAX86100_DEFAULT_CURRENT3	0x60 /* NONE */
#define MAX86100_DEFAULT_CURRENT4	0x60 /* Violet */

#define MAX86100_DEFAULT_UV_SR_INTERVAL	100 /* ms */

#define MODULE_NAME_HRM			"ppg"
#define MODULE_NAME_ACFD		"flicker"
#define MODULE_NAME_UV			"uv"
#define MAX86000_CHIP_NAME		"max86000"
#define MAX86100_CHIP_NAME		"max86100"
#define MAX86100LC_CHIP_NAME	"max86907"
#define VENDOR					"Maxim Integrated"
#define HRM_LDO_ON	1
#define HRM_LDO_OFF	0
#define MAX_EOL_RESULT 132
#define MAX_LIB_VER 20

#define MAX86000_SAMPLE_RATE 4
#define MAX86100_SAMPLE_RATE 1

#define MAX86000_SPO2_HI_RES_EN	1
#define MAX86100_SPO2_ADC_RGE	3


#define MAX86X00_LED1 0
#define MAX86X00_LED2 1

#define MAX86000_LED1_PROX_PA	10000
#define MAX86000_LED2_PROX_PA	0

#define MAX86100_LED1_PROX_PA	0
#define MAX86100_LED2_PROX_PA	5000

#define MAX86000_PROX_ENTER_DB		5
#define MAX86000_PROX_EXIT_DB		(5 * MAX86000_SAMPLE_RATE)
#define MAX86000_PROX_ENTER_THRESH	2000
#define MAX86000_PROX_EXIT_THRESH	6000

#define MAX86100_PROX_ENTER_DB		5
#define MAX86100_PROX_EXIT_DB		(5 * MAX86100_SAMPLE_RATE)
#define MAX86100_PROX_ENTER_THRESH	10000
#define MAX86100_PROX_EXIT_THRESH	35000

#define MAX86000_MIN_CURRENT	0
#define MAX86000_MAX_CURRENT	51000
#define MAX86000_CURRENT_FULL_SCALE		\
		(MAX86000_MAX_CURRENT - MAX86000_MIN_CURRENT)

#define MAX86100_MIN_CURRENT	0
#define MAX86100_MAX_CURRENT	51000
#define MAX86100_CURRENT_FULL_SCALE		\
		(MAX86100_MAX_CURRENT - MAX86100_MIN_CURRENT)

#define MAX86000_MIN_DIODE_VAL	0
#define MAX86000_MAX_DIODE_VAL	((1 << (MAX86000_SPO2_HI_RES_EN ? 16 : 14)) - 1)

#define MAX86100_MIN_DIODE_VAL	0
#define MAX86100_MAX_DIODE_VAL	((1 << 18) - 1)

#define MAX86000_CURRENT_PER_STEP	3188
#define MAX86100_CURRENT_PER_STEP	200

#define MAX86000_AGC_DEFAULT_LED_OUT_RANGE				70
#define MAX86000_AGC_DEFAULT_CORRECTION_COEFF			50
#define MAX86000_AGC_DEFAULT_SENSITIVITY_PERCENT		 14
#define MAX86000_AGC_DEFAULT_MIN_NUM_PERCENT			 (20 * MAX86000_SAMPLE_RATE)

#define MAX86100_AGC_DEFAULT_LED_OUT_RANGE				70
#define MAX86100_AGC_DEFAULT_CORRECTION_COEFF			50
#define MAX86100_AGC_DEFAULT_SENSITIVITY_PERCENT		 14
#define MAX86100_AGC_DEFAULT_MIN_NUM_PERCENT			 (20 * MAX86100_SAMPLE_RATE)

#define ILLEGAL_OUTPUT_POINTER -1
#define CONSTRAINT_VIOLATION -2

#define MAX86100_ENHANCED_UV_GESTURE_MODE	1
#define MAX86100_ENHANCED_UV_HR_MODE		2
#define MAX86100_ENHANCED_UV_MODE			3
#define MAX86100_ENHANCED_UV_EOL_VB_MODE	4
#define MAX86100_ENHANCED_UV_EOL_SUM_MODE	5
#define MAX86100_ENHANCED_UV_EOL_HR_MODE	6
#define MAX86100_ENHANCED_UV_NONE_MODE		7


#define MAX86X100_PPG_MODE		1
#define MAX86X100_ACFD_MODE		2

typedef enum _PART_TYPE {
	PART_TYPE_MAX86000 = 0,
	PART_TYPE_MAX86000A,
	PART_TYPE_MAX86000B,
	PART_TYPE_MAX86000C,
	PART_TYPE_MAX86100A,
	PART_TYPE_MAX86100B,
	PART_TYPE_MAX86100LC,
	PART_TYPE_MAX86100LCA,
} PART_TYPE;


struct max86x00_device_data {
	struct i2c_client *client;
	struct device *dev;
	struct device *hrm_dev;
	struct device *uv_dev;
	struct device *acfd_dev;

	struct input_dev *hrm_input_dev;
	struct input_dev *uv_input_dev;
	struct input_dev *acfd_input_dev;
	struct mutex i2clock;
	struct mutex activelock;
#ifndef CONFIG_SENSORS_MAX86X00_LED_5V
	const char *led_3p3;
#endif
	const char *vdd_1p8;
	struct delayed_work uv_sr_work_queue;

	#ifndef CONFIG_SENSORS_MAX86X00_LED_5V
	struct regulator *regulator_led_3p3;
#endif
	struct regulator *regulator_vdd_1p8;
	struct regulator *vcc_i2c;

	bool *bio_status;
	atomic_t hrm_is_enable;
	atomic_t uv_is_enable;
	atomic_t vb_is_enable;
	atomic_t enhanced_uv_mode;
	atomic_t is_suspend;
	atomic_t irq_enable;
	atomic_t regulator_is_enable;

	bool prox_detect;
	int prox_sample;
	int prox_sum;

	bool agc_is_enable;
	int agc_sum[2];
	u32 agc_current[2];

	s32 agc_led_out_percent;
	s32 agc_corr_coeff;
	s32 agc_min_num_samples;
	s32 agc_sensitivity_percent;
	s32 change_by_percent_of_range[2];
	s32 change_by_percent_of_current_setting[2];
	s32 change_led_by_absolute_count[2];
	s32 reached_thresh[2];

	int (*hrm_led_init)(struct max86x00_device_data*);
	int (*prox_led_init)(struct max86x00_device_data*);
	int (*update_led)(struct max86x00_device_data*, int, int);

	u8 led_current;
	u8 led_current1;
	u8 led_current2;
	u8 led_current3;
	u8 led_current4;
	u16 uv_sr_interval;
	u8 hr_range;
	u8 hr_range2;
	u8 look_mode_ir;
	u8 look_mode_red;
	u8 eol_test_is_enable;
	u8 uv_eol_test_is_enable;
	u8 part_type;
	u8 default_current;
	u8 default_current1;
	u8 default_current2;
	u8 default_current3;
	u8 default_current4;
	u8 test_current_ir;
	u8 test_current_red;
	u8 test_current_led1;
	u8 test_current_led2;
	u8 test_current_led3;
	u8 test_current_led4;
	u8 eol_test_status;
	u16 led;
	u16 sample_cnt;
	int hrm_int;
	int hrm_en;
	int irq;
	int hrm_temp;
	char *eol_test_result;
	char *lib_ver;
	char *uv_lib_ver;
	int ir_sum;
	int r_sum;
	int led_sum[4];
	int spo2_mode;
	u8 flex_mode;
	int num_samples;
	int sum_gesture_data;
};

extern int sensors_create_symlink(struct input_dev *inputdev);
extern void sensors_remove_symlink(struct input_dev *inputdev);
extern struct device *sensors_register(void * drvdata,
	struct device_attribute *attributes[], char *name);
extern void sensors_unregister(struct device *dev,
	struct device_attribute *attributes[]);

#ifdef CONFIG_SENSORS_MAX86X00_DEBUG
void max86x00_debug_remove(void);
int max86x00_debug_init(struct max86x00_device_data *device);
int max86x00_prox_check(struct max86x00_device_data *data, int counts);
int max86x00_hrm_agc(struct max86x00_device_data *data, int counts, int led_num);
#endif

#endif /* _MAX86X00_H_ */
