// SPDX-License-Identifier: GPL-2.0
/*
 * virt_irq.c — 软件触发的中断：硬中断、threaded irq、workqueue。
 *
 * 客户机没有空闲的硬件中断线。模块建一个只有 1 个 hwirq 的 irqdomain，
 * 用 IRQ_WORK_HARD 在硬中断里调用 generic_handle_domain_irq()。
 * 上半部只拿自旋锁计数，然后 IRQ_WAKE_THREAD。线程里可以拿 mutex。
 * 线程再 schedule_work()，工作队列是第三条可以睡眠的路径。
 *
 * 这版内核 CONFIG_PROVE_LOCKING 和 CONFIG_DEBUG_ATOMIC_SLEEP 都没打开，
 * 所以不会在加载时打印 lockdep 报告。上下文用 in_interrupt()/in_task() 记下来。
 * 若把 mutex_lock 放进上半部，打开 lockdep 的内核会报：
 *   possible circular locking dependency
 * 或 illegal sleep in atomic context。
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/irq_work.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/preempt.h>

struct virt_irq {
	struct irq_domain	*domain;
	struct irq_work		iwork;
	struct work_struct	work;
	unsigned int		virq;
	spinlock_t		slock;
	struct mutex		mlock;
	unsigned int		hard_count;
	unsigned int		thread_count;
	unsigned int		work_count;
	unsigned int		hard_in_interrupt;
	unsigned int		thread_in_task;
	int			raise_errno;
};

static void virt_irq_mask(struct irq_data *data)
{
}

static void virt_irq_unmask(struct irq_data *data)
{
}

static struct irq_chip virt_irq_chip = {
	.name = "virt-irq",
	.irq_mask = virt_irq_mask,
	.irq_unmask = virt_irq_unmask,
};

static void virt_work_fn(struct work_struct *work)
{
	struct virt_irq *v = container_of(work, struct virt_irq, work);

	mutex_lock(&v->mlock);
	v->work_count++;
	mutex_unlock(&v->mlock);
}

static irqreturn_t virt_irq_hard(int irq, void *data)
{
	struct virt_irq *v = data;
	unsigned long flags;

	spin_lock_irqsave(&v->slock, flags);
	v->hard_count++;
	v->hard_in_interrupt = in_interrupt() ? 1 : 0;
	spin_unlock_irqrestore(&v->slock, flags);
	return IRQ_WAKE_THREAD;
}

static irqreturn_t virt_irq_thread(int irq, void *data)
{
	struct virt_irq *v = data;

	mutex_lock(&v->mlock);
	v->thread_count++;
	v->thread_in_task = in_task() ? 1 : 0;
	mutex_unlock(&v->mlock);
	schedule_work(&v->work);
	return IRQ_HANDLED;
}

static void virt_irq_raise(struct irq_work *work)
{
	struct virt_irq *v = container_of(work, struct virt_irq, iwork);
	unsigned long flags;
	int ret;

	ret = generic_handle_domain_irq(v->domain, 0);
	spin_lock_irqsave(&v->slock, flags);
	v->raise_errno = ret;
	spin_unlock_irqrestore(&v->slock, flags);
	if (ret)
		pr_err("virt_irq: generic_handle_domain_irq returned %d\n", ret);
}

static int virt_irq_map(struct irq_domain *d, unsigned int virq,
			irq_hw_number_t hwirq)
{
	struct irq_chip *chip = d->host_data;

	irq_set_chip_and_handler(virq, chip, handle_level_irq);
	return 0;
}

static const struct irq_domain_ops virt_irq_domain_ops = {
	.map = virt_irq_map,
};

static ssize_t counts_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct virt_irq *v = dev_get_drvdata(dev);
	unsigned int hard, hin;
	unsigned int thread, work, tin;
	unsigned long flags;
	int raise;

	spin_lock_irqsave(&v->slock, flags);
	hard = v->hard_count;
	hin = v->hard_in_interrupt;
	raise = v->raise_errno;
	spin_unlock_irqrestore(&v->slock, flags);

	mutex_lock(&v->mlock);
	thread = v->thread_count;
	work = v->work_count;
	tin = v->thread_in_task;
	mutex_unlock(&v->mlock);

	return sysfs_emit(buf, "%u %u %u %u %u %d\n",
			  hard, thread, work, hin, tin, raise);
}
static DEVICE_ATTR_RO(counts);

static ssize_t kick_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct virt_irq *v = dev_get_drvdata(dev);

	irq_work_queue(&v->iwork);
	return count;
}
static DEVICE_ATTR_WO(kick);

static int virt_irq_probe(struct platform_device *pdev)
{
	struct virt_irq *v;
	int ret;

	v = devm_kzalloc(&pdev->dev, sizeof(*v), GFP_KERNEL);
	if (!v)
		return -ENOMEM;
	spin_lock_init(&v->slock);
	mutex_init(&v->mlock);
	INIT_WORK(&v->work, virt_work_fn);
	v->iwork = IRQ_WORK_INIT_HARD(virt_irq_raise);
	v->raise_errno = -ENODATA;
	platform_set_drvdata(pdev, v);

	v->domain = irq_domain_add_linear(NULL, 1, &virt_irq_domain_ops,
					  &virt_irq_chip);
	if (!v->domain)
		return -ENOMEM;

	v->virq = irq_create_mapping(v->domain, 0);
	if (!v->virq) {
		ret = -EINVAL;
		goto err_domain;
	}

	ret = request_threaded_irq(v->virq, virt_irq_hard, virt_irq_thread,
				   IRQF_ONESHOT, "virt_irq", v);
	if (ret)
		goto err_map;

	ret = device_create_file(&pdev->dev, &dev_attr_counts);
	if (ret)
		goto err_irq;
	ret = device_create_file(&pdev->dev, &dev_attr_kick);
	if (ret)
		goto err_irq;

	dev_info(&pdev->dev, "virtual irq %u\n", v->virq);
	return 0;

err_irq:
	free_irq(v->virq, v);
err_map:
	irq_dispose_mapping(v->virq);
err_domain:
	irq_domain_remove(v->domain);
	v->domain = NULL;
	return ret;
}

static int virt_irq_remove(struct platform_device *pdev)
{
	struct virt_irq *v = platform_get_drvdata(pdev);

	irq_work_sync(&v->iwork);
	free_irq(v->virq, v);
	cancel_work_sync(&v->work);
	irq_dispose_mapping(v->virq);
	irq_domain_remove(v->domain);
	return 0;
}

static struct platform_driver virt_irq_driver = {
	.probe = virt_irq_probe,
	.remove = virt_irq_remove,
	.driver = {
		.name = "virt_irq",
		.owner = THIS_MODULE,
	},
};

static struct platform_device *virt_irq_pdev;

static int __init virt_irq_init(void)
{
	int ret;

	ret = platform_driver_register(&virt_irq_driver);
	if (ret)
		return ret;
	virt_irq_pdev = platform_device_register_simple("virt_irq",
							PLATFORM_DEVID_NONE,
							NULL, 0);
	if (IS_ERR(virt_irq_pdev)) {
		ret = PTR_ERR(virt_irq_pdev);
		platform_driver_unregister(&virt_irq_driver);
		return ret;
	}
	if (!virt_irq_pdev->dev.driver) {
		platform_device_unregister(virt_irq_pdev);
		platform_driver_unregister(&virt_irq_driver);
		return -ENODEV;
	}
	return 0;
}

static void __exit virt_irq_exit(void)
{
	platform_device_unregister(virt_irq_pdev);
	platform_driver_unregister(&virt_irq_driver);
}

module_init(virt_irq_init);
module_exit(virt_irq_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Virtual threaded IRQ raised from hard irq_work");
