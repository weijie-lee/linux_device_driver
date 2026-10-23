// SPDX-License-Identifier: GPL-2.0
/*
 * virt_bottom.c — 一次软件中断穿过硬中断、软中断、中断线程和工作队列。
 *
 * 客户机没有空闲的硬件中断线。模块建一个 1 个 hwirq 的 irqdomain，
 * 从 IRQ_WORK_HARD 里调用 generic_handle_domain_irq()。
 * 上半部只计数并 tasklet_schedule()，然后 IRQ_WAKE_THREAD。
 * tasklet 在软中断里跑，线程里再 schedule_work()。
 *
 * 回调都标了 noinline，函数跟踪里才有单独的名字。
 * 每一步另外打 virt_bottom_step 事件，事件跟踪不依赖调用栈。
 */
#define CREATE_TRACE_POINTS
#include "virt_bottom_trace.h"

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irq_work.h>
#include <linux/irqdomain.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

struct virt_bottom {
	struct irq_domain	*domain;
	struct irq_work		iwork;
	struct tasklet_struct	tasklet;
	struct work_struct	work;
	unsigned int		virq;
	spinlock_t		lock;
	unsigned int		hard;
	unsigned int		thread;
	unsigned int		tasklet_n;
	unsigned int		work_n;
	unsigned int		hard_in_irq;
	unsigned int		tasklet_in_softirq;
	unsigned int		thread_in_task;
	int			raise_errno;
};

static void virt_bottom_mask(struct irq_data *data)
{
}

static void virt_bottom_unmask(struct irq_data *data)
{
}

static struct irq_chip virt_bottom_chip = {
	.name = "virt-bottom",
	.irq_mask = virt_bottom_mask,
	.irq_unmask = virt_bottom_unmask,
};

static noinline void virt_bottom_work_fn(struct work_struct *work)
{
	struct virt_bottom *v = container_of(work, struct virt_bottom, work);
	unsigned int n;

	spin_lock(&v->lock);
	v->work_n++;
	n = v->work_n;
	spin_unlock(&v->lock);
	trace_virt_bottom_step("work", n);
}

static noinline void virt_bottom_tasklet_fn(struct tasklet_struct *t)
{
	struct virt_bottom *v = from_tasklet(v, t, tasklet);
	unsigned long flags;
	unsigned int n;

	spin_lock_irqsave(&v->lock, flags);
	v->tasklet_n++;
	v->tasklet_in_softirq = in_serving_softirq() ? 1 : 0;
	n = v->tasklet_n;
	spin_unlock_irqrestore(&v->lock, flags);
	trace_virt_bottom_step("tasklet", n);
}

static noinline irqreturn_t virt_bottom_hard(int irq, void *data)
{
	struct virt_bottom *v = data;
	unsigned long flags;
	unsigned int n;

	spin_lock_irqsave(&v->lock, flags);
	v->hard++;
	v->hard_in_irq = in_hardirq() ? 1 : 0;
	n = v->hard;
	spin_unlock_irqrestore(&v->lock, flags);
	trace_virt_bottom_step("hard", n);
	tasklet_schedule(&v->tasklet);
	return IRQ_WAKE_THREAD;
}

static noinline irqreturn_t virt_bottom_thread(int irq, void *data)
{
	struct virt_bottom *v = data;
	unsigned long flags;
	unsigned int n;

	spin_lock_irqsave(&v->lock, flags);
	v->thread++;
	v->thread_in_task = in_task() ? 1 : 0;
	n = v->thread;
	spin_unlock_irqrestore(&v->lock, flags);
	trace_virt_bottom_step("thread", n);
	schedule_work(&v->work);
	return IRQ_HANDLED;
}

static noinline void virt_bottom_raise(struct irq_work *work)
{
	struct virt_bottom *v = container_of(work, struct virt_bottom, iwork);
	unsigned long flags;
	int ret;

	ret = generic_handle_domain_irq(v->domain, 0);
	spin_lock_irqsave(&v->lock, flags);
	v->raise_errno = ret;
	spin_unlock_irqrestore(&v->lock, flags);
	trace_virt_bottom_step("raise", ret ? 0 : 1);
}

