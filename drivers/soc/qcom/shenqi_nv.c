/*
* Based on the work of Tony Sun
* Zhenglq : Add for Get nv data from modem using SMEM.
*/

#include <linux/types.h>
#include <linux/proc_fs.h>
#include "smd_private.h"
#include <linux/module.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define NV_WIFI_ADDR_SIZE	6
#define NV_MAX_SIZE		512

struct smem_nv {
	unsigned char nv_wifi[NV_WIFI_ADDR_SIZE];
};

static struct smem_nv *psmem_nv = NULL;

static int smem_read_nv(void)
{
	struct smem_nv *buf;

	buf = smem_alloc(SMEM_ID_VENDOR_READ_NV, NV_MAX_SIZE,SMEM_APPS, 2);
	if (!buf) {
		printk(KERN_ERR "SMEM_ID_VENDOR_READ_NV smem_alloc failed\n");
		return 1;
	}

	psmem_nv = kzalloc(sizeof(struct smem_nv), GFP_KERNEL);
	if (!psmem_nv) {
		printk(KERN_ERR "++++++++++++++++++++++=malloc psmem_nv fail\n");
		return 1;
	}

	memcpy(psmem_nv, buf, sizeof(struct smem_nv));

	return 0;
}

static long dump_wifi_addr(struct file *filp, char __user *buf,
						   size_t count, loff_t *f_pos)
{
	loff_t pos = *f_pos;
	int ret;

	ret = smem_read_nv();
	if (ret) {
		pr_info("%s: smem_read_nv() failed", __func__);
		return ret;
	}

	if (!psmem_nv)
	{
		printk(KERN_ERR "Could not get smem for wlan mac nv\n");
		return 0;
	}

	if (pos >= NV_WIFI_ADDR_SIZE) {
		count = 0;
		goto out;
	}

	if (count > (NV_WIFI_ADDR_SIZE - pos))
		count = NV_WIFI_ADDR_SIZE - pos;

	pos += count;

	if (copy_to_user(buf, psmem_nv->nv_wifi + *f_pos,count)) {
		count = -EFAULT;
		goto out;
	}

	*f_pos = pos;

out:
	return count;
}

int wlan_get_nv_mac(char* buf)
{
	int ret;

	ret = smem_read_nv();
	if (ret) {
		pr_info("%s: smem_read_nv() failed", __func__);
		return ret;
	}

	if (!psmem_nv){
		printk(KERN_ERR "Could not get smem for wlan mac nv\n");
		return -1;
	}

	memcpy(buf, psmem_nv->nv_wifi, NV_WIFI_ADDR_SIZE);
	return 0;
}
EXPORT_SYMBOL_GPL(wlan_get_nv_mac);

#define DECLARE_FOPS(name) static const struct file_operations name## _fops = { \
		.read = name, \
};

DECLARE_FOPS(dump_wifi_addr)

static int show_nv(void)
{
	struct proc_dir_entry *wifi_addr_entry;

	wifi_addr_entry = proc_create("mac_wifi", 0, NULL, &dump_wifi_addr_fops);
	if (!wifi_addr_entry) {
		pr_info("%s: failed to create mac_wifi entry", __func__);
		remove_proc_entry("mac_wifi", NULL);
		return 1;
	}

	return 0;
}

static int __init shenqi_nv_init(void)
{
	int ret = 0;
	printk("%s(),%d\n" ,__func__, __LINE__);

	ret = show_nv();
	if (ret) {
		pr_info("%s: show_nv() failed", __func__);
		return 0;
	}

	return 1;
}

static void __exit shenqi_nv_exit(void)
{
	printk("%s(),%d\n", __func__, __LINE__);
	kfree(psmem_nv);
	psmem_nv = NULL;
}

late_initcall(shenqi_nv_init);
module_exit(shenqi_nv_exit);
