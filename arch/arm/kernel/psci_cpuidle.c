/*
 * Copyright (C) 2015 Marvell Technology Group Ltd.
 * Author: Jisheng Zhang <jszhang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpuidle.h>
#include <linux/psci.h>

#include <asm/cpuidle.h>

static struct cpuidle_ops psci_cpuidle_ops __initdata = {
	.suspend = cpu_psci_cpu_suspend,
	.init = cpu_psci_cpu_init_idle,
};
CPUIDLE_METHOD_OF_DECLARE(psci_cpuidle, "psci", &psci_cpuidle_ops);
