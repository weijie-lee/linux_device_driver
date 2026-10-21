// SPDX-License-Identifier: GPL-2.0
/*
 * virt_sensor.c — 一个 probe 里串起 regmap、时钟和 threaded irq。
 *
 * 寄存器文件是 4 个 u32，经 regmap 读写：
 *   0 ID，1 CTRL，2 TEMP，3 THRESHOLD
 * 时钟是 24 MHz 固定钟，用 clkdev 登记成 "sensor"，再 devm_clk_get。
 * 温度写入 sysfs 后用硬中断里的 irq_work 触发 threaded handler，
 * handler 里 regmap_read，因为这里可以睡眠，上半部不行。
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/irq_work.h>
#include <linux/mutex.h>

#define REG_ID		0
#define REG_CTRL	1
#define REG_TEMP	2
#define REG_TH		3
#define SENSOR_ID	0x5e

struct virt_sensor {
	u32			regs[4];
	struct regmap		*map;
	struct clk		*clk;
	unsigned long		rate;
	struct irq_domain	*domain;
	struct irq_work		iwork;
	unsigned int		virq;
	struct mutex		lock;
	unsigned int		irq_count;
	unsigned int		last_temp;
};

static void sensor_mask(struct irq_data *data)
{
}

static void sensor_unmask(struct irq_data *data)
{
}

static struct irq_chip sensor_chip = {
	.name = "virt-sensor",
	.irq_mask = sensor_mask,
	.irq_unmask = sensor_unmask,
};

static int sensor_reg_read(void *ctx, unsigned int reg, unsigned int *val)
{
	struct virt_sensor *s = ctx;

	if (reg > REG_TH)
		return -EINVAL;
	*val = s->regs[reg];
	return 0;
}

static int sensor_reg_write(void *ctx, unsigned int reg, unsigned int val)
{
	struct virt_sensor *s = ctx;

	if (reg > REG_TH)
		return -EINVAL;
	s->regs[reg] = val;
	return 0;
}

static const struct regmap_config sensor_regmap_cfg = {
	.reg_bits = 8,
	.val_bits = 32,
	.max_register = REG_TH,
	.reg_read = sensor_reg_read,
	.reg_write = sensor_reg_write,
	.cache_type = REGCACHE_NONE,
};

static irqreturn_t sensor_hard(int irq, void *data)
{
	return IRQ_WAKE_THREAD;
}

static irqreturn_t sensor_thread(int irq, void *data)
{
	struct virt_sensor *s = data;
	unsigned int val = 0;

	regmap_read(s->map, REG_TEMP, &val);
	mutex_lock(&s->lock);
	s->last_temp = val;
	s->irq_count++;
	mutex_unlock(&s->lock);
	return IRQ_HANDLED;
}

static void sensor_raise(struct irq_work *work)
{
	struct virt_sensor *s = container_of(work, struct virt_sensor, iwork);

	generic_handle_domain_irq(s->domain, 0);
}

static int sensor_map(struct irq_domain *d, unsigned int virq,
		      irq_hw_number_t hwirq)
{
	irq_set_chip_and_handler(virq, d->host_data, handle_level_irq);
	return 0;
}

static const struct irq_domain_ops sensor_domain_ops = {
	.map = sensor_map,
};

static ssize_t temperature_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct virt_sensor *s = dev_get_drvdata(dev);
	unsigned int val;
	int ret;

	ret = regmap_read(s->map, REG_TEMP, &val);
	if (ret)
		return ret;
	return sysfs_emit(buf, "%u\n", val);
}

static ssize_t temperature_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct virt_sensor *s = dev_get_drvdata(dev);
	unsigned int val;
	int ret;

	ret = kstrtouint(buf, 0, &val);
	if (ret)
		return ret;
	ret = regmap_write(s->map, REG_TEMP, val);
	if (ret)
		return ret;
	irq_work_queue(&s->iwork);
	return count;
}
static DEVICE_ATTR_RW(temperature);

static ssize_t clock_hz_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct virt_sensor *s = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%lu\n", s->rate);
}
static DEVICE_ATTR_RO(clock_hz);

static ssize_t irq_count_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct virt_sensor *s = dev_get_drvdata(dev);
	unsigned int n;

	mutex_lock(&s->lock);
	n = s->irq_count;
	mutex_unlock(&s->lock);
	return sysfs_emit(buf, "%u\n", n);
}
static DEVICE_ATTR_RO(irq_count);

static ssize_t last_temp_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct virt_sensor *s = dev_get_drvdata(dev);
	unsigned int v;

	mutex_lock(&s->lock);
	v = s->last_temp;
	mutex_unlock(&s->lock);
	return sysfs_emit(buf, "%u\n", v);
}
static DEVICE_ATTR_RO(last_temp);

static int sensor_probe(struct platform_device *pdev)
{
	struct virt_sensor *s;
	struct clk_hw *hw;
	int ret;

	s = devm_kzalloc(&pdev->dev, sizeof(*s), GFP_KERNEL);
	if (!s)
		return -ENOMEM;
	mutex_init(&s->lock);
	s->iwork = IRQ_WORK_INIT_HARD(sensor_raise);
	platform_set_drvdata(pdev, s);

	s->map = devm_regmap_init(&pdev->dev, NULL, s, &sensor_regmap_cfg);
	if (IS_ERR(s->map))
		return PTR_ERR(s->map);

	hw = devm_clk_hw_register_fixed_rate(&pdev->dev, "virt_sensor_clk",
					     NULL, 0, 24000000);
	if (IS_ERR(hw))
		return PTR_ERR(hw);
	ret = devm_clk_hw_register_clkdev(&pdev->dev, hw, "sensor",
					  dev_name(&pdev->dev));
	if (ret)
		return ret;
	s->clk = clk_get(&pdev->dev, "sensor");
	if (IS_ERR(s->clk))
		return PTR_ERR(s->clk);
	ret = clk_prepare_enable(s->clk);
	if (ret) {
		clk_put(s->clk);
		return ret;
	}
	s->rate = clk_get_rate(s->clk);
	/*
	 * 时钟的 owner 在本模块上。一直拿着 clk 引用时 rmmod 会返回 EAGAIN，
	 * exit 没有机会运行。频率已经记下，这里放开引用。
	 */
	clk_disable_unprepare(s->clk);
	clk_put(s->clk);
	s->clk = NULL;

	s->domain = irq_domain_add_linear(NULL, 1, &sensor_domain_ops,
					  &sensor_chip);
	if (!s->domain)
		return -ENOMEM;
	s->virq = irq_create_mapping(s->domain, 0);
	if (!s->virq) {
		ret = -EINVAL;
		goto err_domain;
	}
	ret = request_threaded_irq(s->virq, sensor_hard, sensor_thread,
				   IRQF_ONESHOT, "virt_sensor", s);
	if (ret)
		goto err_map;

	ret = regmap_write(s->map, REG_ID, SENSOR_ID);
	if (ret)
		goto err_irq;
	ret = regmap_write(s->map, REG_CTRL, 1);
	if (ret)
		goto err_irq;

	ret = device_create_file(&pdev->dev, &dev_attr_temperature);
	if (ret)
		goto err_irq;
	ret = device_create_file(&pdev->dev, &dev_attr_clock_hz);
	if (ret)
		goto err_irq;
	ret = device_create_file(&pdev->dev, &dev_attr_irq_count);
	if (ret)
		goto err_irq;
	ret = device_create_file(&pdev->dev, &dev_attr_last_temp);
	if (ret)
		goto err_irq;

	dev_info(&pdev->dev, "clk %lu Hz irq %u\n", s->rate, s->virq);
	return 0;

