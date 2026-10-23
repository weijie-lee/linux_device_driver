// SPDX-License-Identifier: GPL-2.0
/*
 * virt_lock.c — 同一把计数在三种上下文里的值，以及 mutex 能睡、自旋不能睡。
 *
 * 这台内核是 PREEMPT_VOLUNTARY，没有 CONFIG_PREEMPT_COUNT。
 * spin_lock() 不增加 preempt_count 的低 8 位，preempt_disable() 只是编译屏障。
 * 硬中断入口仍然把 HARDIRQ_OFFSET 加进同一个计数，in_hardirq() 因此仍为真。
 *
 * mutex 里调用 msleep() 会进 schedule_timeout()。
 * lockup 属性在开着中断的前提下空转数秒，不调用 schedule()。
 * 软锁检测器靠“中断还在跑、但这个 CPU 没有调度”来打印 soft lockup。
 */
#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/irq_work.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/preempt.h>
#include <linux/smp.h>
#include <linux/spinlock.h>

static DEFINE_SPINLOCK(lock_slock);
static DEFINE_MUTEX(lock_mlock);
static atomic_t lock_atomic = ATOMIC_INIT(0);
static DEFINE_PER_CPU(int, lock_hits);

static unsigned int task_pc, spin_pc, mutex_pc, hard_pc, hard_in_irq;
static struct irq_work lock_iwork;

static noinline void virt_lock_sample_hard(struct irq_work *work)
{
	hard_pc = preempt_count();
	hard_in_irq = in_hardirq() ? 1 : 0;
}

static void virt_lock_bump(void *info)
{
	this_cpu_inc(lock_hits);
	atomic_inc(&lock_atomic);
}

static ssize_t context_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	task_pc = preempt_count();
	spin_lock(&lock_slock);
	spin_pc = preempt_count();
	spin_unlock(&lock_slock);
	mutex_lock(&lock_mlock);
	mutex_pc = preempt_count();
	mutex_unlock(&lock_mlock);

	/* 清掉上一次硬中断里记下的值，再等这一次 IRQ work 写回来。 */
	hard_pc = 0;
	hard_in_irq = 0;
	irq_work_queue(&lock_iwork);
	irq_work_sync(&lock_iwork);

	return sysfs_emit(buf, "task=%u spin=%u mutex=%u hard=%u hard_in_irq=%u\n",
			  task_pc, spin_pc, mutex_pc, hard_pc, hard_in_irq);
}
static DEVICE_ATTR_RO(context);

static ssize_t spread_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	int cpu, sum = 0;

	for_each_online_cpu(cpu)
		sum += per_cpu(lock_hits, cpu);
	return sysfs_emit(buf, "cpu0=%d cpu1=%d sum=%d atomic=%d\n",
			  per_cpu(lock_hits, 0), per_cpu(lock_hits, 1),
			  sum, atomic_read(&lock_atomic));
}
static DEVICE_ATTR_RO(spread);

static noinline void virt_lock_mutex_sleep(void)
{
	mutex_lock(&lock_mlock);
	msleep(20);
	mutex_unlock(&lock_mlock);
}

static ssize_t mutex_sleep_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	virt_lock_mutex_sleep();
	return count;
}
static DEVICE_ATTR_WO(mutex_sleep);

/*
 * 空转期间不调用 schedule()，也不关中断。jiffies 仍在走，循环能自己结束。
 * 软锁阈值是 2 * watchdog_thresh 秒，实验先把阈值改成 1。
 */
static noinline void virt_lock_spin(void)
{
	unsigned long until = jiffies + 5 * HZ;

	while (time_before(jiffies, until))
		cpu_relax();
}

static ssize_t lockup_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	pr_info("virt_lock: virt_lock_spin starts\n");
	virt_lock_spin();
	pr_info("virt_lock: virt_lock_spin ends\n");
	return count;
}
static DEVICE_ATTR_WO(lockup);

static struct attribute *lock_attrs[] = {
	&dev_attr_context.attr,
	&dev_attr_spread.attr,
	&dev_attr_mutex_sleep.attr,
	&dev_attr_lockup.attr,
	NULL,
};
ATTRIBUTE_GROUPS(lock);

static const struct file_operations lock_fops = {
	.owner = THIS_MODULE,
};

static struct miscdevice lock_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "virt_lock",
	.fops = &lock_fops,
	.groups = lock_groups,
};

static int __init virt_lock_init(void)
{
	lock_iwork = IRQ_WORK_INIT_HARD(virt_lock_sample_hard);
	on_each_cpu(virt_lock_bump, NULL, 1);
	return misc_register(&lock_dev);
}

static void __exit virt_lock_exit(void)
{
	irq_work_sync(&lock_iwork);
	misc_deregister(&lock_dev);
}

module_init(virt_lock_init);
module_exit(virt_lock_exit);

MODULE_AUTHOR("Linux Device Driver Lab");
MODULE_DESCRIPTION("preempt_count, per-CPU, mutex sleep and a soft lockup");
MODULE_LICENSE("GPL");
