/*
 * globalmem.c — 字符设备驱动完整示例（含 mutex 并发控制）
 *
 * 【驱动架构概述】
 * 本驱动实现了一个具有 4096 字节内存缓冲区的虚拟字符设备。
 * 系统同时创建 DEVICE_NUM(4) 个设备实例，每个实例独立拥有自己的
 * 内存缓冲区和 mutex 锁。
 *
 * 【内核字符设备注册流程】
 *   1. alloc_chrdev_region()  — 动态申请主设备号 + 连续次设备号范围
 *   2. kzalloc()              — 为每个设备实例分配内核内存
 *   3. cdev_init()            — 将 file_operations 绑定到 cdev 结构体
 *   4. cdev_add()             — 将 cdev 注册到内核，设备开始对用户可见
 *   5. class_create()         — 在 /sys/class/ 下创建设备类别（供 udev 识别）
 *   6. device_create()        — 在 /dev/ 下自动创建设备文件
 *
 * 【拆卸顺序（必须严格逆序）】
 *   device_destroy() → class_destroy() → cdev_del() → kfree() → unregister_chrdev_region()
 *   这个顺序不能额倒！先销毁 class 会导致 device_destroy 访问悬空指针。
 *
 * 【并发控制】
 *   每个设备实例独立拥有 mutex。read/write/ioctl 均先加锁再访问内存。
 *   这防止了多进程并发读写导致的数据竞争。
 *
 * 【核心知识点】
 *   - container_of: open 中通过 inode->i_cdev 反推出业务结构体
 *   - copy_to_user/copy_from_user: 内核与用户空间安全拷贝（不能直接 memcpy）
 *   - __user: 标记用户空间指针，提醒编译器和静态分析工具加以检查
 *   - unlocked_ioctl: 现代内核不再使用 BKL，驱动自己负责加锁
 *
 * 【整改记录】
 *   - 替换 trace_printk() 为 pr_err()（trace_printk 是 ftrace 调试设施，不应出现在生产驱动）
 *   - 修复 init 中 alloc_chrdev_region 失败后缺少 return 的 Bug
 *   - 修复 exit 中 device_destroy 在 class_destroy 之后调用的悬空指针 Bug
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>           /* file_operations, alloc_chrdev_region */
#include <linux/cdev.h>         /* cdev_init, cdev_add, cdev_del */
#include <linux/slab.h>         /* kzalloc, kfree */
#include <linux/uaccess.h>      /* copy_to/from_user */
#include <linux/device.h>       /* class_create, device_create */
#include <linux/mutex.h>        /* mutex_lock, mutex_unlock */
#include <linux/errno.h>        /* EFAULT, EINVAL etc */

#define GLOBALMEM_SIZE	4096		/* 每个设备实例的内存缓冲区大小：4 KiB */
#define DEVICE_NUM	4		/* 同时创建的设备实例数量 */

/*
 * ioctl 命令定义：
 *   GLOBAL_MEM_MAGIC 是设备类型编号（魔数），用于局限 ioctl 命令空间，避免与其他驱动冲突。
 *   _IO(type, nr) 定义不传递数据的 ioctl 命令。
 *   MEM_CLEAR 命令将设备内存清零。
 */
#define GLOBAL_MEM_MAGIC	'g'
#define MEM_CLEAR	_IO(GLOBAL_MEM_MAGIC, 0)

static struct class *globalmem_class;
static const char *chr_dev_name[DEVICE_NUM] = {
	"global_mem_0", "global_mem_1", "global_mem_2", "global_mem_3"
};

/*
 * struct global_mem_dev — 字符设备的私有数据结构体
 *
 * 每个设备实例对应一个此结构体。
 *
 * 关键设计：
 *   cdev   — 内核字符设备结构体，嵌入到业务结构体中。
 *            open 时通过 container_of(inode->i_cdev, struct global_mem_dev, cdev)
 *            反推出整个结构体的地址，存入 filp->private_data。
 *   mem    — 内核空间的内存缓冲区，用户读写的数据存放在这里。
 *   mutex  — 保护 mem 的互斥锁，防止并发读写竞争。
 */
