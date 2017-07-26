/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#define MODULE_NAME "msm_secboot"

static int secboot_show(struct seq_file *s, void *unused)
{
	void __iomem *base;
	unsigned int __iomem phy_base = 0x00070378;

	base = ioremap(phy_base, 0x10);
	seq_printf(s, "0x%08x\n", __raw_readl(base));

	return 0;
}

static int secboot_open(struct inode *inode, struct file *file)
{
	return single_open(file, secboot_show, NULL);
}

static const struct file_operations secboot_fops = {
	.open		= secboot_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init secboot_init(void)
{
	debugfs_create_file("secboot", S_IFREG | S_IRUGO,
				NULL, NULL, &secboot_fops);
	return 0;
}

module_init(secboot_init);
MODULE_DESCRIPTION("MSM Secboot Enable Check Driver");
MODULE_LICENSE("Dual BSD/GPL");
