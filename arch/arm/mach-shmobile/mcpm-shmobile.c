/*
 * SMP support for SoCs with APMU
 *
 * Copyright (C) 2015  Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/io.h>
#include <linux/of.h>
#include <linux/ioport.h>
#include <linux/arm-cci.h>
#include <asm/cp15.h>
#include <asm/mcpm.h>
#include <asm/smp_plat.h>
#include <asm/cacheflush.h>

#include "common.h"
#include "platsmp-apmu.h"

static bool shmobile_mcpm_probed_ok;
extern void shmobile_invalidate_mcpm_entry(void);

bool shmobile_mcpm_probed(void)
{
	return shmobile_mcpm_probed_ok;
}

unsigned int inline pcpu_to_cpu(unsigned int pcpu, unsigned int pcluster)
{
	unsigned int mpidr = 0;

	mpidr = pcpu | (pcluster << MPIDR_LEVEL_BITS);

	return get_logical_index(mpidr);
}

static void __naked apmu_power_up_setup(unsigned int affinity_level)
{
	asm volatile ("\n"
	"cmp	r0, #1\n"
	"bxne	lr\n"
	"b	cci_enable_port_for_self ");
}

int apmu_cpu_powerup(unsigned int pcpu, unsigned int pcluster)
{
	int cpu = pcpu_to_cpu(pcpu, pcluster);

	apmu_power_on(cpu);

	return 0;
}

int apmu_cluster_powerup(unsigned int cluster)
{
	return 0;
}
void apmu_cpu_powerdown_prepare(unsigned int pcpu, unsigned int pcluster)
{
	int cpu = pcpu_to_cpu(pcpu, pcluster);

//	if (cpu == 0)
	flush_cache_all();
		return;

	apmu_power_off(cpu);
}

void apmu_cluster_powerdown_prepare(unsigned int pcluster)
{
	return;
}

void apmu_cpu_cache_disable(void)
{
	v7_exit_coherency_flush(louis);
}

void apmu_cluster_cache_disable(void)
{
	if (read_cpuid_part() == ARM_CPU_PART_CORTEX_A15) {
		 /* disable L2 prefetching on the Cortex-A15 */
		asm volatile(
		"mcr    p15, 1, %0, c15, c0, 3 \n\t"
		"isb    \n\t"
		"dsb    "
		: : "r" (0x400) );
	}

	v7_exit_coherency_flush(all);
	cci_disable_port_by_cpu(read_cpuid_mpidr());
}

void apmu_cpu_is_up(unsigned int pcpu, unsigned int pcluster)
{
}

int apmu_wait_for_powerdown(unsigned int pcpu, unsigned int pcluster)
{
	int cpu = pcpu_to_cpu(pcpu, pcluster);

	apmu_power_off_poll(cpu);

	return 0;
}

static const struct mcpm_platform_ops apmu_power_ops = {
	.cpu_powerup            = apmu_cpu_powerup,
	.cluster_powerup        = apmu_cluster_powerup,
	.cpu_powerdown_prepare  = apmu_cpu_powerdown_prepare,
	.cluster_powerdown_prepare = apmu_cluster_powerdown_prepare,
	.cpu_cache_disable      = apmu_cpu_cache_disable,
	.cluster_cache_disable  = apmu_cluster_cache_disable,
	.wait_for_powerdown     = apmu_wait_for_powerdown,
	.cpu_is_up              = apmu_cpu_is_up,
};

static int __init shmobile_mcpm_init(void)
{
	struct device_node *node;
	int ret = 0;
	int i;

	shmobile_mcpm_probed_ok = false;

	/* Register boot function as boot cpu did not call power up
	 * but will call power down on idle.
	 */

	node = of_find_compatible_node(NULL, NULL, "arm,cci-400");
	if (!node && !of_device_is_available(node)) {
		pr_err("cci-400 node not found!");
		return -ENODEV;
	}

	if (!cci_probed())
		return -ENODEV;

	ret = mcpm_platform_register(&apmu_power_ops);
	if (!ret)
		ret = mcpm_sync_init(apmu_power_up_setup);
	if (!ret)
		ret = mcpm_loopback(apmu_cluster_cache_disable); /* CCI on */
	if (ret) {
		pr_err("mcpm could not be stared. %d\n", ret);
		return ret;
	}

	for (i = 0; i < NR_CPUS; i++)
		shmobile_smp_hook(i,
			virt_to_phys(shmobile_invalidate_mcpm_entry), 0);

	mcpm_smp_set_ops();

	shmobile_mcpm_probed_ok = true;

	return 0;
}
early_initcall(shmobile_mcpm_init);