struct global_mem_dev {
	struct cdev cdev;		/* 内核字符设备结构体，嵌入到业务结构体 */
	unsigned char mem[GLOBALMEM_SIZE];	/* 设备内存缓冲区 */
	struct mutex mutex;		/* 保护 mem 的互斥锁 */
};

static struct global_mem_dev *global_mem_devp;
static dev_t devno;

static int global_mem_open(struct inode *inode, struct file *filp)
{
	/*
	 * 内核字符设备的标准 open 模式：
	 *
	 * inode->i_cdev 指向该设备对应的 struct cdev。
	 * 由于 cdev 是嵌入在 global_mem_dev 中的，
	 * 通过 container_of 可以从 cdev 地址反推出整个业务结构体的地址。
	 *
	 * 将其存入 filp->private_data 后，read/write/ioctl 等函数可直接
	 * 通过 filp->private_data 获取设备私有数据，无需再次查找。
	 */
	struct global_mem_dev *dev = container_of(inode->i_cdev,
						  struct global_mem_dev, cdev);
	filp->private_data = dev;	/* 存入私有数据指针，后续操作直接使用 */
	return 0;
}

static int global_mem_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t global_mem_read(struct file *filp, char __user *buf,
				size_t size, loff_t *ppos)
{
	unsigned long p = *ppos;	/* 当前文件位置（即读写偏移量） */
	unsigned int count = size;	/* 请求读取的字节数 */
	int ret = 0;
	struct global_mem_dev *dev = filp->private_data;	/* 获取设备私有数据 */

	/* 越界检查：如果当前位置已超出缓冲区大小，返回 0（EOF） */
	if (p >= GLOBALMEM_SIZE)
		return 0;

	/* 防止读取超出缓冲区末尾：如果请求量过大，就截断到缓冲区末尾 */
	if (count > GLOBALMEM_SIZE - p)
		count = GLOBALMEM_SIZE - p;

	/*
	 * 加锁保护共享资源。
	 * 必须在持锁期间完成 copy_to_user，防止并发写操作导致读到脂数据。
	 */
	mutex_lock(&dev->mutex);
	/*
	 * copy_to_user(dst_user, src_kernel, count)
	 * 将内核空间的 dev->mem 数据安全地拷贝到用户空间的 buf。
	 * 返回 0 表示成功，返回非 0 表示有多少字节未拷贝成功。
	 * 不能直接用 memcpy，因为用户空间指针可能无效或指向内核空间。
	 */
	if (copy_to_user(buf, dev->mem + p, count)) {
		ret = -EFAULT;	/* 用户空间地址无效 */
	} else {
		*ppos += count;	/* 更新文件位置指针 */
		ret = count;	/* 返回实际读取的字节数 */
		pr_info("globalmem: read %u byte(s) from offset %lu\n", count, p);
	}
	mutex_unlock(&dev->mutex);

	return ret;
}

static ssize_t global_mem_write(struct file *filp, const char __user *buf,
				size_t size, loff_t *ppos)
{
	unsigned long p = *ppos;	/* 当前写入偏移量 */
	unsigned int count = size;
	int ret = 0;
	struct global_mem_dev *dev = filp->private_data;

	/* 越界检查 */
	if (p >= GLOBALMEM_SIZE)
		return 0;

	/* 防止写入超出缓冲区末尾 */
	if (count > GLOBALMEM_SIZE - p)
		count = GLOBALMEM_SIZE - p;

	mutex_lock(&dev->mutex);
	/*
	 * copy_from_user(dst_kernel, src_user, count)
	 * 将用户空间的 buf 数据安全地拷贝到内核空间的 dev->mem。
	 * 返回 0 表示成功。
	 */
	if (copy_from_user(dev->mem + p, buf, count)) {
		ret = -EFAULT;
	} else {
		*ppos += count;
		ret = count;
		pr_info("globalmem: wrote %u byte(s) at offset %lu\n", count, p);
	}
	mutex_unlock(&dev->mutex);

	return ret;
}

