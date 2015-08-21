/*
 * SMP support for SoCs with APMU
 *
 * Copyright (C) 2014  Renesas Electronics Corporation
 * Copyright (C) 2013  Magnus Damm
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/cpu_pm.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/of_address.h>
#include <linux/smp.h>
#include <linux/suspend.h>
#include <linux/threads.h>
#include <asm/cacheflush.h>
#include <asm/cp15.h>
#include <asm/proc-fns.h>
#include <asm/smp_plat.h>
#include <asm/suspend.h>
#include "common.h"
#include "platsmp-apmu.h"

static struct {
	void __iomem *iomem;
	int bit;
	unsigned int pcpu;
	unsigned int pcluster;
} apmu_cpus[NR_CPUS];

#define WUPCR_OFFS 0x10
#define PSTR_OFFS 0x40
#define CPUNCR_OFFS(n) (0x100 + (0x10 * (n)))

int __maybe_unused apmu_power_on(unsigned int cpu)
{
	void __iomem *p = apmu_cpus[cpu].iomem;
	unsigned int pcluster = apmu_cpus[cpu].pcluster;
	unsigned int boot_pcluster = apmu_cpus[0].pcluster;
	int bit =  apmu_cpus[cpu].bit;

	if (!p)
		return -EINVAL;

	if (!shmobile_mcpm_probed() && (pcluster != boot_pcluster)) {
		pr_err("Requested to boot cpu %d on non-boot cluster!\n", cpu);
		return -EINVAL;
	}

	if ((boot_pcluster == 1) && (pcluster != boot_pcluster)) {
		pr_err("Cannot boot a15 when boot cluster is A7\n");
		return -EINVAL;
	}

	if (((readl_relaxed(p + PSTR_OFFS) >> (bit * 4)) & 0x03) == 0)
		return 0;

	/* request power on */
	writel_relaxed(BIT(bit), p + WUPCR_OFFS);

	/* wait for APMU to finish */
	while (readl_relaxed(p + WUPCR_OFFS) != 0)
		;

	return 0;
}

int apmu_power_off(unsigned int cpu)
{
	void __iomem *p = apmu_cpus[cpu].iomem;
	int bit =  apmu_cpus[cpu].bit;

	if (!p)
		return -EINVAL;

	/* request Core Standby for next WFI */
	writel_relaxed(3, p + CPUNCR_OFFS(bit));
	return 0;
}

int __maybe_unused apmu_power_off_poll(unsigned int cpu)
{
	void __iomem *p = apmu_cpus[cpu].iomem;
	int bit =  apmu_cpus[cpu].bit;
	int k;

	if (!p)
		return -EINVAL;

	for (k = 0; k < 1000; k++) {
		if (((readl_relaxed(p + PSTR_OFFS) >> (bit * 4)) & 0x03) == 3)
			return 1;

		mdelay(1);
	}

	return 0;
}

static void apmu_init_cpu(struct resource *res, int cpu, int bit)
{
	if ((cpu >= ARRAY_SIZE(apmu_cpus)) || apmu_cpus[cpu].iomem)
		return;

	apmu_cpus[cpu].iomem = ioremap_nocache(res->start, resource_size(res));
	apmu_cpus[cpu].bit = bit;

	pr_debug("apmu ioremap %d %d %pr\n", cpu, bit, res);
}

static void apmu_parse_cfg(void (*fn)(struct resource *res, int cpu, int bit),
			   struct rcar_apmu_config *apmu_config, int num)
{
	u32 id;
	int k;
	int bit, index;

	for (k = 0; k < num; k++) {
		for (bit = 0; bit < ARRAY_SIZE(apmu_config[k].cpus); bit++) {
			id = apmu_config[k].cpus[bit];
			if (id >= 0) {
				index = get_logical_index(id);
				if (index >= 0) {
					fn(&apmu_config[k].iomem, index, bit);
					apmu_cpus[index].pcpu =
						MPIDR_AFFINITY_LEVEL(id, 0);
					apmu_cpus[index].pcluster =
						MPIDR_AFFINITY_LEVEL(id, 1);
				}
			}
		}
	}
}

