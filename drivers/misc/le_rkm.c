/*
filename: le_rkm.c

this is the lenovo replay kernel message implement.

By RKM,the offline log in andorid can backup previous system logs,
such as kernel dmesg log, tz logs; it also can access current
system lifetime's boot logs,such as sbl logs, lk logs, even tz logs.

Author:KerryXi
Date: Apr, 2014

Copyright Lenovo 2014
*/

//#define DEBUG 1
#include <linux/le_rkm.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/memblock.h>
#include <linux/of.h>
#include <linux/of_fdt.h>

static rkm_log_buf_header_table_t __log_buf_header_table __page_aligned_bss;

static char rkm_sbl1_buf[MAX_RKM_SBL1_BUF_LEN*PAGE_SIZE];
static unsigned long rkm_sbl1_buf_va;     //va addr
static unsigned long phys_sbl1_log_start __initdata = 0;
static unsigned long phys_sbl1_log_size = 0;

static char rkm_lk_buf[MAX_RKM_LK_BUF_LEN*PAGE_SIZE];
static unsigned long rkm_lk_buf_va;    //va addr
static unsigned long phys_lk_log_start __initdata = 0;
static unsigned long phys_lk_log_size = 0;

static char rkm_kernel_buf[MAX_RKM_LOG_BUF_LEN*PAGE_SIZE];
static unsigned long rkm_kernel_buf_va;     //va addr
static unsigned long phys_kernel_log_start __initdata = 0;
static unsigned long phys_kernel_log_size = 0;

extern char __bss_start[], __bss_stop[];

void rkm_init_log_buf_header(char *addr,int len)
{
	__log_buf_header_table.magic1 = RKM_LOG_BUF_HEADER_TABLE_MAGIC1;
	__log_buf_header_table.magic2 = RKM_LOG_BUF_HEADER_TABLE_MAGIC2;
	__log_buf_header_table.version = RKM_LOG_BUF_HEADER_TABLE_VERSION;
	__log_buf_header_table.bss_start = (unsigned int)__pa(__bss_start);
	__log_buf_header_table.bss_stop = (unsigned int)__pa(__bss_stop);
	__log_buf_header_table.log_buf_pa = (unsigned int)__pa(addr);
	__log_buf_header_table.log_buf_len = len;

	pr_err("rkm init log header:bss_start=0x%08x, bss_stop=0x%08x, log_pa=0x%08x, len=%d\n",
			__log_buf_header_table.bss_start,
			__log_buf_header_table.bss_stop,
			__log_buf_header_table.log_buf_pa,
			__log_buf_header_table.log_buf_len);
	return;
}

