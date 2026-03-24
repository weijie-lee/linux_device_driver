/*
 * second.c — 内核定时器演示：通过 /dev/second 读取已过秒数
 *
 * 【驱动架构概述】
 * 本驱动演示了 Linux 内核定时器（struct timer_list）的完整用法：
 *
 * 1. 定时器工作原理：
 *    - 内核定时器基于 jiffies 计数器，HZ 表示每秒的 jiffies 数
 *    - timer_setup() 初始化定时器并绑定回调函数
 *    - add_timer() 启动定时器，到期后自动调用回调
 *    - 回调函数中调用 mod_timer() 重新设置到期时间，实现周期触发
 *    - del_timer_sync() 停止定时器并等待回调函数执行完毕（防止竞态）
 *
 * 2. 原子操作：
 *    - 定时器回调运行在软中断上下文，不能使用互斥锁
 *    - 使用 atomic_t 和 atomic_inc/atomic_read 保证计数器的原子性
 *
 * 3. 设备使用方式：
 *    - open：启动定时器，计数器清零
 *    - read：读取当前计数值（已过秒数），以 int 形式返回
 *    - release：停止定时器
 *
 * 【整改记录】
 *   - 修复 init 中 cdev_add 失败后静默继续的问题
 *   - 修复 init 中 class_create 失败后资源泄漏问题
 *   - 统一使用 pr_info/pr_err 替代裸 printk
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/timer.h>		/* struct timer_list, add_timer, mod_timer, del_timer_sync */
#include <linux/atomic.h>		/* atomic_t, atomic_inc, atomic_read, atomic_set */

static dev_t devno;
static struct class *seconds_class;

/*
 * struct second_dev — 秒计数设备的私有数据结构体
 *
 *   cdev      — 内核字符设备结构体，通过 container_of 从 inode->i_cdev 获取
 *
 *   counter   — 原子计数器，记录已过秒数。
 *               使用 atomic_t 而非普通 int，因为定时器回调在软中断上下文中
 *               递增计数器，而 read 在进程上下文中读取，必须保证原子性。
 *
 *   s_timer   — 内核定时器。每 HZ 个 jiffies（即 1 秒）触发一次回调。
 *               回调函数中通过 mod_timer() 重新设置到期时间，实现周期性触发。
 */
struct second_dev {
	struct cdev cdev;
	atomic_t counter;		/* 原子计数器：已过秒数 */
	struct timer_list s_timer;	/* 内核定时器：每秒触发一次 */
};

static struct second_dev *second_devp;

/*
 * second_timer_handler — 定时器到期回调函数
 *
 * 【调用上下文】软中断（softirq），不能睡眠，不能使用互斥锁。
 *
 * 【执行流程】
 * 1. mod_timer() 重新设置到期时间为 jiffies + HZ（下一秒）
 *    注意：必须先重新设置定时器，再执行其他操作，防止定时器漂移。
 * 2. atomic_inc() 原子递增计数器
 * 3. 打印当前 jiffies 和已过秒数（调试用）
 *
 * 【参数说明】
 * unused — 新版内核（4.15+）的 timer_setup API 将 timer 指针传入回调，
 *          可用 from_timer(dev, t, s_timer) 宏从 timer 指针获取设备结构体。
 *          本驱动通过全局变量访问设备，故参数未使用。
 */
static void second_timer_handler(struct timer_list *unused)
{
	/* 重新设置定时器，下一秒再次触发（周期性定时器的标准写法） */
	mod_timer(&second_devp->s_timer, jiffies + HZ);

	/* 原子递增：定时器回调在软中断上下文，不能用普通 ++ 操作 */
	atomic_inc(&second_devp->counter);

	pr_info("second: jiffies=%ld, elapsed=%d s\n",
		jiffies, atomic_read(&second_devp->counter));
}

/*
 * second_open — 打开设备：初始化并启动定时器
 *
 * timer_setup() 是 4.15+ 内核的新 API，替代了旧的 setup_timer()。
 * 每次打开设备时重置计数器并启动新的定时器。
 *
 * 注意：本驱动使用全局 second_devp，不支持多进程同时打开。
 * 生产代码应在此处加引用计数或互斥锁防止并发打开。
 */
static int second_open(struct inode *inode, struct file *filp)
{
	/* 初始化定时器，绑定回调函数，flags 为 0（无特殊标志） */
	timer_setup(&second_devp->s_timer, second_timer_handler, 0);

	/* 设置第一次到期时间：当前 jiffies + 1 秒 */
	second_devp->s_timer.expires = jiffies + HZ;

	add_timer(&second_devp->s_timer);	/* 启动定时器 */
	atomic_set(&second_devp->counter, 0);	/* 计数器清零 */

	pr_info("second: device opened, timer started\n");
	return 0;
}

