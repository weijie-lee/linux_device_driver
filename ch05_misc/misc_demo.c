/*
 * misc_demo.c — Ch05: Misc 杂项设备驱动
 *
 * 【知识点】
 * Misc（杂项）设备是字符设备的简化变体，所有 misc 设备共享主设备号 10，
 * 只需调用 misc_register() 即可完成注册，无需手动分配设备号、创建 class
 * 和 device，是实现简单字符设备的最便捷方式。
 *
 * 【与普通字符设备的对比】
 *   普通字符设备：alloc_chrdev_region → cdev_init → cdev_add →
 *                 class_create → device_create（5步）
 *   Misc 设备：   misc_register（1步）
 *
 * 【适用场景】
 * - 简单的控制接口（如 /dev/rtc0、/dev/watchdog）
 * - 不需要多个实例的设备
 * - 快速原型开发
 *
 * 【验证方法】
 * sudo insmod misc_demo.ko
 * cat /proc/misc | grep misc_demo     # 查看注册的 misc 设备
 * echo "hello" > /dev/misc_demo       # 写入数据
 * cat /dev/misc_demo                  # 读取数据
 * sudo rmmod misc_demo
 */

#include <linux/module.h>
#include <linux/miscdevice.h>   /* misc_register, misc_deregister, struct miscdevice */
#include <linux/fs.h>           /* file_operations */
#include <linux/uaccess.h>      /* copy_to_user, copy_from_user */
#include <linux/slab.h>         /* kmalloc, kfree */
#include <linux/mutex.h>        /* mutex */

#define MISC_DEMO_NAME  "misc_demo"
#define BUF_SIZE        256

/* ============================================================
 * 设备私有数据
 * ============================================================ */
struct misc_demo_priv {
	char    buf[BUF_SIZE];  /* 内部缓冲区 */
	size_t  data_len;       /* 当前数据长度 */
	struct mutex lock;      /* 保护 buf 和 data_len */
};

/* 静态分配私有数据（misc 设备通常只有一个实例） */
static struct misc_demo_priv misc_priv;

/* ============================================================
 * 文件操作实现
 * ============================================================ */

static int misc_demo_open(struct inode *inode, struct file *filp)
{
	/* misc 设备的私有数据通过 filp->private_data 传递
	 * 也可以通过 container_of(filp->f_inode->i_cdev, ...) 获取，
	 * 但 misc 设备更简单：直接使用全局静态变量即可 */
	filp->private_data = &misc_priv;
	pr_info("%s: opened by pid=%d\n", MISC_DEMO_NAME, current->pid);
	return 0;
}

static int misc_demo_release(struct inode *inode, struct file *filp)
{
	pr_info("%s: closed by pid=%d\n", MISC_DEMO_NAME, current->pid);
	return 0;
}

/*
 * misc_demo_read — 将内部缓冲区数据返回给用户空间
 *
 * 使用 ppos 实现简单的顺序读取语义：
 * - 第一次 read()：返回所有数据
 * - 第二次 read()：返回 0（EOF）
 */
static ssize_t misc_demo_read(struct file *filp, char __user *ubuf,
			      size_t count, loff_t *ppos)
{
	struct misc_demo_priv *priv = filp->private_data;
	ssize_t ret;
	size_t avail;

	if (mutex_lock_interruptible(&priv->lock))
		return -ERESTARTSYS;

	/* 计算从当前位置可读的字节数 */
	avail = (priv->data_len > *ppos) ? (priv->data_len - *ppos) : 0;
	if (avail == 0) {
		ret = 0;  /* EOF */
		goto out;
	}

	count = min(count, avail);
	if (copy_to_user(ubuf, priv->buf + *ppos, count)) {
		ret = -EFAULT;
		goto out;
	}

	*ppos += count;
	ret = count;

out:
	mutex_unlock(&priv->lock);
	return ret;
}

/*
 * misc_demo_write — 接收用户空间数据写入内部缓冲区
 */
