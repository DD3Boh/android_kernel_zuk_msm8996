/*
* Copyright (C) 2012 lenovo, Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

#include <linux/irq.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/ctype.h>
#include <linux/uaccess.h> /* sys_sync */
#include <linux/rtc.h> /* sys_sync */
#include <linux/cpufreq.h> 
#include <linux/platform_device.h>
#include <linux/err.h>
#include <asm/gpio.h>
#include <linux/regulator/consumer.h>
#define private_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0644,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}

#define TLMM_NUM_GPIO 150
#if 0
#define TLMM_GPIO_SIM 100
extern unsigned __msm_gpio_get_inout(unsigned gpio);
static ssize_t tlmm_sim_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	char *p = buf;
	int output_val = 0;

	output_val = __msm_gpio_get_inout(TLMM_GPIO_SIM);
	p += sprintf(p, "%d", output_val);
	
	return (p - buf);
}

static ssize_t tlmm_sim_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{
	printk(KERN_ERR "%s: no support yet.\n", __func__);

	return -EPERM;
}
#endif

extern int msm_gpio_dump_info(char* buf);
/*
static int gpio_num = -1;
static ssize_t gpio_num_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	char* p = buf;
	p += sprintf(p, "A single gpio[0, 149] to be checked by cat gpio\n");
	p += sprintf(p, "-1 to check all 149 gpios by cat gpio\n");
	p += sprintf(p, "%d\n", gpio_num);
	return p - buf;
}

static ssize_t gpio_num_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{
	int gpio;
	int res;
	
	res = sscanf(buf, "%d", &gpio);
	printk("res=%d. %d\n", res, gpio);
	
	if(res != 1)
		goto gpio_num_store_wrong_para;

	if(gpio >= TLMM_NUM_GPIO)
		goto gpio_num_store_wrong_para;

	gpio_num = gpio;
	printk("gpio_num: %d\n", gpio_num);

	goto gpio_num_store_ok;
		
gpio_num_store_wrong_para:
	printk("Wrong Input.\n");	
	printk("Format: gpio_num\n");
	printk("      gpio_num: 0 ~ 145\n");

gpio_num_store_ok:	
	return n;
}
*/

static ssize_t msm_gpio_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	char* p = buf;
	p += msm_gpio_dump_info(buf);
	return p - buf;
}
extern void msm_gpio_set_config(
				  unsigned gpio,
				  unsigned func,
				  unsigned pull,
				  unsigned dir,
				  unsigned drvstr,
				  unsigned output_val);