/*
 * global_mem_llseek — 实现 lseek() 系统调用
 *
 * 内核默认的 generic_file_llseek 对普通文件有效，但字符设备的
 * 内存大小是固定的，需要自定义边界检查逻辑。
 *
 * 支持的 whence 参数：
 *   SEEK_SET (0) — 从文件头开始的绝对偏移
 *   SEEK_CUR (1) — 相对于当前位置的偏移
 *   SEEK_END (2) — 未实现，返回 -EINVAL
 */
static loff_t global_mem_llseek(struct file *filp, loff_t offset, int orig)
{
	loff_t ret;

	switch (orig) {
	case SEEK_SET:	/* 从文件头开始的绝对定位 */
		if (offset < 0 || (unsigned int)offset > GLOBALMEM_SIZE)
			return -EINVAL;	/* 越界拒绝 */
		filp->f_pos = (unsigned int)offset;
		ret = filp->f_pos;
		break;

	case SEEK_CUR:	/* 相对当前位置的相对定位 */
		if ((filp->f_pos + offset) > GLOBALMEM_SIZE ||
		    (filp->f_pos + offset) < 0)
			return -EINVAL;	/* 越界拒绝 */
		filp->f_pos += offset;
		ret = filp->f_pos;
		break;

	default:
		ret = -EINVAL;	/* SEEK_END 等未实现的 whence 返回错误 */
		break;
	}

	return ret;
}

/*
 * global_mem_ioctl — 实现 ioctl() 系统调用
 *
 * ioctl 用于实现无法用 read/write 表达的设备控制命令。
 *
 * 内核 ioctl 命令编号规范（使用 _IO/_IOR/_IOW/_IOWR 宏）：
 *   _IO(type, nr)         — 无数据传递
 *   _IOR(type, nr, size)  — 从驱动读数据
 *   _IOW(type, nr, size)  — 向驱动写数据
 *   _IOWR(type, nr, size) — 双向传递
 *
 * 本驱动只实现了 MEM_CLEAR 命令，将设备内存清零。
 */
static long global_mem_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long args)
{
	long ret = 0;
	struct global_mem_dev *dev = filp->private_data;

	switch (cmd) {
	case MEM_CLEAR:
		/* 加锁后清零设备内存，防止并发读操作读到部分清零的数据 */
		mutex_lock(&dev->mutex);
		memset(dev->mem, 0, GLOBALMEM_SIZE);
		mutex_unlock(&dev->mutex);
		pr_info("globalmem: device memory cleared to zero\n");
		break;
	default:
		ret = -EINVAL;	/* 未知命令返回 -EINVAL */
	}

	return ret;
}

/*
 * global_mem_fops — 字符设备操作函数表
 *
 * 这是内核与用户空间之间的接口层。用户调用 open/read/write/ioctl/lseek 时，
 * 内核就会调用这里对应的函数指针。
 *
 * .owner = THIS_MODULE：防止设备文件打开时模块被卸载。
 * .unlocked_ioctl：现代内核已去除 BKL，驱动自己负责内部加锁。
 */
static const struct file_operations global_mem_fops = {
	.owner		= THIS_MODULE,		/* 防止模块在设备打开时被卸载 */
	.open		= global_mem_open,	/* 获取设备私有数据，存入 private_data */
	.release	= global_mem_release,	/* 关闭时清理（本驱动无需清理） */
	.read		= global_mem_read,	/* 内核内存 → 用户空间 */
	.write		= global_mem_write,	/* 用户空间 → 内核内存 */
	.unlocked_ioctl	= global_mem_ioctl,	/* 设备控制命令 */
	.llseek		= global_mem_llseek,	/* 文件位置移动 */
};

