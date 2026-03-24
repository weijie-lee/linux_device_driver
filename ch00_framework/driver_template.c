/*
 * driver_template.c — Linux 驱动开发通用模板
 *
 * 本文件展示了一个完整的字符设备驱动的标准框架，
 * 包含所有关键知识点的注释说明，可作为新驱动开发的起点。
 *
 * 【使用方法】
 * 1. 将 "mydev" 替换为你的设备名
 * 2. 将 "MY_DEV" 替换为你的设备名大写
 * 3. 根据实际硬件实现各操作函数
 * 4. 添加设备树匹配表（如果是平台设备）
 */

#include <linux/module.h>       /* module_init/exit, MODULE_* 宏 */
#include <linux/fs.h>           /* file_operations, alloc_chrdev_region */
#include <linux/cdev.h>         /* struct cdev, cdev_init, cdev_add */
#include <linux/device.h>       /* class_create, device_create */
#include <linux/slab.h>         /* kmalloc, kzalloc, kfree */
#include <linux/uaccess.h>      /* copy_to_user, copy_from_user */
#include <linux/mutex.h>        /* struct mutex, mutex_lock, mutex_unlock */
#include <linux/wait.h>         /* wait_queue_head_t, wait_event_interruptible */
#include <linux/poll.h>         /* poll_table, POLLIN, POLLOUT */

/* ============================================================
 * 第一部分：宏定义与常量
 * ============================================================ */

#define MYDEV_NAME      "mydev"         /* 设备名，出现在 /dev/ 和 /proc/devices */
#define MYDEV_CLASS     "mydev_class"   /* 设备类名，出现在 /sys/class/ */
#define MYDEV_BUF_SIZE  4096            /* 设备内部缓冲区大小 */
#define MYDEV_MINOR_CNT 1               /* 次设备号数量（支持多个设备实例时增大） */

/* ============================================================
 * 第二部分：设备私有数据结构体
 *
 * 设计原则：
 * - struct cdev 必须作为结构体成员（不能是指针），以便 container_of 使用
 * - 将所有设备相关数据集中在此结构体中，通过 filp->private_data 传递
 * - 使用互斥锁保护共享数据，使用等待队列实现阻塞 I/O
 * ============================================================ */
struct mydev_priv {
	struct cdev     cdev;           /* 内核字符设备，必须是第一个成员或通过 container_of 访问 */
	struct mutex    lock;           /* 互斥锁：保护 buf、r_pos、w_pos */
	wait_queue_head_t r_wait;       /* 读等待队列：缓冲区为空时阻塞读者 */
	wait_queue_head_t w_wait;       /* 写等待队列：缓冲区满时阻塞写者 */
	char            buf[MYDEV_BUF_SIZE]; /* 设备内部缓冲区 */
	unsigned int    r_pos;          /* 读指针 */
	unsigned int    w_pos;          /* 写指针 */
	unsigned int    data_len;       /* 当前缓冲区中的数据长度 */
};

/* ============================================================
 * 第三部分：全局变量
 * ============================================================ */
static dev_t            mydev_devno;    /* 设备号（主设备号 + 次设备号） */
static struct class    *mydev_class;    /* 设备类指针 */
static struct mydev_priv *mydev_priv;   /* 设备私有数据指针 */

/* ============================================================
 * 第四部分：文件操作函数实现
 * ============================================================ */

/*
 * mydev_open — 打开设备
 *
 * 通过 container_of 从 inode->i_cdev 获取设备私有数据，
 * 保存到 filp->private_data 供后续 read/write 使用。
 */
static int mydev_open(struct inode *inode, struct file *filp)
{
	struct mydev_priv *priv = container_of(inode->i_cdev,
					       struct mydev_priv, cdev);
	filp->private_data = priv;

	pr_info("%s: opened (pid=%d)\n", MYDEV_NAME, current->pid);
	return 0;
}

/*
 * mydev_release — 关闭设备
 */
static int mydev_release(struct inode *inode, struct file *filp)
{
	pr_info("%s: closed (pid=%d)\n", MYDEV_NAME, current->pid);
	return 0;
}

/*
 * mydev_read — 读取设备数据
 *
 * 【阻塞 I/O 实现模式】
 * 1. 获取互斥锁
 * 2. 检查条件（缓冲区是否有数据）
 * 3. 如果条件不满足，释放锁并进入等待队列
 * 4. 被唤醒后重新获取锁并检查条件（防止虚假唤醒）
 * 5. 执行实际操作
 * 6. 释放锁并唤醒等待写入的进程
 */