static ssize_t msm_gpio_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{
	char pull_c, dir_c;
	int gpio, func, pull, dir, drvstr, output_val;
	int res;
	
	res = sscanf(buf, "%d %d %c %c %d %d", &gpio, &func, &pull_c, &dir_c, &drvstr, &output_val);
	printk("res=%d.  %d %d %c %c %d %d\n", res, gpio, func, pull_c, dir_c, drvstr, output_val);
	if(res < 0) {
		printk("Error: Config failed.\n");
		goto gpio_store_wrong_para;
	}
	//Add a shortcut wrting format to change an output gpio's value
	/*
	if(res == 2 && gpio < TLMM_NUM_GPIO && (func == 0 || func == 1)) {
		output_val = func;
		goto gpio_store_only_output_val;
	}*/
	if((res != 5) && (res != 6))
		goto gpio_store_wrong_para;

	if(gpio >= TLMM_NUM_GPIO)
		goto gpio_store_wrong_para;
		
	if('n' == pull_c)
		pull = 0;
	else if('d' == pull_c)
		pull = 1;
	else if('k' == pull_c)
		pull = 2;
	else if('u' == pull_c)
		pull = 3;
	else 
		goto gpio_store_wrong_para;

	if('i' == dir_c)
		dir = 0;
	else if('o' == dir_c)
		dir = 1;
	else 
		goto gpio_store_wrong_para;
	
	drvstr = drvstr/2 - 1; // 2mA -> 0, 4mA -> 1, 6mA -> 2, ...	
	if(drvstr > 7)
		goto gpio_store_wrong_para;
	
	if(output_val > 1)
		goto gpio_store_wrong_para;
		
	printk("final set: %d %d %d %d %d %d\n", gpio, func, pull, dir, drvstr, output_val);

	//cfg = GPIO_CFG(gpio, func, pull, dir, drvstr, output_val);	
	msm_gpio_set_config(gpio, func, pull, dir, drvstr, output_val);

	
/*	
	if((func == 0)  && (dir == 1)) // gpio output
gpio_store_only_output_val:
		gpio_set_value(gpio, output_val);
		*/
	
	goto gpio_store_ok;
		
gpio_store_wrong_para:
	printk("Wrong Input.\n");	
	printk("Standard Format: gpio_num  function  pull  direction  strength [output_value]\n");
	printk("Shortcut Format: gpio_num  output_value\n");
	printk("      gpio_num: 0 ~ 145\n");
	printk("      function: number, where 0 is GPIO\n");	
	printk("      pull: 'N': NO_PULL, 'D':PULL_DOWN, 'K':KEEPER, 'U': PULL_UP\n");	
	printk("      direction: 'I': Input, 'O': Output\n");	
	printk("      strength:  2, 4, 6, 8, 10, 12, 14, 16\n");	
	printk("      output_value:  Optional. 0 or 1. vaild if GPIO output\n");	
	printk(" e.g.  'echo  20 0 D I 2'  ==> set pin 20 as GPIO input \n");	
	printk(" e.g.  'echo  20 0 D O 2 1'  ==> set pin 20 as GPIO output and the output = 1 \n");	
	printk(" e.g.  'echo  20 1'  ==> set output gpio pin 20 output = 1 \n");

gpio_store_ok:	
	return n;
}
#if 0
/* Set GPIO's sleep config from sysfs */
extern int msm_gpio_before_sleep_table_dump_info(char* buf);
static ssize_t msm_gpio_before_sleep_table_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	char* p = buf;
	p += msm_gpio_before_sleep_table_dump_info(buf);
	return p - buf;
}

extern int msm_gpio_before_sleep_table_set_cfg(unsigned gpio, unsigned cfg);
static ssize_t msm_gpio_before_sleep_table_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{
	char pull_c, dir_c;
	int gpio, func = 0, pull = 0, dir = 0, drvstr = 0, output_val = 0;
	int ignore;
	unsigned cfg;
	int res;
	
	res = sscanf(buf, "%d %d %c %c %d %d", &gpio, &func, &pull_c, &dir_c, &drvstr, &output_val);
	printk("res=%d.  %d %d %c %c %d %d\n", res, gpio, func, pull_c, dir_c, drvstr, output_val);
	
	if(1 == res) { // if only gpio, means ingore(disable) the gpio's sleep config 
		ignore = 1;
		printk("final set: to disable gpio %d sleep config\n", gpio);
	}
	else {
		ignore = 0;
	
		if((res != 5) && (res != 6)) 
			goto msm_gpio_before_sleep_table_store_wrong_para;

		if(gpio >= TLMM_NUM_GPIO)
			goto msm_gpio_before_sleep_table_store_wrong_para;
			
		if('N' == pull_c)
			pull = 0;
		else if('D' == pull_c)
			pull = 1;
		else if('K' == pull_c)
			pull = 2;
		else if('U' == pull_c)
			pull = 3;
		else 
			goto msm_gpio_before_sleep_table_store_wrong_para;

		if('I' == dir_c)
			dir = 0;
		else if('O' == dir_c)
			dir = 1;
		else 
			goto msm_gpio_before_sleep_table_store_wrong_para;
		
		drvstr = drvstr/2 - 1; // 2mA -> 0, 4mA -> 1, 6mA -> 2, ...	
		if(drvstr > 7)
			goto msm_gpio_before_sleep_table_store_wrong_para;
				
		printk("final set: %d %d %d %d %d\n", gpio, func, pull, dir, drvstr);
	}
		 
	cfg = GPIO_CFG(ignore ? 0xff : gpio, func, dir, pull, drvstr);
	res = msm_gpio_before_sleep_table_set_cfg(gpio, cfg | (output_val << 30));
	if(res < 0) {
		printk("Error: Config failed.\n");
		goto msm_gpio_before_sleep_table_store_wrong_para;
	}
	
	goto msm_gpio_before_sleep_table_store_ok;
		
msm_gpio_before_sleep_table_store_wrong_para:
	printk("Wrong Input.\n");	
	printk("Format: refer to gpio's format except  'echo gpio_num > xxx' to disable the gpio's setting\n");	

msm_gpio_before_sleep_table_store_ok:
	return n;
}
#endif