static int __init global_mem_init(void)
{
	int ret;
	int i;

	/*
	 * 第一步：动态申请设备号
	 * alloc_chrdev_region(&devno, baseminor, count, name)
	 *   devno      — 输出参数，内核分配的主设备号+起始次设备号
	 *   baseminor  — 起始次设备号（从 0 开始）
	 *   count      — 申请的次设备号数量
	 *   name       — 设备名称（出现在 /proc/devices 中）
	 *
	 * 与静态分配（register_chrdev_region）相比，动态分配避免了设备号冲突。
	 */
	ret = alloc_chrdev_region(&devno, 0, DEVICE_NUM, "global_mem");
	if (ret < 0) {
		pr_err("globalmem: failed to allocate chrdev region, ret=%d\n", ret);
		return ret;	/* 失败时立即返回，不能用无效的 devno 继续执行 */
	}
	pr_info("globalmem: major=%d minor_base=%d\n", MAJOR(devno), MINOR(devno));

	/*
	 * 第二步：为所有设备实例分配内核内存
	 * kzalloc 分配并清零内存，避免未初始化的垃圾数据。
	 * GFP_KERNEL 表示允许内核在内存不足时休眠等待（可以睡眠的上下文）。
	 */
	global_mem_devp = kzalloc(sizeof(*global_mem_devp) * DEVICE_NUM, GFP_KERNEL);
	if (!global_mem_devp) {
		ret = -ENOMEM;
		goto fail_malloc;
	}

	/*
	 * 第三步：初始化并注册每个 cdev
	 * cdev_init 将 file_operations 绑定到 cdev。
	 * cdev_add  将 cdev 注册到内核，此后设备对用户可见。
	 * MKDEV(major, minor) 将主设备号和次设备号组合成 dev_t。
	 */
	for (i = 0; i < DEVICE_NUM; i++) {
		mutex_init(&(global_mem_devp + i)->mutex);	/* 初始化每个设备的独立 mutex */
		cdev_init(&(global_mem_devp + i)->cdev, &global_mem_fops);
		(global_mem_devp + i)->cdev.owner = THIS_MODULE;
		ret = cdev_add(&(global_mem_devp + i)->cdev, MKDEV(MAJOR(devno), i), 1);
		if (ret) {
			pr_err("globalmem: failed to add cdev %d, ret=%d\n", i, ret);
			goto fail_cdev;
		}
	}

	/*
	 * 第四步：创建设备类别和设备文件
	 * class_create  在 /sys/class/ 下创建设备类别目录，供 udev 识别。
	 * device_create 在 /dev/ 下自动创建设备文件（触发 udev 规则）。
	 */
	globalmem_class = class_create("global_mem_class");
	if (IS_ERR(globalmem_class)) {
		ret = PTR_ERR(globalmem_class);
		pr_err("globalmem: class_create failed, ret=%d\n", ret);
		goto fail_cdev;
	}

	for (i = 0; i < DEVICE_NUM; i++)
		device_create(globalmem_class, NULL, MKDEV(MAJOR(devno), i),
			      NULL, chr_dev_name[i]);

	return 0;

	/*
	 * 错误处理：使用 goto 实现逆序清理。
	 * 每个标签对应一个清理层次，确保已分配的资源全部释放。
	 */
fail_cdev:
	while (--i >= 0)
		cdev_del(&(global_mem_devp + i)->cdev);
	kfree(global_mem_devp);
fail_malloc:
	unregister_chrdev_region(devno, DEVICE_NUM);
	return ret;
}

static void __exit global_mem_exit(void)
{
	int i;

	/*
	 * 拆卸顺序必须严格逆序（与 init 相反）：
	 *
	 *   第一步：device_destroy()    — 删除 /dev/ 下的设备文件
	 *   第二步：class_destroy()     — 删除 /sys/class/ 下的类别目录
	 *   第三步：cdev_del()          — 从内核注销 cdev
	 *   第四步：kfree()             — 释放设备实例内存
	 *   第五步：unregister_chrdev_region() — 释放设备号
	 *
	 * 注意：必须先 device_destroy 再 class_destroy。
	 * 如果先 class_destroy，则 device_destroy 会访问已销毁的 class 指针，
	 * 导致内核 oops（悬空指针访问）。
	 */
	for (i = 0; i < DEVICE_NUM; i++)
		device_destroy(globalmem_class, MKDEV(MAJOR(devno), i));

	class_destroy(globalmem_class);

	for (i = 0; i < DEVICE_NUM; i++)
		cdev_del(&(global_mem_devp + i)->cdev);

	kfree(global_mem_devp);
	unregister_chrdev_region(devno, DEVICE_NUM);
	pr_info("globalmem: module unloaded\n");
}

module_init(global_mem_init);
module_exit(global_mem_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Simple character device driver with mutex concurrency control");