void __init arm_rkm_log_backup(void)
{
	unsigned long phys_log_end;

	if (phys_lk_log_size)
	{
		phys_log_end = phys_lk_log_start+phys_lk_log_size;

		if (phys_lk_log_size &&
				 !memblock_is_region_memory(phys_lk_log_start, phys_lk_log_size )) {
			pr_err("rkm: 0x%08lx+0x%08lx is not a memory region - disabling lk log\n",
					phys_lk_log_start, phys_lk_log_size);
			phys_lk_log_start=phys_lk_log_size=0;
		}
		if (phys_lk_log_size &&
				memblock_is_region_reserved(phys_lk_log_start, phys_lk_log_size)) {
			pr_err("rkm: 0x%08lx+0x%08lx overlaps in-use memory region - disabling lk log\n",
					phys_lk_log_start, phys_lk_log_size);
			phys_lk_log_start=phys_lk_log_size=0;
		}
		if (phys_lk_log_size) {
			rkm_lk_buf_va = ( unsigned long)__va(phys_lk_log_start);
			pr_debug("rkm:reserve lk log ok,va=0x%08lx\n",rkm_lk_buf_va);
		}
	}

	if (phys_sbl1_log_size)
	{
		if (phys_sbl1_log_size &&
				 !memblock_is_region_memory(phys_sbl1_log_start, phys_sbl1_log_size )) {
			pr_err("rkm: 0x%08lx+0x%08lx is not a memory region - disabling sbl1 log\n",
					phys_sbl1_log_start, phys_sbl1_log_size);
			phys_sbl1_log_start=phys_sbl1_log_size=0;
		}
		if (phys_sbl1_log_size &&
				memblock_is_region_reserved(phys_sbl1_log_start, phys_sbl1_log_size)) {
			pr_err("rkm: 0x%08lx+0x%08lx overlaps in-use memory region - disabling sbl1 log\n",
					phys_sbl1_log_start, phys_sbl1_log_size);
			phys_sbl1_log_start=phys_sbl1_log_size=0;
		}
		if (phys_sbl1_log_size) {
			rkm_sbl1_buf_va = ( unsigned long)__va(phys_sbl1_log_start);
			pr_err("rkm:reserve sbl1 log ok,va=0x%08lx\n",rkm_sbl1_buf_va);
		}
	}

	if (phys_kernel_log_size)
	{
		phys_log_end = phys_kernel_log_start+phys_kernel_log_size;

		if ((phys_log_end  < 0x82000000) || (phys_log_end  > 0x82200000)) {
			pr_err("rkm:unvalid kernel log pa,end phys: 0x%lx\n",phys_log_end);
			phys_kernel_log_start=phys_kernel_log_size=0;
		}

		if (phys_kernel_log_size &&
				 !memblock_is_region_memory(phys_kernel_log_start, phys_kernel_log_size )) {
			pr_err("rkm: 0x%08lx+0x%08lx is not a memory region - disabling kernel log\n",
					phys_kernel_log_start, phys_kernel_log_size);
			phys_kernel_log_start=phys_kernel_log_size=0;
		}
		if (phys_kernel_log_size &&
				memblock_is_region_reserved(phys_kernel_log_start, phys_kernel_log_size)) {
			pr_err("rkm: 0x%08lx+0x%08lx overlaps in-use memory region - disabling kernel log\n",
					phys_kernel_log_start, phys_kernel_log_size);
			phys_kernel_log_start=phys_kernel_log_size=0;
		}
		if (phys_kernel_log_size) {
			rkm_kernel_buf_va = ( unsigned long)__va(phys_kernel_log_start);
			pr_debug("rkm:reserve kernel log ok,va=0x%08lx\n",rkm_kernel_buf_va);
		}
	}

	//bakcup rkm message log
	if ((phys_lk_log_size>0) && (phys_lk_log_size <= sizeof(rkm_lk_buf))) {
		pr_info("rkm:backup lk log message: va=%p len=%ld\n",(void*)rkm_lk_buf_va,phys_lk_log_size);
		memcpy(rkm_lk_buf,(void*)rkm_lk_buf_va,phys_lk_log_size);
	}

	if (phys_sbl1_log_size>0) {
		pr_info("rkm:backup sbl1 log message: va=%p len=%ld\n",(void*)rkm_sbl1_buf_va,phys_sbl1_log_size);
		memcpy(rkm_sbl1_buf,(void*)rkm_sbl1_buf_va,phys_sbl1_log_size);
	}

	if ((phys_kernel_log_size>0) && (phys_kernel_log_size <= sizeof(rkm_kernel_buf))) {
		pr_info("rkm:backup kernel log message: va=%p len=%ld\n",(void*)rkm_kernel_buf_va,phys_kernel_log_size);
		memcpy(rkm_kernel_buf,(void*)rkm_kernel_buf_va,phys_kernel_log_size);
	}
	else 
		pr_info("rkm:fail backup kernel log message: len=%ld\n",phys_kernel_log_size);
	return;
}

static void __init early_init_dt_check_for_lk_log(unsigned long node)
{
	unsigned int start, size, len;
	const unsigned int *prop;

	pr_debug("rkm:Looking for lk log properties... ");

	prop = of_get_flat_dt_prop(node, "lk,lk_log_start", &len);
	if (!prop)
		return;
	start = of_read_ulong(prop, len/4);

	prop = of_get_flat_dt_prop(node, "lk,lk_log_size", &len);
	if (!prop)
		return;
	size = of_read_ulong(prop, len/4);

	pr_debug("rkm:lk_log_start=0x%x  lk_log_size=0x%x\n", start, size);

	if ((int)size <=4) {
		pr_err("uncorrect lk log size %d\n",(int)size);
		return;
	}
	phys_lk_log_start = start;
	phys_lk_log_size = size;

	prop = of_get_flat_dt_prop(node, "sbl1,log_start", &len);
	if (!prop)
		return;
	start = of_read_ulong(prop, len/4);

	prop = of_get_flat_dt_prop(node, "sbl1,log_size", &len);
	if (!prop)
		return;
	size = of_read_ulong(prop, len/4);

	if ((int)size <=4) {
		pr_err("uncorrect sbl1 log size %d\n",(int)size);
		return;
	}
	phys_sbl1_log_start = start;
	phys_sbl1_log_size = size;
}