static ssize_t misc_demo_write(struct file *filp, const char __user *ubuf,
			       size_t count, loff_t *ppos)
{
	struct misc_demo_priv *priv = filp->private_data;
	ssize_t ret;

	if (mutex_lock_interruptible(&priv->lock))
		return -ERESTARTSYS;

	/* 限制写入大小，防止溢出 */
	count = min(count, (size_t)(BUF_SIZE - 1));
	if (copy_from_user(priv->buf, ubuf, count)) {
		ret = -EFAULT;
		goto out;
	}

	priv->buf[count] = '\0';  /* 确保字符串以 NUL 结尾 */
	priv->data_len = count;
	*ppos = 0;  /* 重置读指针，允许重新读取 */
	ret = count;

	pr_info("%s: wrote %zu bytes: [%.*s]\n", MISC_DEMO_NAME,
		count, (int)count, priv->buf);

out:
	mutex_unlock(&priv->lock);
	return ret;
}

/*
 * misc_demo_ioctl — 控制命令
 *
 * 命令定义使用 _IO/_IOR/_IOW 宏，参数：
 *   type（幻数）：'M'
 *   nr（命令编号）：0, 1, ...
 *   size（数据大小）：sizeof(int) 等
 */
#define MISC_MAGIC      'M'
#define MISC_CLEAR      _IO(MISC_MAGIC, 0)          /* 清空缓冲区 */
#define MISC_GET_LEN    _IOR(MISC_MAGIC, 1, int)    /* 获取数据长度 */

static long misc_demo_ioctl(struct file *filp, unsigned int cmd,
			    unsigned long arg)
{
	struct misc_demo_priv *priv = filp->private_data;
	int len;

	switch (cmd) {
	case MISC_CLEAR:
		mutex_lock(&priv->lock);
		memset(priv->buf, 0, BUF_SIZE);
		priv->data_len = 0;
		mutex_unlock(&priv->lock);
		pr_info("%s: buffer cleared\n", MISC_DEMO_NAME);
		return 0;

	case MISC_GET_LEN:
		len = (int)priv->data_len;
		if (put_user(len, (int __user *)arg))
			return -EFAULT;
		return 0;

	default:
		return -ENOTTY;
	}
}

/* ============================================================
 * 文件操作表与 miscdevice 注册结构体
 * ============================================================ */
static const struct file_operations misc_demo_fops = {
	.owner          = THIS_MODULE,
	.open           = misc_demo_open,
	.release        = misc_demo_release,
	.read           = misc_demo_read,
	.write          = misc_demo_write,
	.unlocked_ioctl = misc_demo_ioctl,
	.llseek         = default_llseek,  /* 使用内核默认的 llseek 实现 */
};

/*
 * struct miscdevice — misc 设备描述符
 *
 * .minor = MISC_DYNAMIC_MINOR 表示动态分配次设备号
 * .name  = 设备名，会在 /dev/ 下创建同名节点
 * .fops  = 文件操作表
 */
static struct miscdevice misc_demo_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = MISC_DEMO_NAME,
	.fops  = &misc_demo_fops,
};

/* ============================================================
 * 模块初始化与退出
 * ============================================================ */
static int __init misc_demo_init(void)
{
	int ret;

	/* 初始化私有数据 */
	mutex_init(&misc_priv.lock);
	misc_priv.data_len = 0;

	/*
	 * misc_register() 一步完成：
	 * 1. 分配次设备号
	 * 2. 注册字符设备
	 * 3. 创建 /dev/misc_demo 节点
	 * 等价于普通字符设备的 5 步操作
	 */
	ret = misc_register(&misc_demo_dev);
	if (ret) {
		pr_err("%s: misc_register failed: %d\n", MISC_DEMO_NAME, ret);
		return ret;
	}

	pr_info("%s: registered, minor=%d, /dev/%s created\n",
		MISC_DEMO_NAME, misc_demo_dev.minor, MISC_DEMO_NAME);
	return 0;
}

static void __exit misc_demo_exit(void)
{
	/* misc_deregister() 一步完成所有清理工作 */
	misc_deregister(&misc_demo_dev);
	pr_info("%s: unregistered\n", MISC_DEMO_NAME);
}

module_init(misc_demo_init);
module_exit(misc_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("weijie-lee");
MODULE_DESCRIPTION("Ch05: Misc device driver demo");
