#ifndef __CONFIG_LINUX_KERNEL_INC__
#define __CONFIG_LINUX_KERNEL_INC__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/crc32.h>
#include <linux/ftrace.h>
#include <sound/pcm.h>

/* 
	i2c transaction on Linux limited to 64k
	(See Linux kernel documentation: Documentation/i2c/writing-clients)
*/
#define MAX_I2C_BUFFER_SIZE 65536

/* dbgprint.h */
#define PRINT(fmt) "%s: " fmt, __func__

#define _ASSERT(e)
#define PRINT_ASSERT(e)if ((e)) printk(KERN_ERR "PrintAssert:%s (%s:%d) error code:%d\n",__FUNCTION__,__FILE__,__LINE__, e)

#define TFA98XX_MAX_REGISTER              0xf0

#define TFA98XX_FLAG_DSP_START_ON_MUTE	(1 << 0)
#define TFA98XX_FLAG_SKIP_INTERRUPTS	(1 << 1)
#define TFA98XX_FLAG_SAAM_AVAILABLE	(1 << 2)
#define TFA98XX_FLAG_STEREO_DEVICE	(1 << 3)
#define TFA98XX_FLAG_MULTI_MIC_INPUTS	(1 << 4)
#define TFA98XX_FLAG_TAPDET_AVAILABLE	(1 << 5)
#define TFA98XX_FLAG_TFA9890_FAM_DEV	(1 << 6)

#define TFA98XX_NUM_RATES		9
/* DSP init status */
enum tfa98xx_dsp_init_state {
	TFA98XX_DSP_INIT_STOPPED,	/* DSP not running */
	TFA98XX_DSP_INIT_RECOVER,	/* DSP error detected at runtime */
	TFA98XX_DSP_INIT_FAIL,		/* DSP init failed */
	TFA98XX_DSP_INIT_PENDING,	/* DSP start requested */
	TFA98XX_DSP_INIT_DONE,		/* DSP running */
	TFA98XX_DSP_INIT_INVALIDATED,	/* DSP was running, requires re-init */
};

enum tfa98xx_dsp_fw_state {
       TFA98XX_DSP_FW_NONE = 0,
       TFA98XX_DSP_FW_PENDING,
       TFA98XX_DSP_FW_FAIL,
       TFA98XX_DSP_FW_OK,
};

struct tfa98xx_firmware {
	void			*base;
	struct tfa98xx_device	*dev;
	char			name[9];	//TODO get length from tfa parameter defs
};

enum tfa98xx_dev_type {
	TFA98XX_DEV_MAX1 = 1,
	TFA98XX_DEV_MAX2,
};


struct tfa98xx {
	struct regmap *regmap;
	struct i2c_client *i2c;
	struct regulator *vdd;
	struct snd_soc_codec *codec;
	struct workqueue_struct *tfa98xx_wq;
	struct delayed_work init_work;
	struct delayed_work monitor_work;
	struct delayed_work interrupt_work;
	struct delayed_work tapdet_work;
	struct mutex dsp_lock;
	int dsp_init;
	int dsp_fw_state;
	int sysclk;
	int rst_gpio;
	u16 rev;
	int has_drc;
	int audio_mode;
	struct tfa98xx_firmware fw;
	char *fw_name;
	enum tfa98xx_dev_type dev_type;
	int rate;
	wait_queue_head_t wq;
	struct device *dev;
	unsigned int init_count;
	int pstream;
	int cstream;
	struct input_dev *input;
	bool tapdet_enabled;		/* service enabled */
	bool tapdet_open;		/* device file opened */
	unsigned int tapdet_profiles;	/* tapdet profile bitfield */
	bool tapdet_poll;		/* tapdet running on polling mode */

	unsigned int rate_constraint_list[TFA98XX_NUM_RATES];
	struct snd_pcm_hw_constraint_list rate_constraint;

	int reset_gpio;
	int power_gpio;
	int irq_gpio;

	int handle;

#ifdef CONFIG_DEBUG_FS
	struct dentry *dbg_dir;
#endif
	u8 reg;

	unsigned int count_wait_for_source_state;
	unsigned int count_noclk;
	unsigned int flags;
};

#endif /* __CONFIG_LINUX_KERNEL_INC__ */