static void __init early_init_dt_check_for_kernel_log(unsigned long node)
{
	unsigned int start, size, len;
	const unsigned int *prop;

	pr_debug("rkm:Looking for kernel log properties... ");

	prop = of_get_flat_dt_prop(node, "kernel,log_buf_start", &len);
	if (!prop)
		return;
	start = of_read_ulong(prop, len/4);

	prop = of_get_flat_dt_prop(node, "kernel,log_buf_size", &len);
	if (!prop)
		return;
	size = of_read_ulong(prop, len/4);

	pr_debug("rkm:log_buf_start=0x%x  log_buf_size=0x%x\n", start, size);

	if ((int)size <=4) {
		pr_err("uncorrect kernel log size %d\n",(int)size);
		return;
	}
	phys_kernel_log_start = start;
	phys_kernel_log_size = size;
}

int __init early_init_dt_scan_boot_log(unsigned long node, const char *uname,
				     int depth, void *data)
{
	pr_debug("search \"rkm_log\", depth: %d, uname: %s\n", depth, uname);

	if (depth != 1 ||
	    (strcmp(uname, "rkm_log") != 0 && strcmp(uname, "rkm_log@0") != 0))
		return 0;

	early_init_dt_check_for_lk_log(node);
	early_init_dt_check_for_kernel_log(node);

	return 1;
}

/* Define lk_log to add a sysfs interface to read/write */
static ssize_t rkm_debug_lk_log_read(struct file *file,char __user *buf, size_t n, loff_t *ppos)
{
	if (*ppos == 0)
		pr_debug("rkm:---lk_log read/wrte testing--- pos=%d\n",(int)*ppos);
	if ((*ppos == 0) || (*ppos == 4096)) {
		if (!copy_to_user(buf,rkm_lk_buf+*ppos,PAGE_SIZE)) {
			if (*ppos == 0)
				pr_debug("%s:check copy_to_user ok\n",__func__);
			*ppos += PAGE_SIZE;
			return PAGE_SIZE;
		}
	}
	return 0;
}

static const struct file_operations rkm_debug_lk_log_fops = {
	.owner = THIS_MODULE,
	.read = rkm_debug_lk_log_read,
};

static ssize_t rkm_debug_sbl1_log_read(struct file *file,char __user *buf, size_t n, loff_t *ppos)
{
	if (*ppos == 0)
		pr_debug("rkm:---sbl1_log read/wrte testing--- pos=%d\n",(int)*ppos);
	if ((*ppos == 0) || (*ppos == 4096)) {
		if (!copy_to_user(buf,rkm_sbl1_buf+*ppos,PAGE_SIZE)) {
			if (*ppos == 0)
				pr_debug("%s:check copy_to_user ok\n",__func__);
			*ppos += PAGE_SIZE;
			return PAGE_SIZE;
		}
	}
	return 0;
}

static const struct file_operations rkm_debug_sbl1_log_fops = {
	.owner = THIS_MODULE,
	.read = rkm_debug_sbl1_log_read,
};

/* Define kernel_log to add a sysfs interface to read/write */
static ssize_t rkm_debug_kernel_log_read(struct file *file,char __user *buf, size_t n, loff_t *ppos)
{
	if (*ppos == 0)
		pr_debug("rkm:---kernel_log read/wrte testing--- pos=%d\n",(int)*ppos);

	if ( (*ppos < phys_kernel_log_size)
		&& ( (*ppos & (PAGE_SIZE-1)) == 0) ) {
		if (!copy_to_user(buf,rkm_kernel_buf+*ppos,PAGE_SIZE)) {
			if (*ppos == 0)
				pr_debug("%s:check copy_to_user ok\n",__func__);
			*ppos += PAGE_SIZE;
			return PAGE_SIZE;
		}
	}
	return 0;
}

static const struct file_operations rkm_debug_kernel_log_fops = {
	.owner = THIS_MODULE,
	.read = rkm_debug_kernel_log_read,
};

static int __init le_rkm_init_debugfs(void)
{
	struct dentry *dir, *file;

	dir = debugfs_create_dir("le_rkm", NULL);
	if (!dir)
		return -ENOMEM;

	file = debugfs_create_file("sbl1_mesg", 0400, dir, NULL,
					&rkm_debug_sbl1_log_fops);
	if (!file) {
		pr_err("%s:init sbl1_mesg node fail\n",__func__);
		debugfs_remove(dir);
		return -ENOMEM;
	}

	file = debugfs_create_file("lk_mesg", 0400, dir, NULL,
					&rkm_debug_lk_log_fops);
	if (!file) {
		pr_err("%s:init lk_mesg node fail\n",__func__);
		debugfs_remove(dir);
		return -ENOMEM;
	}
	file = debugfs_create_file("last_kmsg", 0400, dir, NULL,
					&rkm_debug_kernel_log_fops);
	if (!file) {
		pr_err("%s:init last_kmsg node fail\n",__func__);
		debugfs_remove(dir);
		return -ENOMEM;
	}
	return 0;
}
/* Debugfs setup must be done later */
module_init(le_rkm_init_debugfs);
