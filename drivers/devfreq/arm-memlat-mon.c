/*
 * Copyright (c) 2014-2016, 2018 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "arm-memlat-mon: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/cpu_pm.h>
#include <linux/cpu.h>
#include "governor.h"
#include "governor_memlat.h"
#include <linux/perf_event.h>

enum ev_index {
	INST_IDX,
	CM_IDX,
	CYC_IDX,
	NUM_EVENTS
};
#define INST_EV		0x08
#define L2DM_EV		0x17
#define CYC_EV		0x11

struct event_data {
	struct perf_event *pevent;
	unsigned long prev_count;
};

struct cpu_pmu_stats {
	struct event_data events[NUM_EVENTS];
	ktime_t prev_ts;
};

struct cpu_grp_info {
	cpumask_t cpus;
	cpumask_t inited_cpus;
	unsigned long cache_miss_event;
	unsigned long inst_event;
	struct cpu_pmu_stats *cpustats;
	struct memlat_hwmon hw;
	struct notifier_block arm_memlat_cpu_notif;
	struct list_head mon_list;
};

#define to_cpustats(cpu_grp, cpu) \
	(&cpu_grp->cpustats[cpu - cpumask_first(&cpu_grp->cpus)])
#define to_devstats(cpu_grp, cpu) \
	(&cpu_grp->hw.core_stats[cpu - cpumask_first(&cpu_grp->cpus)])
#define to_cpu_grp(hwmon) container_of(hwmon, struct cpu_grp_info, hw)

static LIST_HEAD(memlat_mon_list);
static DEFINE_MUTEX(list_lock);

static unsigned long compute_freq(struct cpu_pmu_stats *cpustats,
						unsigned long cyc_cnt)
{
	ktime_t ts;
	unsigned int diff;
	unsigned long freq = 0;

	ts = ktime_get();
	diff = ktime_to_us(ktime_sub(ts, cpustats->prev_ts));
	if (!diff)
		diff = 1;
	cpustats->prev_ts = ts;
	freq = cyc_cnt;
	do_div(freq, diff);

	return freq;
}

#define MAX_COUNT_LIM 0xFFFFFFFFFFFFFFFF
static inline unsigned long read_event(struct event_data *event)
{
	unsigned long ev_count;
	u64 total, enabled, running;

	total = perf_event_read_value(event->pevent, &enabled, &running);
	if (total >= event->prev_count)
		ev_count = total - event->prev_count;
	else
		ev_count = (MAX_COUNT_LIM - event->prev_count) + total;

	event->prev_count = total;

	return ev_count;
}

static void read_perf_counters(int cpu, struct cpu_grp_info *cpu_grp)
{
	struct cpu_pmu_stats *cpustats = to_cpustats(cpu_grp, cpu);
	struct dev_stats *devstats = to_devstats(cpu_grp, cpu);
	unsigned long cyc_cnt;

	devstats->inst_count = read_event(&cpustats->events[INST_IDX]);
	devstats->mem_count = read_event(&cpustats->events[CM_IDX]);
	cyc_cnt = read_event(&cpustats->events[CYC_IDX]);
	devstats->freq = compute_freq(cpustats, cyc_cnt);
}

static unsigned long get_cnt(struct memlat_hwmon *hw)
{
	int cpu;
	struct cpu_grp_info *cpu_grp = to_cpu_grp(hw);

	for_each_cpu(cpu, &cpu_grp->inited_cpus)
		read_perf_counters(cpu, cpu_grp);

	return 0;
}

static void delete_events(struct cpu_pmu_stats *cpustats)
{
	int i;

	for (i = 0; i < NUM_EVENTS; i++) {
		cpustats->events[i].prev_count = 0;
		perf_event_release_kernel(cpustats->events[i].pevent);
	}
}

static void stop_hwmon(struct memlat_hwmon *hw)
{
	int cpu;
	struct cpu_grp_info *cpu_grp = to_cpu_grp(hw);
	struct dev_stats *devstats;

	get_online_cpus();
	for_each_cpu(cpu, &cpu_grp->inited_cpus) {
		delete_events(to_cpustats(cpu_grp, cpu));

		/* Clear governor data */
		devstats = to_devstats(cpu_grp, cpu);
		devstats->inst_count = 0;
		devstats->mem_count = 0;
		devstats->freq = 0;
	}
	mutex_lock(&list_lock);
	if (!cpumask_equal(&cpu_grp->cpus, &cpu_grp->inited_cpus))
		list_del(&cpu_grp->mon_list);
	mutex_unlock(&list_lock);
	cpumask_clear(&cpu_grp->inited_cpus);

	put_online_cpus();

	unregister_cpu_notifier(&cpu_grp->arm_memlat_cpu_notif);
}

