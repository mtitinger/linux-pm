

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/pm_qos.h>

#include "fake.h"

#define DRIVER_NAME "fake_dev_driver"

#define pr_fake(fmt, ...) printk(KERN_ERR "%s +%d: "fmt"\n", \
                                __func__, __LINE__, ##__VA_ARGS__);


struct fake_dev_data {
	struct dev_pm_qos_request low_latency_req;
	struct dentry *dbg_dir;
	u64 save_latency_us;
	u64 max_off_time_us;
	u64 constraint;
	u32 pstate;
	bool const_set;
	const char *name;

	const char *genpd_name;
	const char *genpd_parent_name;

};

static int fake_dev_runtime_suspend(struct device *dev)
{
	pr_fake(" device = %s",dev_name(dev));
	return 0;
}

static int fake_dev_runtime_resume(struct device *dev)
{
	pr_fake(" device = %s",dev_name(dev));
	return 0;
}

static int fake_dev_runtime_idle(struct device *dev)
{
	pr_fake(" device = %s",dev_name(dev));
        pm_runtime_mark_last_busy(dev);
        pm_runtime_autosuspend(dev);
	return 0;
}

static int fake_dev_runtime_perf(struct device *dev, unsigned int pstate)
{
	pr_fake(" device = %s pstate = %d\n",dev_name(dev), pstate);
	return 0;
}

static const struct dev_pm_ops fake_dev_pm_ops = {
        .runtime_suspend = fake_dev_runtime_suspend,
        .runtime_resume = fake_dev_runtime_resume,
	.runtime_idle = fake_dev_runtime_idle,
	.runtime_perf = fake_dev_runtime_perf,
};

static int fake_dev_dbgfs_mode_get(void *data, u64 *val)
{
	pr_fake("");
	return 0;
}

static int fake_dev_dbgfs_add(void *data, u64 val)
{
	struct platform_device *pdev = (struct platform_device *)data;
	struct device *dev =  &pdev->dev;

	pr_fake("");
	pm_runtime_get_sync(dev);

	return 0;
}

static int fake_dev_dbgfs_remove(void *data, u64 val)
{
	struct platform_device *pdev = (struct platform_device *)data;
	struct device *dev =  &pdev->dev;

	pr_fake("");
	pm_runtime_put_sync(dev);

	return 0;
}

static int fake_dev_dbgfs_get_save_latency(void *data, u64 *val)
{
	struct platform_device *pdev = (struct platform_device *)data;
	struct fake_dev_data *driver_data = platform_get_drvdata(pdev);

	pr_fake("");
	*val = driver_data->save_latency_us;

	return 0;
}

static int fake_dev_dbgfs_set_save_latency(void *data, u64 val)
{
	struct platform_device *pdev = (struct platform_device *)data;
	struct fake_dev_data *driver_data = platform_get_drvdata(pdev);

	pr_fake("");
	driver_data->save_latency_us = val;

	return 0;
}

static int fake_dev_dbgfs_get_max_off_time_us(void *data, u64 *val)
{
	struct platform_device *pdev = (struct platform_device *)data;
	struct fake_dev_data *driver_data = platform_get_drvdata(pdev);

	pr_fake("");
	*val = driver_data->max_off_time_us;

	return 0;
}

static int fake_dev_dbgfs_set_max_off_time_us(void *data, u64 val)
{
	struct platform_device *pdev = (struct platform_device *)data;
	struct fake_dev_data *driver_data = platform_get_drvdata(pdev);
	struct device *dev =  &pdev->dev;

	pr_fake("");
	driver_data->max_off_time_us = val;

	if (!val && driver_data->const_set) {
		dev_pm_qos_remove_request(&driver_data->low_latency_req);
		driver_data->const_set = false;
	} else if (!driver_data->const_set) {
		dev_pm_qos_add_request(dev,
			&driver_data->low_latency_req,
			DEV_PM_QOS_RESUME_LATENCY, val);
		driver_data->const_set = true;
	}
	return 0;
}


extern int pm_genpd_pstate_req(struct device *dev, unsigned int pstate);

static int fake_dev_dbgfs_get_pstate(void *data, u64 *val)
{
	struct platform_device *pdev = (struct platform_device *)data;
	struct fake_dev_data *driver_data = platform_get_drvdata(pdev);

	pr_fake("");
	*val = driver_data->pstate;

	return 0;
}

static int fake_dev_dbgfs_set_pstate(void *data, u64 val)
{
	struct platform_device *pdev = (struct platform_device *)data;
	struct device *dev =  &pdev->dev;

	pr_fake("");
	pm_runtime_set_pstate(dev, (u32)val);

	return 0;
}