extern int msm_gpio_before_sleep_dump_info(char* buf);
static ssize_t msm_gpio_before_sleep_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	char* p = buf;
	p += msm_gpio_before_sleep_dump_info(buf);
	return p - buf;
}

static ssize_t msm_gpio_before_sleep_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{
	printk(KERN_ERR "%s: no support.\n", __func__);
	return n;
}

extern int vreg_dump_info(char* buf);
static ssize_t vreg_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	char* p = buf;
	p += vreg_dump_info(buf);
	return p - buf;
}

static ssize_t vreg_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{
	printk(KERN_ERR "%s: no support.\n", __func__);
	return n;
}

extern int vreg_before_sleep_dump_info(char* buf);
static ssize_t vreg_before_sleep_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	char* p = buf;
	p += vreg_before_sleep_dump_info(buf);
	return p - buf;
}

static ssize_t vreg_before_sleep_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{
	printk(KERN_ERR "%s: no support.\n", __func__);
	return n;
}

#if 0
static ssize_t clk_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
#if 0
	extern int clk_dump_info(char* buf);
#endif //0
	char *s = buf;

	// show all enabled clocks
#if 0
	//s += sprintf(s, "\nEnabled Clocks:\n");
	s += clk_dump_info(s);
#else
	//Use interface /sys/kernel/debug/clk/enabled_clocks provided by krait instead
	s += sprintf(s, "cat /sys/kernel/debug/clk/enabled_clocks to show Enabled Clocks\n");
#endif //0
	
	return (s - buf);
}

static ssize_t clk_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{
	printk(KERN_ERR "%s: no support.\n", __func__);

	return -EPERM;
}
#endif

extern int wakelock_dump_info(char* buf);
static ssize_t pm_status_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	char *s = buf;
	unsigned long rate; // khz
	int cpu;

	// show CPU clocks
	for (cpu = 0; cpu < nr_cpu_ids; cpu++) {
		s += sprintf(s, "APPS[%d]:", cpu);
		if (cpu_online(cpu)) {
			rate = cpufreq_get(cpu); // khz
			s += sprintf(s, "(%3lu MHz); \n", rate / 1000);
		} else {
			s += sprintf(s, "sleep; \n");
		}
	}

	s += wakelock_dump_info(s);
	
	return (s - buf);
}

static ssize_t pm_status_store(struct kobject *kobj, struct kobj_attribute *attr,
			const char *buf, size_t n)
{
	printk(KERN_ERR "%s: no support yet.\n", __func__);

	return -EPERM;
}

static unsigned pm_wakeup_fetched = true;
static ssize_t pm_wakeup_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	char *s = buf;

	if (!pm_wakeup_fetched) {
		pm_wakeup_fetched = true;
		s += sprintf(s, "true");
	} else
		s += sprintf(s, "false");
	
	return (s - buf);
}

static ssize_t pm_wakeup_store(struct kobject *kobj, struct kobj_attribute *attr,
			const char *buf, size_t n)
{
	printk(KERN_ERR "%s: no support yet.\n", __func__);

	return -EPERM;
}

