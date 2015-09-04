/*
 * ARM CPU Generic PM Domain.
 *
 * Copyright (C) 2015 Linaro Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpu_pm.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>

#include <asm/arm-pd.h>

#define NAME_MAX 36

struct arm_pm_domain {
	struct generic_pm_domain genpd;
	struct of_arm_pd_ops platform_ops;
};

extern struct of_arm_pd_method __arm_pd_method_of_table[];
static const struct of_arm_pd_method __arm_pd_method_of_table_sentinel
	__used __section(__arm_pd_method_of_table_end);

static inline
struct arm_pm_domain *to_arm_pd(struct generic_pm_domain *d)
{
	return container_of(d, struct arm_pm_domain, genpd);
}

static int arm_pd_power_down(struct generic_pm_domain *genpd)
{
	struct arm_pm_domain *pd = to_arm_pd(genpd);

	/*
	 * Notify CPU PM domain power down
	 * TODO: Call the notificated directly from here.
	 */
	cpu_cluster_pm_enter();

	if (pd->platform_ops.power_off)
		return pd->platform_ops.power_off(genpd);

	return 0;
}

static int arm_pd_power_up(struct generic_pm_domain *genpd)
{
	struct arm_pm_domain *pd = to_arm_pd(genpd);

	/* Notify CPU PM domain power up */
	cpu_cluster_pm_exit();

	if (pd->platform_ops.power_on)
		return pd->platform_ops.power_on(genpd);

	return 0;
}

static void __init run_cpu(void *unused)
{
	struct device *cpu_dev = get_cpu_device(smp_processor_id());

	/* We are running, increment the usage count */
	pm_runtime_get_noresume(cpu_dev);
}

static int __init arm_domain_cpu_init(void)
{
	int cpuid, ret;

	/* Find any CPU nodes with a phandle to this power domain */
	for_each_possible_cpu(cpuid) {
		struct device *cpu_dev;
		struct of_phandle_args pd_args;

		cpu_dev = get_cpu_device(cpuid);
		if (!cpu_dev) {
			pr_warn("%s: Unable to get device for CPU%d\n",
					__func__, cpuid);
			return -ENODEV;
		}

		/*
		 * We are only interested in CPUs that can be attached to
		 * PM domains that are arm,pd compatible.
		 */
		ret = of_parse_phandle_with_args(cpu_dev->of_node,
				"power-domains", "#power-domain-cells",
				0, &pd_args);
		if (ret) {
			dev_dbg(cpu_dev,
				"%s: Did not find a valid PM domain\n",
					__func__);
			continue;
		}

		if (!of_device_is_compatible(pd_args.np, "arm,pd")) {
			dev_dbg(cpu_dev, "%s: does not have an ARM PD\n",
					__func__);
			continue;
		}

		if (cpu_online(cpuid)) {
			pm_runtime_set_active(cpu_dev);
			/*
			 * Execute the below on that 'cpu' to ensure that the
			 * reference counting is correct. Its possible that
			 * while this code is executing, the 'cpu' may be
			 * powered down, but we may incorrectly increment the
			 * usage. By executing the get_cpu on the 'cpu',
			 * we can ensure that the 'cpu' and its usage count are
			 * matched.
			 */
			smp_call_function_single(cpuid, run_cpu, NULL, true);
		} else {
			pm_runtime_set_suspended(cpu_dev);
		}
		pm_runtime_irq_safe(cpu_dev);
		pm_runtime_enable(cpu_dev);

		/*
		 * We attempt to attach the device to genpd again. We would
		 * have failed in our earlier attempt to attach to the domain
		 * provider as the CPU device would not have been IRQ safe,
		 * while the domain is defined as IRQ safe. IRQ safe domains
		 * can only have IRQ safe devices.
		 */
		ret = genpd_dev_pm_attach(cpu_dev);
		if (ret) {
			dev_warn(cpu_dev,
				"%s: Unable to attach to power-domain: %d\n",
				__func__, ret);
			pm_runtime_disable(cpu_dev);
		}
	}

	return 0;
}

static int __init arm_domain_init(void)
{
	struct device_node *np;
	int count = 0;
	struct of_arm_pd_method *m = __arm_pd_method_of_table;

	for_each_compatible_node(np, NULL, "arm,pd") {
		struct arm_pm_domain *pd;

		if (!of_device_is_available(np))
			continue;

		pd = kzalloc(sizeof(*pd), GFP_KERNEL);
		if (!pd)
			return -ENOMEM;

		/* Invoke platform initialization for the PM domain */
		for (; m->handle; m++) {
			int ret;

			if (of_device_is_compatible(np, m->handle)) {
				ret = m->ops->init(np, &pd->genpd);
				if (!ret) {
					pr_debug("CPU PD ops found for %s\n",
							m->handle);
					pd->platform_ops.power_on =
						m->ops->power_on;
					pd->platform_ops.power_off =
						m->ops->power_off;
				}
				break;
			}
		}

		/* Initialize rest of CPU PM domain specifics */
		pd->genpd.name = kstrndup(np->name, NAME_MAX, GFP_KERNEL);
		pd->genpd.power_off = arm_pd_power_down;
		pd->genpd.power_on = arm_pd_power_up;
		pd->genpd.flags |= GENPD_FLAG_IRQ_SAFE;

		pr_debug("adding %s as generic power domain.\n", np->full_name);
		pm_genpd_init(&pd->genpd, &simple_qos_governor, NULL, 0, false);
		of_genpd_add_provider_simple(np, &pd->genpd);

		count++;
	}

	/* We have ARM PD(s), attach CPUs to their domain */
	if (count)
		return arm_domain_cpu_init();

	return 0;
}
device_initcall(arm_domain_init);
