/*
filename: le_rkm.h

this is the lenovo replay kernel message implement.

By RKM,the offline log in andorid can backup previous system logs,
such as kernel dmesg log, tz logs; it also can access current
system lifetime's boot logs,such as sbl logs, lk logs, even tz logs.

Author:KerryXi
Date: Apr, 2014

Copyright Lenovo 2014
*/

#ifndef _LE_RKM_H_
#define _LE_RKM_H_

/*rkm backup message layout, page unit
offset page 0: the rkm overall message area, include version info,kernel
		banner,etc. reserve 1 pages
offset page 1: current lk buf log information,reserve two pages,8k
offset page 3: last system kernel dmesg log.due to the dmesglog length maybe
		not fix, so it should stay tail. reserve 512k
*/
#define RKM_OVERVIEW_LOG_OFFSET        0
#define RKM_LK_BUF_LOG_OFFSET          1
#define RKM_LAST_KERNEL_LOG_BUF_OFFSET 3

//previous system kernel log_buf message
#define RKM_LOG_BUF_HEADER_TABLE_MAGIC1 0x686d6b72
#define RKM_LOG_BUF_HEADER_TABLE_MAGIC2 0x7461626c
#define RKM_LOG_BUF_HEADER_TABLE_VERSION 1

//uint is page size
#define MAX_RKM_LOG_BUF_LEN 512
#define MAX_RKM_LK_BUF_LEN 2
#define MAX_RKM_SBL1_BUF_LEN 4

#define RKM_SCAN_KERNEL_LOG_BUF_START_OFFSET 0xD00000
#define RKM_SCAN_KERNEL_LOG_BUF_END 0x1e00000

typedef struct {
	unsigned int magic1;    //0x686d6b72  "rkmh"
	unsigned int magic2;    //0x7461626c  "lbat"
	unsigned int version;
	unsigned int bss_start;
	unsigned int bss_stop;
	unsigned int log_buf_pa;
	unsigned int log_buf_len;
} rkm_log_buf_header_table_t;

void rkm_init_log_buf_header(char *addr,int len);

void arm_rkm_log_backup(void);

int early_init_dt_scan_boot_log(unsigned long node, const char *uname,
				     int depth, void *data);
#endif  //_LE_RKM_H_