#if 0
private_attr(tlmm_sim);
private_attr(gpio_num);
private_attr(msm_gpio_before_sleep_table);
private_attr(clk);
#endif
private_attr(msm_gpio_before_sleep);
private_attr(msm_gpio);
private_attr(vreg_before_sleep);
private_attr(vreg);
private_attr(pm_status);
private_attr(pm_wakeup);

static struct attribute *g_private_attr[] = {
#if 0
	&gpio_num_attr.attr,
	&msm_gpio_before_sleep_table_attr.attr,
	&tlmm_sim_attr.attr,
	&clk_attr.attr,
#endif
	&msm_gpio_before_sleep_attr.attr,
	&msm_gpio_attr.attr,
	&vreg_attr.attr,
	&vreg_before_sleep_attr.attr,
	&pm_status_attr.attr,
	&pm_wakeup_attr.attr,
	NULL,
};

static struct attribute_group private_attr_group = {
	.attrs = g_private_attr,
};

static struct kobject *sysfs_private_kobj;

#define SLEEP_LOG
#ifdef SLEEP_LOG
#define WRITE_SLEEP_LOG
#define MAX_WAKEUP_IRQ 8

enum {
	DEBUG_SLEEP_LOG = 1U << 0,
	DEBUG_WRITE_LOG = 1U << 1,
	DEBUG_WAKEUP_IRQ = 1U << 2,
	DEBUG_RPM_SPM_LOG = 1U << 3,
	DEBUG_RPM_CXO_LOG = 1U << 4,
	DEBUG_ADSP_CXO_LOG = 1U << 5,
	DEBUG_MODEM_CXO_LOG = 1U << 6,
	DEBUG_WCNSS_CXO_LOG = 1U << 7,
};
static int debug_mask;// = DEBUG_WRITE_LOG;
module_param_named(debug_mask, debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

struct sleep_log_t {
	char time[18];
	long timesec;
	unsigned int log;
	uint32_t maoints[2];
	int wakeup_irq[MAX_WAKEUP_IRQ];
	int wakeup_gpio;
//31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
//bit1-0=00 :try to sleep; bit 1-0 = 01 : leave from sleep    ;bit1-0=10:fail to sleep
//bit31-bit24 : return value
};

struct rpm_smem_state_t {
	uint32_t wakeup_ints[2];
};
struct rpm_smem_state_t rpm_smem_state_data;

#define TRY_TO_SLEEP  (0)
#define LEAVE_FORM_SLEEP  (1)
#define FAIL_TO_SLEEP  (2)

#define SLEEP_LOG_LENGTH 80

struct sleep_log_t sleep_log_array[SLEEP_LOG_LENGTH];
int sleep_log_pointer = 0;
int sleep_log_count = 0;
int enter_times = 0;

static int irq_wakeup_saved = MAX_WAKEUP_IRQ;
static int irq_wakeup_irq[MAX_WAKEUP_IRQ];
static int irq_wakeup_gpio;

char sleep_log_name[60];
struct file *sleep_log_file = NULL;

#ifdef WRITE_SLEEP_LOG
static int sleep_log_write(void)
{
	char buf[256];
	char *p, *p0;
	int i, j, pos;
	mm_segment_t old_fs;
	p = buf;
	p0 = p;

	if (sleep_log_file == NULL)
		sleep_log_file = filp_open(sleep_log_name, O_RDWR | O_APPEND | O_CREAT,
				0644);
	if (IS_ERR(sleep_log_file)) {
		printk("error occured while opening file %s, exiting...\n",
				sleep_log_name);
		return 0;
	}

	if (sleep_log_count > 1) {
		for (i = 0; i < 2; i++) {
			if (sleep_log_pointer == 0)
				pos = SLEEP_LOG_LENGTH - 2 + i;
			else
				pos = sleep_log_pointer - 2 + i;
			switch (sleep_log_array[pos].log & 0xF) {
			case TRY_TO_SLEEP:
				p += sprintf(p, ">[%ld]%s\n", sleep_log_array[pos].timesec,
						sleep_log_array[pos].time);
				break;
			case LEAVE_FORM_SLEEP:
				p += sprintf(p, "<[%ld]%s(0x%x,0x%x,",
						sleep_log_array[pos].timesec,
						sleep_log_array[pos].time,
						sleep_log_array[pos].maoints[0],
						sleep_log_array[pos].maoints[1]);
				for (j = 0; j < MAX_WAKEUP_IRQ && sleep_log_array[pos].wakeup_irq[j]; j++)
					p += sprintf(p, " %d", sleep_log_array[pos].wakeup_irq[j]);

				if (sleep_log_array[pos].wakeup_gpio)
					p += sprintf(p, ", gpio %d", sleep_log_array[pos].wakeup_gpio);

				p += sprintf(p, ")\n");
				break;
			case FAIL_TO_SLEEP:
				p += sprintf(p, "^[%ld]%s(%d)\n", sleep_log_array[pos].timesec,
						sleep_log_array[pos].time,
						(char) (sleep_log_array[pos].log >> 24));
				break;
			}
		}
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	sleep_log_file->f_op->write(sleep_log_file, p0, p - p0,
			&sleep_log_file->f_pos);
	set_fs(old_fs);

	if (sleep_log_file != NULL) {
		filp_close(sleep_log_file, NULL);
		sleep_log_file = NULL;
	}
	return 0;
}
#else //WRITE_SLEEP_LOG
static int sleep_log_write(void)
{
	return 0;
}
#endif //WRITE_SLEEP_LOG

static int save_irq_wakeup_internal(int irq)
{
	int i;
	int ret;

	ret = 0;
	if (irq_wakeup_saved < MAX_WAKEUP_IRQ) {
		for (i = 0; i < irq_wakeup_saved; i++) {
			if (irq == irq_wakeup_irq[i])
				break;
		}
		if (i == irq_wakeup_saved)
			ret = irq_wakeup_irq[irq_wakeup_saved++] = irq;
	}
	return ret;
}

int save_irq_wakeup_gpio(int irq, int gpio)
{
	struct irq_desc *desc;
	int ret;

	ret = 0;
	if (debug_mask & DEBUG_WAKEUP_IRQ) {
		desc = irq_to_desc(irq);
		if (desc != NULL) {
			if (irqd_is_wakeup_set(&desc->irq_data)) {
				ret = save_irq_wakeup_internal(irq);
				if (ret) {
					if (gpio != 0 && irq_wakeup_gpio == 0) {
						irq_wakeup_gpio = gpio;
						irq_wakeup_saved = MAX_WAKEUP_IRQ;
					}
#ifdef CONFIG_KALLSYMS
					printk("%s(), irq=%d, gpio=%d, %s, handler=(%pS)\n", __func__, irq, gpio, 
						desc->action && desc->action->name ? desc->action->name : "",
						desc->action ? (void *)desc->action->handler : 0);
#else
					printk("%s(), irq=%d, gpio=%d, %s, handler=0x%08x\n", __func__, irq, gpio, 
						desc->action && desc->action->name ? desc->action->name : "",
						desc->action ? (unsigned int)desc->action->handler : 0);
#endif
				}
			}
		}
	}

	return ret;
}

static void clear_irq_wakeup_saved(void)
{
	if (debug_mask & DEBUG_WAKEUP_IRQ) {
		memset(irq_wakeup_irq, 0, sizeof(irq_wakeup_irq));
		irq_wakeup_gpio = 0;
		irq_wakeup_saved = 0;
	}
}

static void set_irq_wakeup_saved(void)
{
	if (debug_mask & DEBUG_WAKEUP_IRQ)
		irq_wakeup_saved = MAX_WAKEUP_IRQ;
}

void log_suspend_enter(void)
{
	//extern void smem_set_reserved(int index, int data);
	struct timespec ts_;
	struct rtc_time tm_;

	//Turn on/off the share memory flag to inform RPM to record spm logs
	//smem_set_reserved(6, debug_mask & DEBUG_WAKEUP_IRQ ? 1 : 0);
	//smem_set_reserved(6, debug_mask);

	if (debug_mask & DEBUG_SLEEP_LOG) {
		printk("%s(), APPS try to ENTER sleep mode>>>\n", __func__);

		getnstimeofday(&ts_);
		rtc_time_to_tm(ts_.tv_sec + 8 * 3600, &tm_);

		sprintf(sleep_log_array[sleep_log_pointer % SLEEP_LOG_LENGTH].time,
				"%d-%02d-%02d %02d:%02d:%02d", tm_.tm_year + 1900, tm_.tm_mon + 1,
				tm_.tm_mday, tm_.tm_hour, tm_.tm_min, tm_.tm_sec);

		if (strlen(sleep_log_name) < 1) {
			sprintf(sleep_log_name,
					"/data/local/log/aplog/sleeplog%d%02d%02d_%02d%02d%02d.txt",
					tm_.tm_year + 1900, tm_.tm_mon + 1, tm_.tm_mday, tm_.tm_hour,
					tm_.tm_min, tm_.tm_sec);
			printk("%s(), sleep_log_name = %s \n", __func__, sleep_log_name);
		}

		sleep_log_array[sleep_log_pointer % SLEEP_LOG_LENGTH].timesec = ts_.tv_sec;
		sleep_log_array[sleep_log_pointer % SLEEP_LOG_LENGTH].log = TRY_TO_SLEEP;
		sleep_log_pointer++;
		sleep_log_count++;
		if (sleep_log_pointer == SLEEP_LOG_LENGTH)
			sleep_log_pointer = 0;
	}

	clear_irq_wakeup_saved();
	pm_wakeup_fetched = false;
}

void log_suspend_exit(int error)
{
#if 0
	extern int smem_get_reserved(int index);
#else
	//extern void msm_rpmstats_get_reverved(u32 reserved[][4]);
	u32 reserved[4][4];
#endif
	struct timespec ts_;
	struct rtc_time tm_;
	uint32_t smem_value;
	int i;

	if (debug_mask & DEBUG_SLEEP_LOG) {
		getnstimeofday(&ts_);
		rtc_time_to_tm(ts_.tv_sec + 8 * 3600, &tm_);
		sprintf(sleep_log_array[sleep_log_pointer % SLEEP_LOG_LENGTH].time,
				"%d-%02d-%02d %02d:%02d:%02d", tm_.tm_year + 1900, tm_.tm_mon + 1,
				tm_.tm_mday, tm_.tm_hour, tm_.tm_min, tm_.tm_sec);

		sleep_log_array[sleep_log_pointer % SLEEP_LOG_LENGTH].timesec = ts_.tv_sec;

		if (error == 0) {
#if 0
			rpm_smem_state_data.wakeup_ints[0] = smem_get_reserved(0);
			rpm_smem_state_data.wakeup_ints[1] = smem_get_reserved(1);
#elif 0
			if (debug_mask & DEBUG_RPM_SPM_LOG) {
				for(i = 0; i <= 5; i++) {
					smem_value = ((uint32_t)smem_get_reserved(i)) & 0xffff;
					if(smem_value > 0)
						printk("rpm: %s[%d] = %d\n", "spm_active" , i, smem_value);
				}
			}
			if (debug_mask & DEBUG_RPM_CXO_LOG) {
				for(i = 0; i <= 5; i++) {
					smem_value = ((uint32_t)smem_get_reserved(i)) >> 16;
					if(smem_value > 0)
						printk("rpm: %s[%d] = %d\n", "cxo_voter" , i, smem_value);
				}
			}
#else
			if (debug_mask & (DEBUG_RPM_SPM_LOG | DEBUG_RPM_CXO_LOG)) {
				memset(reserved, 0, sizeof(reserved));
				//msm_rpmstats_get_reverved(reserved);
#if 0
				for(i = 0; i < 3; i++)
					printk("reserved[0][%d]=0x%08x\n", i, reserved[0][i]);
				for(i = 0; i < 3; i++)
					printk("reserved[1][%d]=0x%08x\n", i, reserved[1][i]);
#endif
			}

			if (debug_mask & DEBUG_RPM_SPM_LOG) {
				for(i = 0; i <= 5; i++) {
					smem_value = (reserved[1][i/2] >> (16 * (i % 2))) & 0xffff;
					if(smem_value > 0)
						printk("rpm: %s[%d] = %d\n", "spm_active" , i, smem_value);
				}
			}
			if (debug_mask & DEBUG_RPM_CXO_LOG) {
				for(i = 0; i <= 5; i++) {
					smem_value = (reserved[0][i/2] >> (16 * (i % 2))) & 0xffff;
					if(smem_value > 0)
						printk("rpm: %s[%d] = %d\n", "cxo_voter" , i, smem_value);
				}
			}
#endif

			printk("%s(), APPS Exit from sleep<<<: wakeup ints=0x%x, 0x%x\n", __func__ ,
					rpm_smem_state_data.wakeup_ints[0],
					rpm_smem_state_data.wakeup_ints[1]);

			sleep_log_array[sleep_log_pointer % SLEEP_LOG_LENGTH].log =
					LEAVE_FORM_SLEEP;
			sleep_log_array[sleep_log_pointer % SLEEP_LOG_LENGTH].maoints[0] =
					rpm_smem_state_data.wakeup_ints[0];
			sleep_log_array[sleep_log_pointer % SLEEP_LOG_LENGTH].maoints[1] =
					rpm_smem_state_data.wakeup_ints[1];
			for (i = 0; i < (irq_wakeup_gpio == 0 ? irq_wakeup_saved : 1); i++)
				sleep_log_array[sleep_log_pointer % SLEEP_LOG_LENGTH].wakeup_irq[i] =
					irq_wakeup_irq[i];
			for (; i < MAX_WAKEUP_IRQ; i++)
				sleep_log_array[sleep_log_pointer % SLEEP_LOG_LENGTH].wakeup_irq[i] = 0;
			sleep_log_array[sleep_log_pointer % SLEEP_LOG_LENGTH].wakeup_gpio =
					irq_wakeup_gpio;
		} else {
			printk("%s(), APPS FAIL to enter sleep^^^\n", __func__);

			sleep_log_array[sleep_log_pointer % SLEEP_LOG_LENGTH].log =
					FAIL_TO_SLEEP | (error << 24);
		}

		sleep_log_pointer++;
		sleep_log_count++;

		if (sleep_log_pointer == SLEEP_LOG_LENGTH)
			sleep_log_pointer = 0;

		if (debug_mask & DEBUG_WRITE_LOG) {
			enter_times++;
			if (enter_times < 5000)
				sleep_log_write();
		}
	}

	set_irq_wakeup_saved();
}
#else //SLEEP_LOG
void log_suspend_enter(void)
{
	clear_irq_wakeup_saved();
	pm_wakeup_fetched = false;
}

void log_suspend_exit(int error)
{
	set_irq_wakeup_saved();
}
#endif //SLEEP_LOG

static int __init sysfs_private_init(void)
{
	int result;

	printk("%s(), %d\n", __func__, __LINE__);

	sysfs_private_kobj = kobject_create_and_add("private", NULL);
	if (!sysfs_private_kobj)
		return -ENOMEM;

	result = sysfs_create_group(sysfs_private_kobj, &private_attr_group);
	printk("%s(), %d, result=%d\n", __func__, __LINE__, result);

#ifdef SLEEP_LOG
	strcpy (sleep_log_name, "");
	sleep_log_pointer = 0;
	sleep_log_count = 0;
	enter_times = 0;
#endif

	return result;
}

static void __exit sysfs_private_exit(void)
{
	printk("%s(), %d\n", __func__, __LINE__);
	sysfs_remove_group(sysfs_private_kobj, &private_attr_group);

	kobject_put(sysfs_private_kobj);
}

module_init(sysfs_private_init);
module_exit(sysfs_private_exit);