err_irq:
	free_irq(s->virq, s);
err_map:
	irq_dispose_mapping(s->virq);
err_domain:
	irq_domain_remove(s->domain);
	s->domain = NULL;
	return ret;
}

static int sensor_remove(struct platform_device *pdev)
{
	struct virt_sensor *s = platform_get_drvdata(pdev);

	irq_work_sync(&s->iwork);
	free_irq(s->virq, s);
	irq_dispose_mapping(s->virq);
	irq_domain_remove(s->domain);
	return 0;
}

static struct platform_driver sensor_driver = {
	.probe = sensor_probe,
	.remove = sensor_remove,
	.driver = {
		.name = "virt_sensor",
		.owner = THIS_MODULE,
	},
};

static struct platform_device *sensor_pdev;

static int __init sensor_init(void)
{
	int ret;

	ret = platform_driver_register(&sensor_driver);
	if (ret)
		return ret;
	sensor_pdev = platform_device_register_simple("virt_sensor",
						      PLATFORM_DEVID_NONE,
						      NULL, 0);
	if (IS_ERR(sensor_pdev)) {
		platform_driver_unregister(&sensor_driver);
		return PTR_ERR(sensor_pdev);
	}
	if (!sensor_pdev->dev.driver) {
		platform_device_unregister(sensor_pdev);
		platform_driver_unregister(&sensor_driver);
		return -ENODEV;
	}
	return 0;
}

static void __exit sensor_exit(void)
{
	platform_device_unregister(sensor_pdev);
	platform_driver_unregister(&sensor_driver);
}

module_init(sensor_init);
module_exit(sensor_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Virtual sensor combining regmap, clk and a threaded IRQ");
