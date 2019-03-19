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
#define NV_BT_ADDR_SIZE		6
#define NV_MAX_SIZE		512
/* [BEGIN] guohh1 20131011 add for FACTORYDATACHECK */
//[quanfj1] added for run qlogd when the first booting
#define NV_OTHERS_SIZE   (NV_MAX_SIZE - NV_WIFI_ADDR_SIZE - NV_BT_ADDR_SIZE-32-32-16-16-16 -16 - 32 -100 -32)
/* [END   ] guohh1 20131011 add for FACTORYDATACHECK*/

struct smem_nv {
	unsigned char nv_wifi[NV_WIFI_ADDR_SIZE];
	unsigned char nv_bt[NV_BT_ADDR_SIZE];
	/* [BEGIN] guohh1 20131011 add for FACTORYDATACHECK */
	unsigned char nv_sn1[32];
	unsigned char nv_sn2[32];
	unsigned char nv_meid[16];
	unsigned char nv_imei1[16];
	unsigned char nv_imei2[16];
	unsigned char nv_hwid[16];
	unsigned char nv_station[32];
	/* [END   ] guohh1 20131011 add for FACTORYDATACHECK*/

	//[quanfj1] read NV_FANCY_RUN_QLOGD_ALWAYS and set proc
	unsigned char nv_qlogd[32];

	/* [BEGIN][PLAT-66][MODEM][guohh11][20150610] read NV2498 and set proc */
	unsigned char nv_2498[100];
	/* [END][PLAT-66][MODEM][guohh11][20150610] read NV2498 and set proc */
	unsigned char nv_others[NV_OTHERS_SIZE];
};
static struct smem_nv * psmem_nv = NULL;

void dump_smem(struct smem_nv *buf)
{
	print_hex_dump(KERN_ALERT, "wifi   : ", DUMP_PREFIX_NONE, 16, 1, buf->nv_wifi, NV_WIFI_ADDR_SIZE, false);
	print_hex_dump(KERN_ALERT, "bt     : ", DUMP_PREFIX_NONE, 16, 1, buf->nv_bt, NV_BT_ADDR_SIZE, false);
	print_hex_dump(KERN_ALERT, "sn1    : ", DUMP_PREFIX_NONE, 16, 1, buf->nv_sn1, 32, true);
	print_hex_dump(KERN_ALERT, "sn2    : ", DUMP_PREFIX_NONE, 16, 1, buf->nv_sn2, 32, true);
	print_hex_dump(KERN_ALERT, "meid   : ", DUMP_PREFIX_NONE, 16, 1, buf->nv_meid, 16, true);
	print_hex_dump(KERN_ALERT, "imei1  : ", DUMP_PREFIX_NONE, 16, 1, buf->nv_imei1, 16, true);
	print_hex_dump(KERN_ALERT, "imei2  : ", DUMP_PREFIX_NONE, 16, 1, buf->nv_imei2, 16, true);
	print_hex_dump(KERN_ALERT, "hwid   : ", DUMP_PREFIX_NONE, 16, 1, buf->nv_hwid, 16, true);
	print_hex_dump(KERN_ALERT, "station: ", DUMP_PREFIX_NONE, 16, 1, buf->nv_station, 32, true);
	print_hex_dump(KERN_ALERT, "2498   : ", DUMP_PREFIX_NONE, 16, 1, buf->nv_2498, 100, true);
	print_hex_dump(KERN_ALERT, "qlogd  : ", DUMP_PREFIX_NONE, 16, 1, buf->nv_qlogd, 32, true);
	print_hex_dump(KERN_ALERT, "others : ", DUMP_PREFIX_NONE, 16, 1, buf->nv_others, NV_OTHERS_SIZE, false);
}

static void smem_read_nv(void)
{
	struct smem_nv * buf;

	buf = smem_alloc(SMEM_ID_VENDOR_READ_NV, NV_MAX_SIZE,SMEM_APPS,2); //shixy1,flag 1 need to be confirm here
	if(!buf) 
		printk(KERN_ERR "SMEM_ID_VENDOR_READ_NV smem_alloc failed\n");

	psmem_nv = kzalloc(sizeof(struct smem_nv),GFP_KERNEL); 
	if(!psmem_nv)
		printk(KERN_ERR "++++++++++++++++++++++=malloc psmem_nv fail \n");

	memcpy( psmem_nv, buf, sizeof(struct smem_nv));
	dump_smem(psmem_nv);

	return;	
}

