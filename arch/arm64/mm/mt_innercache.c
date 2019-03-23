/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <asm/topology.h>
#include <mt-plat/sync_write.h>
#include "mt_innercache.h"

/*
 * inner_dcache_flush_all: Flush (clean + invalidate) the entire L1 data cache.
 *
 * This can be used ONLY by the M4U driver!!
 * Other drivers should NOT use this function at all!!
 * Others should use DMA-mapping APIs!!
 *
 * After calling the function, the buffer should not be touched anymore.
 * And the M4U driver should then call outer_flush_all() immediately.
 * Here is the example:
 *     // Cannot touch the buffer from here.
 *     inner_dcache_flush_all();
 *     outer_flush_all();
 *     // Can touch the buffer from here.
 * If preemption occurs and the driver cannot guarantee that no other process will touch the buffer,
 * the driver should use LOCK to protect this code segment.
 */

void inner_dcache_flush_all(void)
{
	__inner_flush_dcache_all();
}
EXPORT_SYMBOL(inner_dcache_flush_all);

void inner_dcache_flush_L1(void)
{
	__inner_flush_dcache_L1();
}

void inner_dcache_flush_L2(void)
{
	__inner_flush_dcache_L2();
}

/*
 * smp_inner_dcache_flush_all: Flush (clean + invalidate) the entire L1 data cache.
 *
 * This can be used ONLY by the M4U driver!!
 * Other drivers should NOT use this function at all!!
 * Others should use DMA-mapping APIs!!
 *
 * This is the smp version of inner_dcache_flush_all().
 * It will use IPI to do flush on all CPUs.
 * Must not call this function with disabled interrupts or from a
 * hardware interrupt handler or from a bottom half handler.
 */
void smp_inner_dcache_flush_all(void)
{
	int i, total_core, cid, last_cid;
	struct cpumask mask;

	if (in_interrupt()) {
		pr_err("Cannot invoke smp_inner_dcache_flush_all() in interrupt/softirq context\n");
		return;
	}

	get_online_cpus();
	preempt_disable();

	/* Find first online cpu in each cluster */
	last_cid = -1;
	cpumask_clear(&mask);
	total_core = num_possible_cpus();
	for (i = 0; i < total_core; i++) {
		if (!cpu_online(i))
			continue;

		cid = arch_get_cluster_id(i);
		if (last_cid != cid) {
			cpumask_set_cpu(i, &mask);
			last_cid = cid;
		}
	}

	on_each_cpu((smp_call_func_t)inner_dcache_flush_L1, NULL, true);
	smp_call_function_many(&mask, (smp_call_func_t)inner_dcache_flush_L2,
				NULL, true);
	/*
	 * smp_call_function_many only run on "other Cpus".
	 * Flush L2 here if this is one of the first cores
	 */
	if (cpumask_test_cpu(smp_processor_id(), &mask))
		inner_dcache_flush_L2();

	preempt_enable();
	put_online_cpus();
}
EXPORT_SYMBOL(smp_inner_dcache_flush_all);

#if 0
static ssize_t cache_test_show(struct device_driver *driver, char *buf)
{
	__disable_icache();
	__enable_icache();
	__disable_dcache();
	__enable_dcache();
	__disable_cache();
	__enable_cache();
	__inner_inv_dcache_L1();
	__inner_inv_dcache_L2();
	__inner_inv_dcache_all();
	__inner_clean_dcache_L1();
	__inner_clean_dcache_L2();
	__inner_clean_dcache_all();
	__inner_flush_dcache_L1();
	__inner_flush_dcache_L2();
	__inner_flush_dcache_all();
	__disable_dcache__inner_flush_dcache_L1();
	__disable_dcache__inner_flush_dcache_L1__inner_clean_dcache_L2();
	__disable_dcache__inner_flush_dcache_L1__inner_flush_dcache_L2();

	return strlen(buf);
}

static ssize_t cache_test_store(struct device_driver *driver, const char *buf, size_t count)
{
	return count;
}
#endif
