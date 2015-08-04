/*
 * linux/arch/arm/include/asm/arm-pd.h
 *
 * Copyright (C) 2015 Linaro Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARM_PD_H__
#define __ARM_PD_H__

struct of_arm_pd_ops {
	int (*init)(struct device_node *dn, struct generic_pm_domain *d);
	int (*power_on)(struct generic_pm_domain *d);
	int (*power_off)(struct generic_pm_domain *d);
};

struct of_arm_pd_method {
	const char *handle;
	struct of_arm_pd_ops *ops;
};

#define ARM_PD_METHOD_OF_DECLARE(_name, _handle, _ops)	\
	static const struct of_arm_pd_method __arm_pd_method_of_table_##_name \
	__used __section(__arm_pd_method_of_table)		\
	= { .handle = _handle, .ops = _ops }

#endif /* __ARM_PD_H__ */