static long dump_wifi_addr(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) 
{
	loff_t pos = *f_pos;

	if (!psmem_nv)
		smem_read_nv();

	if (!psmem_nv) 
	{
		printk(KERN_ERR "Could not get smem for wlan mac nv\n");
		return 0;
	}

	if(pos >= NV_WIFI_ADDR_SIZE) {
		count = 0;
		goto out;
	}

	if(count > (NV_WIFI_ADDR_SIZE - pos))
		count = NV_WIFI_ADDR_SIZE - pos;

	pos += count;

	if(copy_to_user(buf,psmem_nv->nv_wifi+*f_pos,count)){
		count = -EFAULT;
		goto out;
	}

	*f_pos = pos;

out:
	return count;
}

int wlan_get_nv_mac(char* buf)
{
	int ret = -1;

	if (!psmem_nv) 
		smem_read_nv();

	if (!psmem_nv){
		printk(KERN_ERR "Could not get smem for wlan mac nv\n");
		return ret;
	}

	printk(KERN_ERR "wifi addr  = 0x %02x %02x %02x %02x %02x %02x\n",
					psmem_nv->nv_wifi[0],psmem_nv->nv_wifi[1],psmem_nv->nv_wifi[2],
					psmem_nv->nv_wifi[3],psmem_nv->nv_wifi[4],psmem_nv->nv_wifi[5]);
	memcpy( buf, psmem_nv->nv_wifi, NV_WIFI_ADDR_SIZE);
	return 0;
}
EXPORT_SYMBOL_GPL(wlan_get_nv_mac);

static long dump_bt_addr(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) 
{
	loff_t pos = *f_pos;
	size_t len;

	if (!psmem_nv)
		smem_read_nv();

	if (!psmem_nv) {
		printk(KERN_ERR "Could not get smem for bt mac nv\n");
		return 0;
	}

	len = sizeof(psmem_nv->nv_bt);

	if(pos >= len ) {
			count = 0;
			goto out;
	}

	if(count > (len - pos))
			count = len - pos;

	pos += count;

	if(copy_to_user(buf,psmem_nv->nv_bt + (*f_pos),count)){
		count = -EFAULT;
		goto out;
	}

	*f_pos = pos;

out:
	return count;
}

/* [BEGIN] guohh1 20131011 add for FACTORYDATACHECK */
static long dump_lnv_sn1(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) 
{
	loff_t pos = *f_pos;
	size_t len;

	if (!psmem_nv)
		smem_read_nv();

	if (!psmem_nv) 
		return 0;

	len = sizeof(psmem_nv->nv_sn1);

	if(pos >= len) {
		count = 0;
		goto out;
	}

	if(count > (len - pos))
		count = len - pos;

	pos += count;

	if(copy_to_user(buf,psmem_nv->nv_sn1+*f_pos,count)){
		count = -EFAULT;
		goto out;
	}

	*f_pos = pos;

out:
	return count;
}

static long dump_lnv_sn2(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) 
{
	loff_t pos = *f_pos;
	size_t len;

	if (!psmem_nv)
		smem_read_nv();

	if (!psmem_nv) 
		return 0;

	len = sizeof(psmem_nv->nv_sn2);

	if(pos >= len)
	{
		count = 0;
		goto out;
	}

	if(count > (len - pos))
		count = len - pos;

	pos += count;

	if(copy_to_user(buf,psmem_nv->nv_sn2+*f_pos,count)){
		count = -EFAULT;
		goto out;
	}

	*f_pos = pos;

out:
	return count;
}

static long dump_lnv_meid(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) 
{
	loff_t pos = *f_pos;
	size_t len;

	if (!psmem_nv)
		smem_read_nv();

	if (!psmem_nv) 
		return 0;

	len = sizeof(psmem_nv->nv_meid);

	if(pos >= len)
	{
			count = 0;
			goto out;
	}

	if(count > (len - pos))
			count = len - pos;

	pos += count;

	if(copy_to_user(buf,psmem_nv->nv_meid+*f_pos,count)) {
			count = -EFAULT;
			goto out;
	}

	*f_pos = pos;

out:
	return count;
}