int dev_callback_start(struct device *dev)
{
	pr_fake("");
	mdelay(1);
	return 0;
}

int dev_callback_stop(struct device *dev)
{
	pr_fake("");
	mdelay(1);
	return 0;
}

int dev_callback_start_slow(struct device *dev)
{
	pr_fake("");
	mdelay(20);
	return 0;
}

int dev_callback_stop_slow(struct device *dev)
{
	pr_fake("");
	mdelay(20);
	return 0;
}


int dev_callback_restore(struct device *dev, int state)
{
	pr_fake("");
	return 0;
}

int dev_callback_save(struct device *dev, int state)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct fake_dev_data *driver_data = platform_get_drvdata(pdev);

	pr_fake("");
	udelay(driver_data->save_latency_us);

	return 0;
}

int dev_callback(struct device *dev)
{
	pr_fake("");
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fake_dev_dbgfs_mode_fops, fake_dev_dbgfs_mode_get,
		NULL, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fake_dev_dbgfs_add_fops, NULL,
		fake_dev_dbgfs_add, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fake_dev_dbgfs_remove_fops, NULL,
		fake_dev_dbgfs_remove, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fake_dev_dbgfs_save_latency_fops, fake_dev_dbgfs_get_save_latency,
		fake_dev_dbgfs_set_save_latency, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fake_dev_dbgfs_fops_max_off_time_us, fake_dev_dbgfs_get_max_off_time_us,
		fake_dev_dbgfs_set_max_off_time_us, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fake_dev_dbgfs_fops_pstate, fake_dev_dbgfs_get_pstate,
		fake_dev_dbgfs_set_pstate, "%llu\n");

// extern int genpd_dev_pm_attach_force(struct device *dev, bool replace);

static int fake_dev_probe(struct platform_device *pdev)
{
	struct fake_dev_data *driver_data;
	struct device *dev =  &pdev->dev;
	struct device_node *np;
	char string[255];
	int ret;

	np = dev->of_node;

	driver_data = devm_kzalloc(&pdev->dev, sizeof(*driver_data),
                                   GFP_KERNEL);
	if (!driver_data)
		return -ENOMEM;


	ret = of_property_read_string(np, "name",
			&driver_data->name);


	ret =  of_property_read_u64(np, "constraint",
			&driver_data->constraint);

	sprintf(string, "fake_dev_driver_%s", driver_data->name);

	driver_data->dbg_dir = debugfs_create_dir(string, NULL);
	debugfs_create_file("users", S_IRUGO, driver_data->dbg_dir,
		 pdev, &fake_dev_dbgfs_mode_fops);
	debugfs_create_file("rpm_get", S_IRUGO, driver_data->dbg_dir,
		 pdev, &fake_dev_dbgfs_add_fops);
	debugfs_create_file("rpm_put", S_IRUGO, driver_data->dbg_dir,
		 pdev, &fake_dev_dbgfs_remove_fops);
	debugfs_create_file("save_latency", S_IRUGO, driver_data->dbg_dir,
		 pdev, &fake_dev_dbgfs_save_latency_fops);
	debugfs_create_file("constraint_us", S_IRUGO, driver_data->dbg_dir,
		 pdev, &fake_dev_dbgfs_fops_max_off_time_us);
	debugfs_create_file("pstate", S_IRUGO, driver_data->dbg_dir,
		 pdev, &fake_dev_dbgfs_fops_pstate);


	platform_set_drvdata(pdev, driver_data);

	/* ask for irq_safe callbacks, so that we can add this fake driver
	 * to the Cluster's PD */
	pm_runtime_irq_safe(dev);

	ret = genpd_dev_pm_attach(dev);

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_set_autosuspend_delay(&pdev->dev, 2000);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_resume(&pdev->dev);

	return 0;
}

static int fake_dev_remove(struct platform_device *pdev)
{
	pr_fake("");
	return 0;
}

static const struct of_device_id fake_dev_match_table[] = {
	{ .compatible = "fake,fake-driver", .data = NULL },
}
MODULE_DEVICE_TABLE(of, fake_dev_match_table);

static struct platform_driver fake_dev_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.pm     = &fake_dev_pm_ops,
		.of_match_table = of_match_ptr(fake_dev_match_table),
	},
        .remove = fake_dev_remove,
	.probe = fake_dev_probe,
};

module_platform_driver(fake_dev_driver);
