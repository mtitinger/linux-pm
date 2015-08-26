#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/pm_domain.h>


#define pr_fake(fmt, ...) printk(KERN_ERR "%s:%d: "fmt"\n", \
                                __func__, __LINE__, ##__VA_ARGS__);

int fake_domain_set_pstate(struct generic_pm_domain *domain, unsigned int pstate)
{
	pr_fake(" domain = %s ", domain->name);
	return 0;
}

int fake_domain_power_off(struct generic_pm_domain *domain)
{
	pr_fake(" domain = %s ", domain->name);
	return 0;
}

int fake_domain_power_on(struct generic_pm_domain *domain)
{
	pr_fake(" domain = %s ", domain->name);
	return 0;
}

int fake_domain_attach_dev(struct generic_pm_domain *domain,
			struct device *dev)
{
	pr_fake(" domain = %s ", domain->name);
	return 0;
}

void fake_domain_detach_dev(struct generic_pm_domain *domain,
		struct device *dev)
{
	pr_fake(" domain = %s ", domain->name);
}

struct generic_pm_domain* fn(struct of_phandle_args *args, void *data)
{
	pr_fake("");

	return (struct generic_pm_domain*) data;
}

extern int pm_genpd_add_pstates(struct generic_pm_domain *genpd,
			 struct generic_pm_domain *parent,
			 struct device_node *np);

static int fake_domain_domain_add(struct device_node *np, struct generic_pm_domain *parent)
{
	struct device_node *child;
	struct generic_pm_domain *genpd;
	int ret;

	pr_fake("");

	genpd = kzalloc(sizeof(*genpd), GFP_KERNEL);
	if (!genpd)
		return -ENOMEM;

	genpd->name = np->name;
        genpd->power_off                = fake_domain_power_off;
        genpd->power_on                 = fake_domain_power_on;
        genpd->set_pstate               = fake_domain_set_pstate;
        genpd->attach_dev               = fake_domain_attach_dev;
        genpd->detach_dev               = fake_domain_detach_dev;
	pm_genpd_init(genpd, &simple_qos_governor, false);

	__of_genpd_add_provider(np, fn, genpd);

	if (parent)
		pm_genpd_add_subdomain(parent, genpd);

	ret = pm_genpd_add_pstates(genpd, parent, np);
	if (ret)
		printk("Error adding pstate\n");

	for_each_child_of_node(np, child) {
		fake_domain_domain_add(child, genpd);
	}

	return 0;
}

static int fake_domain_domain_init(void)
{
	struct device_node *np, *parent;
	pr_fake("");

	for_each_compatible_node(parent, NULL, "fake,fake-pmdomain") {
		for_each_child_of_node(parent, np) {
			fake_domain_domain_add(np, NULL);
		}
	}

	return 0;
}
arch_initcall(fake_domain_domain_init);