void __init shmobile_smp_apmu_prepare_cpus(unsigned int max_cpus,
					   struct rcar_apmu_config *apmu_config,
					   int num)
{
	/* install boot code shared by all CPUs */
	shmobile_boot_fn = virt_to_phys(shmobile_smp_boot);
	shmobile_boot_arg = MPIDR_HWID_BITMASK;

	/* perform per-cpu setup */
	apmu_parse_cfg(apmu_init_cpu, apmu_config, num);
}

#ifdef CONFIG_SMP
int shmobile_smp_apmu_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	/* For this particular CPU register boot vector */
	shmobile_smp_hook(cpu, virt_to_phys(secondary_startup), 0);

	return apmu_power_on(cpu);
}
#endif

#if defined(CONFIG_HOTPLUG_CPU) || defined(CONFIG_SUSPEND)
/* nicked from arch/arm/mach-exynos/hotplug.c */
static inline void cpu_enter_lowpower_a15(void)
{
	unsigned int v;

	asm volatile(
	"       mrc     p15, 0, %0, c1, c0, 0\n"
	"       bic     %0, %0, %1\n"
	"       mcr     p15, 0, %0, c1, c0, 0\n"
		: "=&r" (v)
		: "Ir" (CR_C)
		: "cc");

	flush_cache_louis();

	asm volatile(
	/*
	 * Turn off coherency
	 */
	"       mrc     p15, 0, %0, c1, c0, 1\n"
	"       bic     %0, %0, %1\n"
	"       mcr     p15, 0, %0, c1, c0, 1\n"
		: "=&r" (v)
		: "Ir" (0x40)
		: "cc");

	isb();
	dsb();
}

void shmobile_smp_apmu_cpu_shutdown(unsigned int cpu)
{

	/* Select next sleep mode using the APMU */
	apmu_power_off(cpu);

	/* Do ARM specific CPU shutdown */
	cpu_enter_lowpower_a15();
}

static inline void cpu_leave_lowpower(void)
{
	unsigned int v;

	asm volatile("mrc    p15, 0, %0, c1, c0, 0\n"
		     "       orr     %0, %0, %1\n"
		     "       mcr     p15, 0, %0, c1, c0, 0\n"
		     "       mrc     p15, 0, %0, c1, c0, 1\n"
		     "       orr     %0, %0, %2\n"
		     "       mcr     p15, 0, %0, c1, c0, 1\n"
		     : "=&r" (v)
		     : "Ir" (CR_C), "Ir" (0x40)
		     : "cc");
}
#endif

#if defined(CONFIG_HOTPLUG_CPU)
void shmobile_smp_apmu_cpu_die(unsigned int cpu)
{
	/* For this particular CPU deregister boot vector */
	shmobile_smp_hook(cpu, 0, 0);

	/* Shutdown CPU core */
	shmobile_smp_apmu_cpu_shutdown(cpu);

	/* jump to shared mach-shmobile sleep / reset code */
	shmobile_smp_sleep();
}

int shmobile_smp_apmu_cpu_kill(unsigned int cpu)
{
	return apmu_power_off_poll(cpu);
}
#endif

#if defined(CONFIG_SUSPEND)
static int shmobile_smp_apmu_do_suspend(unsigned long cpu)
{
	shmobile_smp_hook(cpu, virt_to_phys(cpu_resume), 0);
	shmobile_smp_apmu_cpu_shutdown(cpu);
	cpu_do_idle(); /* WFI selects Core Standby */
	return 1;
}

static int shmobile_smp_apmu_enter_suspend(suspend_state_t state)
{
	cpu_suspend(smp_processor_id(), shmobile_smp_apmu_do_suspend);
	cpu_leave_lowpower();
	return 0;
}

void __init shmobile_smp_apmu_suspend_init(void)
{
	shmobile_suspend_ops.enter = shmobile_smp_apmu_enter_suspend;
}
#endif