static long dump_lnv_imei1(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) 
{
	loff_t pos = *f_pos;
	size_t len;

	if (!psmem_nv)
		smem_read_nv();

	if (!psmem_nv) 
		return 0;

	len = sizeof(psmem_nv->nv_imei1);

	if(pos >= len)
	{
			count = 0;
			goto out;
	}

	if(count > (len - pos))
			count = len - pos;

	pos += count;

	if(copy_to_user(buf,psmem_nv->nv_imei1+*f_pos,count)){
			count = -EFAULT;
			goto out;
	}

	*f_pos = pos;

out:
	return count;
}

static long dump_lnv_imei2(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) 
{
	loff_t pos = *f_pos;
	size_t len;

	if (!psmem_nv)
		smem_read_nv();

	if (!psmem_nv) 
		return 0;

	len = sizeof(psmem_nv->nv_imei2);

	if(pos >= len)
	{
			count = 0;
			goto out;
	}

	if(count > (len - pos))
			count = len - pos;

	pos += count;

	if(copy_to_user(buf,psmem_nv->nv_imei2+*f_pos,count)){
			count = -EFAULT;
			goto out;
	}

	*f_pos = pos;

out:
	return count;
}

static long dump_lnv_hwid(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) 
{
	loff_t pos = *f_pos;
	size_t len;

	if (!psmem_nv)
		smem_read_nv();

	if (!psmem_nv) 
		return 0;

	len = sizeof(psmem_nv->nv_hwid);

	if(pos >= len) {
		count = 0;
		goto out;
	}

	if(count > (len - pos))
			count = len - pos;

	pos += count;


	if(copy_to_user(buf,psmem_nv->nv_hwid+*f_pos,count)) {
			count = -EFAULT;
			goto out;
	}

	*f_pos = pos;

out:
	return count;
}

static long dump_lnv_station(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) 
{
	loff_t pos = *f_pos;
	size_t len;

	if (!psmem_nv)
		smem_read_nv();

	if (!psmem_nv) 
		return 0;

	len = sizeof(psmem_nv->nv_station);

	if(pos >= len)
	{
			count = 0;
			goto out;
	}

	if(count > (len - pos))
			count = len - pos;

	pos += count;

	if(copy_to_user(buf,psmem_nv->nv_station+*f_pos,count)){
		count = -EFAULT;
		goto out;
	}

	*f_pos = pos;

out:
	return count;
}
/* [END   ] guohh1 20131011 add for FACTORYDATACHECK*/
/* [BEGIN][PLAT-66][MODEM][guohh11][20150610] read NV2498 and set proc */
static long dump_lnv_nv2498(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) 
{
	loff_t pos = *f_pos;
	size_t len;

	if (!psmem_nv)
		smem_read_nv();

	if (!psmem_nv) 
		return 0;

	len = sizeof(psmem_nv->nv_2498);

	if(pos >= len) {
		count = 0;
		goto out;
	}

	if(count > (len - pos))
			count = len - pos;

	pos += count;

	if(copy_to_user(buf,psmem_nv->nv_2498 + (*f_pos),count)) {
		count = -EFAULT;
		goto out;
	}

	*f_pos = pos;

out:
	return count;
}
/* [END][PLAT-66][MODEM][guohh11][20150610] read NV2498 and set proc */


//[quanfj1] read NV_FANCY_RUN_QLOGD_ALWAYS and set proc
static long dump_lnv_qlogd(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	loff_t pos = *f_pos;
	size_t len;

	if (!psmem_nv)
		smem_read_nv();

	if (!psmem_nv)
		return 0;

	len = sizeof(psmem_nv->nv_qlogd);

	if(pos >= len)
	{
		count = 0;
		goto out;
	}

	if(count > (len - pos))
		count = len - pos;

	pos += count;

	if(copy_to_user(buf,psmem_nv->nv_qlogd + (*f_pos),count)) {
		count = -EFAULT;
		goto out;
	}

	*f_pos = pos;

out:
	return count;
}

static long dump_lnv_debug(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) 
{
	loff_t pos = *f_pos;
	size_t len;

	if (!psmem_nv)
		smem_read_nv();

	if (!psmem_nv) 
		return 0;

	len = sizeof(struct smem_nv);

	if(pos >= len) {
		count = 0;
		goto out;
	}

	if(count > (len - pos))
			count = len - pos;

	pos += count;

	*f_pos = pos;

	dump_smem(psmem_nv);

out:
	return count;
}