static ssize_t mydev_read(struct file *filp, char __user *buf,
			  size_t count, loff_t *ppos)
{
	struct mydev_priv *priv = filp->private_data;
	ssize_t ret;

	/* 非阻塞模式：缓冲区为空时立即返回 EAGAIN */
	if (filp->f_flags & O_NONBLOCK) {
		if (priv->data_len == 0)
			return -EAGAIN;
	}

	/* 阻塞等待：直到缓冲区有数据或被信号中断 */
	if (wait_event_interruptible(priv->r_wait, priv->data_len > 0))
		return -ERESTARTSYS;

	/* 获取互斥锁后再次检查（防止虚假唤醒） */
	if (mutex_lock_interruptible(&priv->lock))
		return -ERESTARTSYS;

	if (priv->data_len == 0) {
		mutex_unlock(&priv->lock);
		return 0;
	}

	/* 实际读取：从缓冲区拷贝数据到用户空间 */
	count = min(count, (size_t)priv->data_len);
	if (copy_to_user(buf, priv->buf + priv->r_pos, count)) {
		ret = -EFAULT;
		goto out;
	}

	priv->r_pos = (priv->r_pos + count) % MYDEV_BUF_SIZE;
	priv->data_len -= count;
	ret = count;

out:
	mutex_unlock(&priv->lock);

	/* 唤醒等待写入的进程（缓冲区现在有空间了） */
	wake_up_interruptible(&priv->w_wait);

	return ret;
}

/*
 * mydev_write — 写入数据到设备
 */
static ssize_t mydev_write(struct file *filp, const char __user *buf,
			   size_t count, loff_t *ppos)
{
	struct mydev_priv *priv = filp->private_data;
	ssize_t ret;
	size_t free_space;

	/* 阻塞等待：直到缓冲区有空间 */
	if (wait_event_interruptible(priv->w_wait,
				     priv->data_len < MYDEV_BUF_SIZE))
		return -ERESTARTSYS;

	if (mutex_lock_interruptible(&priv->lock))
		return -ERESTARTSYS;

	free_space = MYDEV_BUF_SIZE - priv->data_len;
	if (free_space == 0) {
		ret = -EAGAIN;
		goto out;
	}

	count = min(count, free_space);
	if (copy_from_user(priv->buf + priv->w_pos, buf, count)) {
		ret = -EFAULT;
		goto out;
	}

	priv->w_pos = (priv->w_pos + count) % MYDEV_BUF_SIZE;
	priv->data_len += count;
	ret = count;

out:
	mutex_unlock(&priv->lock);

	/* 唤醒等待读取的进程（缓冲区现在有数据了） */
	wake_up_interruptible(&priv->r_wait);

	return ret;
}

/*
 * mydev_poll — 支持 select/poll/epoll
 *
 * 返回当前可进行的操作掩码：
 *   POLLIN | POLLRDNORM  — 可读（缓冲区有数据）
 *   POLLOUT | POLLWRNORM — 可写（缓冲区有空间）
 */
static __poll_t mydev_poll(struct file *filp, poll_table *wait)
{
	struct mydev_priv *priv = filp->private_data;
	__poll_t mask = 0;

	/* 将等待队列注册到 poll_table，内核会在条件变化时通知 */
	poll_wait(filp, &priv->r_wait, wait);
	poll_wait(filp, &priv->w_wait, wait);

	if (priv->data_len > 0)
		mask |= POLLIN | POLLRDNORM;    /* 可读 */
	if (priv->data_len < MYDEV_BUF_SIZE)
		mask |= POLLOUT | POLLWRNORM;   /* 可写 */

	return mask;
}

/*
 * mydev_ioctl — 设备控制命令
 *
 * ioctl 命令编码规范（使用 _IO/_IOR/_IOW/_IOWR 宏）：
 *   _IO(type, nr)           — 无数据传输
 *   _IOR(type, nr, size)    — 从驱动读取数据
 *   _IOW(type, nr, size)    — 向驱动写入数据
 *   _IOWR(type, nr, size)   — 双向数据传输
 */
#define MYDEV_MAGIC     'M'
#define MYDEV_RESET     _IO(MYDEV_MAGIC, 0)     /* 复位设备 */
#define MYDEV_GET_LEN   _IOR(MYDEV_MAGIC, 1, int) /* 获取缓冲区数据长度 */