static struct perf_event_attr *alloc_attr(void)
{
	struct perf_event_attr *attr;

	attr = kzalloc(sizeof(struct perf_event_attr), GFP_KERNEL);
	if (!attr)
		return attr;

	attr->type = PERF_TYPE_RAW;
	attr->size = sizeof(struct perf_event_attr);
	attr->pinned = 1;
	attr->exclude_idle = 1;

	return attr;
}

static int set_events(struct cpu_grp_info *cpu_grp, int cpu)
{
	struct perf_event *pevent;
	struct perf_event_attr *attr;
	int err;
	struct cpu_pmu_stats *cpustats = to_cpustats(cpu_grp, cpu);

	/* Allocate an attribute for event initialization */
	attr = alloc_attr();
	if (!attr)
		return -ENOMEM;

	attr->config = cpu_grp->inst_event;
	pevent = perf_event_create_kernel_counter(attr, cpu, NULL, NULL, NULL);
	if (IS_ERR(pevent))
		goto err_out;
	cpustats->events[INST_IDX].pevent = pevent;
	perf_event_enable(cpustats->events[INST_IDX].pevent);

	attr->config = cpu_grp->cache_miss_event;
	pevent = perf_event_create_kernel_counter(attr, cpu, NULL, NULL, NULL);
	if (IS_ERR(pevent))
		goto err_out;
	cpustats->events[CM_IDX].pevent = pevent;
	perf_event_enable(cpustats->events[CM_IDX].pevent);

	attr->config = CYC_EV;
	pevent = perf_event_create_kernel_counter(attr, cpu, NULL, NULL, NULL);
	if (IS_ERR(pevent))
		goto err_out;
	cpustats->events[CYC_IDX].pevent = pevent;
	perf_event_enable(cpustats->events[CYC_IDX].pevent);

	kfree(attr);
	return 0;

err_out:
	err = PTR_ERR(pevent);
	kfree(attr);
	return err;
}

static int arm_memlat_cpu_callback(struct notifier_block *nb,
		unsigned long action, void *hcpu)
{
	unsigned long cpu = (unsigned long)hcpu;
	struct cpu_grp_info *cpu_grp, *tmp;

	if (action != CPU_ONLINE)
		return NOTIFY_OK;

	mutex_lock(&list_lock);
	list_for_each_entry_safe(cpu_grp, tmp, &memlat_mon_list, mon_list) {
		if (!cpumask_test_cpu(cpu, &cpu_grp->cpus) ||
		    cpumask_test_cpu(cpu, &cpu_grp->inited_cpus))
			continue;
		if (set_events(cpu_grp, cpu))
			pr_warn("Failed to create perf ev for CPU%lu\n", cpu);
		else
			cpumask_set_cpu(cpu, &cpu_grp->inited_cpus);
		if (cpumask_equal(&cpu_grp->cpus, &cpu_grp->inited_cpus))
			list_del(&cpu_grp->mon_list);
	}
	mutex_unlock(&list_lock);

	return NOTIFY_OK;
}

static int start_hwmon(struct memlat_hwmon *hw)
{
	int cpu, ret = 0;
	struct cpu_grp_info *cpu_grp = to_cpu_grp(hw);

	register_cpu_notifier(&cpu_grp->arm_memlat_cpu_notif);

	get_online_cpus();
	for_each_cpu(cpu, &cpu_grp->cpus) {
		ret = set_events(cpu_grp, cpu);
		if (ret) {
			if (!cpu_online(cpu)) {
				ret = 0;
			} else {
				pr_warn("Perf event init failed on CPU%d\n",
					cpu);
				break;
			}
		} else {
			cpumask_set_cpu(cpu, &cpu_grp->inited_cpus);
		}
	}
	mutex_lock(&list_lock);
	if (!cpumask_equal(&cpu_grp->cpus, &cpu_grp->inited_cpus))
		list_add_tail(&cpu_grp->mon_list, &memlat_mon_list);
	mutex_unlock(&list_lock);

	put_online_cpus();

	return ret;
}