#define DECLARE_FOPS(name)	static const struct file_operations name## _fops = { \
		.read = name, \
};

DECLARE_FOPS(dump_wifi_addr)
DECLARE_FOPS(dump_bt_addr)
DECLARE_FOPS(dump_lnv_sn1)
DECLARE_FOPS(dump_lnv_sn2)
DECLARE_FOPS(dump_lnv_meid)
DECLARE_FOPS(dump_lnv_imei1)
DECLARE_FOPS(dump_lnv_imei2)
DECLARE_FOPS(dump_lnv_hwid)
DECLARE_FOPS(dump_lnv_station)
DECLARE_FOPS(dump_lnv_nv2498)
DECLARE_FOPS(dump_lnv_qlogd)
DECLARE_FOPS(dump_lnv_debug)

static void show_nv(void)
{
	struct proc_dir_entry *wifi_addr_entry;
	struct proc_dir_entry *bt_addr_entry;
	/* [BEGIN] guohh1 20131011 add for FACTORYDATACHECK */
	struct proc_dir_entry *sn1_addr_entry;
	struct proc_dir_entry *sn2_addr_entry;
	struct proc_dir_entry *meid_addr_entry;
	struct proc_dir_entry *imei1_addr_entry;
	struct proc_dir_entry *imei2_addr_entry;
	struct proc_dir_entry *hwid_addr_entry;
	struct proc_dir_entry *station_addr_entry;
	/* [END   ] guohh1 20131011 add for FACTORYDATACHECK*/

	struct proc_dir_entry *qlogd_addr_entry; //[quanfj1] read NV_FANCY_RUN_QLOGD_ALWAYS and set proc

	/* [BEGIN][PLAT-66][MODEM][guohh11][20150610] read NV2498 and set proc */
	struct proc_dir_entry *nv2498_addr_entry;
	/* [END][PLAT-66][MODEM][guohh11][20150610] read NV2498 and set proc */
	struct proc_dir_entry *debug_entry;

	wifi_addr_entry = proc_create("mac_wifi", 0, NULL, &dump_wifi_addr_fops);
	bt_addr_entry = proc_create("mac_bt", 0, NULL, &dump_bt_addr_fops);
	/* [BEGIN] guohh1 20131011 add for FACTORYDATACHECK */
	sn1_addr_entry = proc_create("lnvsn1", 0, NULL, &dump_lnv_sn1_fops);
	sn2_addr_entry = proc_create("lnvshowcode", 0, NULL, &dump_lnv_sn2_fops);
	meid_addr_entry = proc_create("lnvmeid", 0, NULL, &dump_lnv_meid_fops);
	imei1_addr_entry = proc_create("lnvimei1", 0, NULL, &dump_lnv_imei1_fops);
	imei2_addr_entry = proc_create("lnvimei2", 0, NULL, &dump_lnv_imei2_fops);
	hwid_addr_entry = proc_create("lnvhwid", 0, NULL, &dump_lnv_hwid_fops);
	station_addr_entry = proc_create("lnvstation", 0, NULL, &dump_lnv_station_fops);
	/* [END   ] guohh1 20131011 add for FACTORYDATACHECK*/

	//[quanfj1] read NV_FANCY_RUN_QLOGD_ALWAYS and set proc 
	qlogd_addr_entry = proc_create("lnvqlogd", 0, NULL, &dump_lnv_qlogd_fops);

	/* [BEGIN][PLAT-66][MODEM][guohh11][20150610] read NV2498 and set proc */
	nv2498_addr_entry = proc_create("lnv2498", 0, NULL, &dump_lnv_nv2498_fops);
	/* [END][PLAT-66][MODEM][guohh11][20150610] read NV2498 and set proc */
	debug_entry = proc_create("ln_debug", 0, NULL, &dump_lnv_debug_fops);

}

static int __init shenqi_nv_init(void)
{
	printk("%s(),%d\n",__func__,__LINE__);
	show_nv();	

	return 1;
}

static void __exit shenqi_nv_exit(void)
{
	printk("%s(),%d\n",__func__,__LINE__);
	kfree(psmem_nv);
	psmem_nv = NULL;
}

module_init(shenqi_nv_init);
module_exit(shenqi_nv_exit);