static long mydev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct mydev_priv *priv = filp->private_data;
	int len;

	switch (cmd) {
	case MYDEV_RESET:
		mutex_lock(&priv->lock);
		priv->r_pos = priv->w_pos = priv->data_len = 0;
		mutex_unlock(&priv->lock);
		pr_info("%s: device reset\n", MYDEV_NAME);
		return 0;

	case MYDEV_GET_LEN:
		len = priv->data_len;
		if (put_user(len, (int __user *)arg))
			return -EFAULT;
		return 0;

	default:
		return -ENOTTY;  /* 不支持的命令 */
	}
}

/* ============================================================
 * 第五部分：文件操作表
 * ============================================================ */
static const struct file_operations mydev_fops = {
	.owner          = THIS_MODULE,
	.open           = mydev_open,
	.release        = mydev_release,
	.read           = mydev_read,
	.write          = mydev_write,
	.poll           = mydev_poll,
	.unlocked_ioctl = mydev_ioctl,
};

/* ============================================================
 * 第六部分：模块初始化与退出
 *
 * 资源分配顺序（退出时严格逆序释放）：
 *   1. alloc_chrdev_region
 *   2. kzalloc
 *   3. cdev_init + cdev_add
 *   4. class_create
 *   5. device_create
 * ============================================================ */
static int __init mydev_init(void)
{
	int ret;

	/* 步骤 1：动态分配设备号 */
	ret = alloc_chrdev_region(&mydev_devno, 0, MYDEV_MINOR_CNT, MYDEV_NAME);
	if (ret < 0) {
		pr_err("%s: alloc_chrdev_region failed: %d\n", MYDEV_NAME, ret);
		return ret;
	}
	pr_info("%s: major=%d, minor=%d\n", MYDEV_NAME,
		MAJOR(mydev_devno), MINOR(mydev_devno));

	/* 步骤 2：分配并初始化设备私有数据 */
	mydev_priv = kzalloc(sizeof(*mydev_priv), GFP_KERNEL);
	if (!mydev_priv) {
		ret = -ENOMEM;
		goto fail_alloc;
	}

	/* 初始化同步原语 */
	mutex_init(&mydev_priv->lock);
	init_waitqueue_head(&mydev_priv->r_wait);
	init_waitqueue_head(&mydev_priv->w_wait);

	/* 步骤 3：初始化并注册字符设备 */
	cdev_init(&mydev_priv->cdev, &mydev_fops);
	mydev_priv->cdev.owner = THIS_MODULE;
	ret = cdev_add(&mydev_priv->cdev, mydev_devno, MYDEV_MINOR_CNT);
	if (ret) {
		pr_err("%s: cdev_add failed: %d\n", MYDEV_NAME, ret);
		goto fail_cdev;
	}

	/* 步骤 4：创建设备类（使 udev 能自动创建 /dev 节点） */
	mydev_class = class_create(THIS_MODULE, MYDEV_CLASS);
	if (IS_ERR(mydev_class)) {
		ret = PTR_ERR(mydev_class);
		pr_err("%s: class_create failed: %d\n", MYDEV_NAME, ret);
		goto fail_class;
	}

	/* 步骤 5：创建设备节点 /dev/mydev */
	if (!device_create(mydev_class, NULL, mydev_devno, NULL, MYDEV_NAME)) {
		ret = -ENOMEM;
		goto fail_device;
	}

	pr_info("%s: module loaded, /dev/%s created\n", MYDEV_NAME, MYDEV_NAME);
	return 0;

	/* 错误处理：按逆序释放已分配的资源 */
fail_device:
	class_destroy(mydev_class);
fail_class:
	cdev_del(&mydev_priv->cdev);
fail_cdev:
	kfree(mydev_priv);
fail_alloc:
	unregister_chrdev_region(mydev_devno, MYDEV_MINOR_CNT);
	return ret;
}

static void __exit mydev_exit(void)
{
	/* 退出顺序严格与初始化相反 */
	device_destroy(mydev_class, mydev_devno);   /* 先删设备节点 */
	class_destroy(mydev_class);                  /* 再删设备类 */
	cdev_del(&mydev_priv->cdev);                 /* 注销字符设备 */
	kfree(mydev_priv);                           /* 释放私有数据 */
	unregister_chrdev_region(mydev_devno, MYDEV_MINOR_CNT); /* 释放设备号 */

	pr_info("%s: module unloaded\n", MYDEV_NAME);
}

module_init(mydev_init);
module_exit(mydev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Linux driver development template with char device framework");