static int get_mask_from_dev_handle(struct platform_device *pdev,
					cpumask_t *mask)
{
	struct device *dev = &pdev->dev;
	struct device_node *dev_phandle;
	struct device *cpu_dev;
	int cpu, i = 0;
	int ret = -ENOENT;

	dev_phandle = of_parse_phandle(dev->of_node, "qcom,cpulist", i++);
	while (dev_phandle) {
		for_each_possible_cpu(cpu) {
			cpu_dev = get_cpu_device(cpu);
			if (cpu_dev && cpu_dev->of_node == dev_phandle) {
				cpumask_set_cpu(cpu, mask);
				ret = 0;
				break;
			}
		}
		dev_phandle = of_parse_phandle(dev->of_node,
						"qcom,cpulist", i++);
	}

	return ret;
}

static int arm_memlat_mon_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct memlat_hwmon *hw;
	struct cpu_grp_info *cpu_grp;
	int cpu, ret;
	u32 cachemiss_ev, inst_ev;

	cpu_grp = devm_kzalloc(dev, sizeof(*cpu_grp), GFP_KERNEL);
	if (!cpu_grp)
		return -ENOMEM;
	cpu_grp->arm_memlat_cpu_notif.notifier_call = arm_memlat_cpu_callback;
	hw = &cpu_grp->hw;

	hw->dev = dev;
	hw->of_node = of_parse_phandle(dev->of_node, "qcom,target-dev", 0);
	if (!hw->of_node) {
		dev_err(dev, "Couldn't find a target device\n");
		return -ENODEV;
	}

	if (get_mask_from_dev_handle(pdev, &cpu_grp->cpus)) {
		dev_err(dev, "CPU list is empty\n");
		return -ENODEV;
	}

	hw->num_cores = cpumask_weight(&cpu_grp->cpus);
	hw->core_stats = devm_kzalloc(dev, hw->num_cores *
				sizeof(*(hw->core_stats)), GFP_KERNEL);
	if (!hw->core_stats)
		return -ENOMEM;

	cpu_grp->cpustats = devm_kzalloc(dev, hw->num_cores *
			sizeof(*(cpu_grp->cpustats)), GFP_KERNEL);
	if (!cpu_grp->cpustats)
		return -ENOMEM;

	ret = of_property_read_u32(dev->of_node, "qcom,cachemiss-ev",
			&cachemiss_ev);
	if (ret) {
		dev_dbg(dev, "Cache Miss event not specified. Using def:0x%x\n",
				L2DM_EV);
		cachemiss_ev = L2DM_EV;
	}
	cpu_grp->cache_miss_event = cachemiss_ev;

	ret = of_property_read_u32(dev->of_node, "qcom,inst-ev", &inst_ev);
	if (ret) {
		dev_dbg(dev, "Inst event not specified. Using def:0x%x\n",
				INST_EV);
		inst_ev = INST_EV;
	}
	cpu_grp->inst_event = inst_ev;

	for_each_cpu(cpu, &cpu_grp->cpus)
		to_devstats(cpu_grp, cpu)->id = cpu;

	hw->start_hwmon = &start_hwmon;
	hw->stop_hwmon = &stop_hwmon;
	hw->get_cnt = &get_cnt;

	ret = register_memlat(dev, hw);
	if (ret) {
		pr_err("Mem Latency Gov registration failed\n");
		return ret;
	}

	return 0;
}

static struct of_device_id match_table[] = {
	{ .compatible = "qcom,arm-memlat-mon" },
	{}
};

static struct platform_driver arm_memlat_mon_driver = {
	.probe = arm_memlat_mon_driver_probe,
	.driver = {
		.name = "arm-memlat-mon",
		.of_match_table = match_table,
		.owner = THIS_MODULE,
		.suppress_bind_attrs = true,
	},
};

static int __init arm_memlat_mon_init(void)
{
	return platform_driver_register(&arm_memlat_mon_driver);
}
module_init(arm_memlat_mon_init);

static void __exit arm_memlat_mon_exit(void)
{
	platform_driver_unregister(&arm_memlat_mon_driver);
}
module_exit(arm_memlat_mon_exit);
