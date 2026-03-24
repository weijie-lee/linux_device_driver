/*
 * input_demo.c — Ch07: Input 输入子系统驱动
 *
 * 【知识点】
 * Linux Input 子系统为所有输入设备（键盘、鼠标、触摸屏、游戏手柄等）
 * 提供统一的抽象层。驱动只需调用 input_report_*() 上报事件，
 * 用户空间通过 /dev/input/eventX 读取标准化的 struct input_event。
 *
 * 【事件类型】
 *   EV_KEY  — 按键事件（键盘、按钮）
 *   EV_REL  — 相对坐标（鼠标移动）
 *   EV_ABS  — 绝对坐标（触摸屏）
 *   EV_SYN  — 同步事件（标记一组事件的结束）
 *
 * 【本示例的模拟方案】
 * 注册一个虚拟键盘设备，通过 /proc/virt_kbd 接口接收命令，
 * 模拟按键事件上报。支持的命令：
 *   echo "key 28" > /proc/virt_kbd   # 模拟按下 Enter 键（keycode 28）
 *   echo "key 1"  > /proc/virt_kbd   # 模拟按下 ESC 键（keycode 1）
 *
 * 【验证方法】
 * sudo insmod input_demo.ko
 * cat /proc/bus/input/devices | grep virt_kbd   # 查看注册的输入设备
 * sudo evtest /dev/input/eventX &               # 监听事件（X 为设备编号）
 * echo "key 28" > /proc/virt_kbd                # 触发 Enter 键事件
 * sudo rmmod input_demo
 */

#include <linux/module.h>
#include <linux/input.h>        /* input_dev, input_report_key, input_sync */
#include <linux/proc_fs.h>      /* proc_create, proc_remove */
#include <linux/uaccess.h>
#include <linux/slab.h>

#define DRIVER_NAME     "virt_kbd"
#define PROC_NAME       "virt_kbd"

/* ============================================================
 * 全局变量
 * ============================================================ */
static struct input_dev *virt_kbd_dev;  /* 虚拟键盘 input_dev */
static struct proc_dir_entry *proc_entry; /* /proc/virt_kbd 控制接口 */

/* ============================================================
 * /proc/virt_kbd 写入处理：接收命令并上报按键事件
 * ============================================================ */
static ssize_t virt_kbd_proc_write(struct file *filp,
				   const char __user *ubuf,
				   size_t count, loff_t *ppos)
{
	char kbuf[32];
	unsigned int keycode;
	int ret;

	if (count >= sizeof(kbuf))
		return -EINVAL;

	if (copy_from_user(kbuf, ubuf, count))
		return -EFAULT;

	kbuf[count] = '\0';

	/* 解析命令格式："key <keycode>" */
	ret = sscanf(kbuf, "key %u", &keycode);
	if (ret != 1) {
		pr_err("%s: invalid command: %s\n", DRIVER_NAME, kbuf);
		pr_err("%s: usage: echo 'key <keycode>' > /proc/%s\n",
		       DRIVER_NAME, PROC_NAME);
		return -EINVAL;
	}

	if (keycode > KEY_MAX) {
		pr_err("%s: keycode %u out of range (max %d)\n",
		       DRIVER_NAME, keycode, KEY_MAX);
		return -EINVAL;
	}

	/*
	 * 上报按键事件的标准流程：
	 * 1. input_report_key(dev, keycode, 1) — 按下
	 * 2. input_sync(dev)                   — 同步（标记事件组结束）
	 * 3. input_report_key(dev, keycode, 0) — 释放
	 * 4. input_sync(dev)                   — 同步
	 */
	input_report_key(virt_kbd_dev, keycode, 1);  /* 按下 */
	input_sync(virt_kbd_dev);
	input_report_key(virt_kbd_dev, keycode, 0);  /* 释放 */
	input_sync(virt_kbd_dev);

	pr_info("%s: reported key %u (press+release)\n", DRIVER_NAME, keycode);
	return count;
}

static ssize_t virt_kbd_proc_read(struct file *filp, char __user *ubuf,
				  size_t count, loff_t *ppos)
{
	const char *help = "Usage: echo 'key <keycode>' > /proc/virt_kbd\n"
			   "Common keycodes:\n"
			   "  1=ESC  2-11=1-0  28=Enter  57=Space\n"
			   "  30=A  48=B  46=C  32=D  18=E  33=F\n";
	size_t len = strlen(help);

	if (*ppos >= len)
		return 0;
	if (count > len - *ppos)
		count = len - *ppos;
	if (copy_to_user(ubuf, help + *ppos, count))
		return -EFAULT;
	*ppos += count;
	return count;
}

static const struct proc_ops virt_kbd_proc_ops = {
	.proc_read  = virt_kbd_proc_read,
	.proc_write = virt_kbd_proc_write,
};

/* ============================================================
 * 模块初始化与退出
 * ============================================================ */
static int __init input_demo_init(void)
{
	int ret, i;

	/*
	 * 步骤1：分配 input_dev
	 * input_allocate_device() 分配并初始化一个 input_dev 结构体
	 */
	virt_kbd_dev = input_allocate_device();
	if (!virt_kbd_dev) {
		pr_err("%s: input_allocate_device failed\n", DRIVER_NAME);
		return -ENOMEM;
	}

	/* 步骤2：设置设备信息（显示在 /proc/bus/input/devices） */
	virt_kbd_dev->name       = "Virtual Keyboard";
	virt_kbd_dev->phys       = "virt/input0";
	virt_kbd_dev->id.bustype = BUS_VIRTUAL;
	virt_kbd_dev->id.vendor  = 0x0001;
	virt_kbd_dev->id.product = 0x0001;
	virt_kbd_dev->id.version = 0x0100;

	/*
	 * 步骤3：声明设备支持的事件类型和按键
	 *
	 * set_bit(EV_KEY, dev->evbit)：声明支持按键事件
	 * set_bit(KEY_A, dev->keybit)：声明支持 A 键
	 *
	 * 这里声明支持所有标准键盘按键（KEY_0 到 KEY_MAX）
	 */
	set_bit(EV_KEY, virt_kbd_dev->evbit);
	set_bit(EV_SYN, virt_kbd_dev->evbit);

	/* 声明支持所有按键（简化处理，真实驱动只声明实际存在的按键） */
	for (i = KEY_ESC; i < KEY_MAX; i++)
		set_bit(i, virt_kbd_dev->keybit);

	/*
	 * 步骤4：注册 input_dev
	 * 注册后，/dev/input/eventX 节点由内核自动创建
	 */
	ret = input_register_device(virt_kbd_dev);
	if (ret) {
		pr_err("%s: input_register_device failed: %d\n", DRIVER_NAME, ret);
		input_free_device(virt_kbd_dev);
		return ret;
	}

	/* 步骤5：创建 /proc/virt_kbd 控制接口 */
	proc_entry = proc_create(PROC_NAME, 0666, NULL, &virt_kbd_proc_ops);
	if (!proc_entry) {
		pr_err("%s: proc_create failed\n", DRIVER_NAME);
		input_unregister_device(virt_kbd_dev);
		return -ENOMEM;
	}

	pr_info("%s: registered, use /proc/%s to inject key events\n",
		DRIVER_NAME, PROC_NAME);
	return 0;
}

static void __exit input_demo_exit(void)
{
	proc_remove(proc_entry);
	input_unregister_device(virt_kbd_dev);
	pr_info("%s: unregistered\n", DRIVER_NAME);
}

module_init(input_demo_init);
module_exit(input_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("weijie-lee");
MODULE_DESCRIPTION("Ch07: Input subsystem driver demo - virtual keyboard");