/*
 * second_release — 关闭设备：停止定时器
 *
 * del_timer_sync() 与 del_timer() 的区别：
 *   del_timer()      — 仅从定时器队列中移除，不等待正在执行的回调完成
 *   del_timer_sync() — 等待回调函数执行完毕后再返回（SMP 安全）
 *
 * 必须使用 del_timer_sync() 防止以下竞态：
 *   close() 释放 second_devp 后，定时器回调仍在另一个 CPU 上运行，
 *   访问已释放的内存，导致 use-after-free。
 */
static int second_release(struct inode *inode, struct file *filp)
{
	del_timer_sync(&second_devp->s_timer);	/* 等待回调完成后停止定时器 */
	pr_info("second: device closed, timer stopped\n");
	return 0;
}

/*
 * second_read — 读取已过秒数
 *
 * 将 atomic_t 计数器的当前值以 int 形式写入用户空间缓冲区。
 * put_user() 是 copy_to_user() 的单值简化版，适用于基本类型。
 *
 * 返回值：成功返回 sizeof(int)（4 字节），失败返回 -EFAULT。
 */
static ssize_t second_read(struct file *filp, char __user *buf,
			   size_t count, loff_t *ppos)
{
	int counter = atomic_read(&second_devp->counter);

	/* put_user：将内核变量 counter 写入用户空间指针 buf */
	if (put_user(counter, (int __user *)buf))
		return -EFAULT;

	return sizeof(int);
}

static const struct file_operations second_fops = {
	.owner   = THIS_MODULE,
	.open    = second_open,
	.release = second_release,
	.read    = second_read,
};

/*
 * second_init — 模块初始化
 *
 * 【资源分配顺序】（与 exit 相反）
 *   1. alloc_chrdev_region — 动态分配主/次设备号
 *   2. kzalloc             — 分配设备私有数据结构体
 *   3. cdev_init + cdev_add — 注册字符设备到内核
 *   4. class_create        — 创建设备类（/sys/class/seconds_class）
 *   5. device_create       — 创建设备节点（/dev/second）
 *
 * 【错误处理】使用 goto 标签确保失败时按逆序释放已分配的资源。
 */
static int __init second_init(void)
{
	int ret;

	/* 步骤 1：动态分配设备号（主设备号由内核分配，避免冲突） */
	ret = alloc_chrdev_region(&devno, 0, 1, "second");
	if (ret < 0) {
		pr_err("second: failed to allocate chrdev region, ret=%d\n", ret);
		return ret;
	}

	/* 步骤 2：分配设备私有数据（kzalloc 会将内存清零） */
	second_devp = kzalloc(sizeof(*second_devp), GFP_KERNEL);
	if (!second_devp) {
		ret = -ENOMEM;
		goto fail_malloc;
	}

	/* 步骤 3：初始化并注册字符设备 */
	cdev_init(&second_devp->cdev, &second_fops);
	second_devp->cdev.owner = THIS_MODULE;
	ret = cdev_add(&second_devp->cdev, devno, 1);
	if (ret) {
		pr_err("second: cdev_add failed, ret=%d\n", ret);
		goto fail_cdev;
	}

	/* 步骤 4：创建设备类（使 udev 能自动创建 /dev 节点） */
	seconds_class = class_create(THIS_MODULE, "seconds_class");
	if (IS_ERR(seconds_class)) {
		ret = PTR_ERR(seconds_class);
		pr_err("second: class_create failed, ret=%d\n", ret);
		goto fail_class;
	}

	/* 步骤 5：创建设备节点 /dev/second */
	device_create(seconds_class, NULL, devno, NULL, "second");
	pr_info("second: module loaded, device /dev/second created\n");
	return 0;

	/* 错误处理：按逆序释放已分配的资源 */
fail_class:
	cdev_del(&second_devp->cdev);
fail_cdev:
	kfree(second_devp);
fail_malloc:
	unregister_chrdev_region(devno, 1);
	return ret;
}

/*
 * second_exit — 模块卸载
 *
 * 【资源释放顺序】（与 init 相反）
 *   1. device_destroy       — 删除 /dev/second 节点
 *   2. class_destroy        — 删除设备类（必须在 device_destroy 之后）
 *   3. cdev_del             — 从内核注销字符设备
 *   4. kfree                — 释放设备私有数据
 *   5. unregister_chrdev_region — 释放设备号
 *
 * 注意：定时器在 second_release() 中已被停止。
 * 如果模块被强制卸载（rmmod -f）而设备未关闭，
 * 需要在此处调用 del_timer_sync() 防止定时器回调访问已释放的内存。
 */
static void __exit second_exit(void)
{
	device_destroy(seconds_class, devno);	/* 先删设备节点 */
	class_destroy(seconds_class);		/* 再删设备类 */
	cdev_del(&second_devp->cdev);		/* 注销字符设备 */
	kfree(second_devp);			/* 释放私有数据 */
	unregister_chrdev_region(devno, 1);	/* 释放设备号 */
	pr_info("second: module unloaded\n");
}

module_init(second_init);
module_exit(second_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Kernel timer demo: counts elapsed seconds via /dev/second");