static int virt_bottom_map(struct irq_domain *d, unsigned int virq,
			   irq_hw_number_t hwirq)
{
	irq_set_chip_and_handler(virq, d->host_data, handle_level_irq);
	return 0;
}

static const struct irq_domain_ops virt_bottom_domain_ops = {
	.map = virt_bottom_map,
};

static ssize_t counts_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct virt_bottom *v = dev_get_drvdata(dev);
	unsigned long flags;
	unsigned int hard, thread, tasklet_n, work_n;
	unsigned int hin, sin, tin;
	int raise;

	spin_lock_irqsave(&v->lock, flags);
	hard = v->hard;
	thread = v->thread;
	tasklet_n = v->tasklet_n;
	work_n = v->work_n;
	hin = v->hard_in_irq;
	sin = v->tasklet_in_softirq;
	tin = v->thread_in_task;
	raise = v->raise_errno;
	spin_unlock_irqrestore(&v->lock, flags);

	return sysfs_emit(buf, "%u %u %u %u %u %u %u %d\n",
			  hard, thread, tasklet_n, work_n, hin, sin, tin, raise);
}
static DEVICE_ATTR_RO(counts);

static ssize_t kick_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct virt_bottom *v = dev_get_drvdata(dev);

	irq_work_queue(&v->iwork);
	return count;
}
static DEVICE_ATTR_WO(kick);

static int virt_bottom_probe(struct platform_device *pdev)
{
	struct virt_bottom *v;
	int ret;

	v = devm_kzalloc(&pdev->dev, sizeof(*v), GFP_KERNEL);
	if (!v)
		return -ENOMEM;
	spin_lock_init(&v->lock);
	tasklet_setup(&v->tasklet, virt_bottom_tasklet_fn);
	INIT_WORK(&v->work, virt_bottom_work_fn);
	v->iwork = IRQ_WORK_INIT_HARD(virt_bottom_raise);
	v->raise_errno = -ENODATA;
	platform_set_drvdata(pdev, v);

	v->domain = irq_domain_add_linear(NULL, 1, &virt_bottom_domain_ops,
					  &virt_bottom_chip);
	if (!v->domain)
		return -ENOMEM;
	v->virq = irq_create_mapping(v->domain, 0);
	if (!v->virq) {
		ret = -EINVAL;
		goto err_domain;
	}
	ret = request_threaded_irq(v->virq, virt_bottom_hard, virt_bottom_thread,
				   IRQF_ONESHOT, "virt_bottom", v);
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

static int virt_bottom_remove(struct platform_device *pdev)
{
	struct virt_bottom *v = platform_get_drvdata(pdev);

	irq_work_sync(&v->iwork);
	free_irq(v->virq, v);
	tasklet_kill(&v->tasklet);
	cancel_work_sync(&v->work);
	irq_dispose_mapping(v->virq);
	irq_domain_remove(v->domain);
	return 0;
}

static struct platform_driver virt_bottom_driver = {
	.probe = virt_bottom_probe,
	.remove = virt_bottom_remove,
	.driver = {
		.name = "virt_bottom",
	},
};

static struct platform_device *virt_bottom_pdev;

static int __init virt_bottom_init(void)
{
	int ret;

	ret = platform_driver_register(&virt_bottom_driver);
	if (ret)
		return ret;
	virt_bottom_pdev = platform_device_register_simple("virt_bottom",
							   PLATFORM_DEVID_NONE,
							   NULL, 0);
	if (IS_ERR(virt_bottom_pdev)) {
		ret = PTR_ERR(virt_bottom_pdev);
		platform_driver_unregister(&virt_bottom_driver);
		return ret;
	}
	if (!virt_bottom_pdev->dev.driver) {
		platform_device_unregister(virt_bottom_pdev);
		platform_driver_unregister(&virt_bottom_driver);
		return -ENODEV;
	}
	return 0;
}

static void __exit virt_bottom_exit(void)
{
	platform_device_unregister(virt_bottom_pdev);
	platform_driver_unregister(&virt_bottom_driver);
}

module_init(virt_bottom_init);
module_exit(virt_bottom_exit);

MODULE_AUTHOR("Linux Device Driver Lab");
MODULE_DESCRIPTION("Hardirq, softirq, threaded IRQ and workqueue on one raise");
MODULE_LICENSE("GPL");
