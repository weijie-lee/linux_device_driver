/*
 * kernel kfifo demo
 *
 * (C) 2020.03.28 liweijie<ee.liweijie@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kfifo.h>
#include <linux/proc_fs.h>

/* name of the proc entry */
#define PROC_FIFO "bytestream-fifo"

/* lock for procfs write access */
static DEFINE_MUTEX(read_lock);

/* lock for procfs write access */
static DEFINE_MUTEX(write_lock);

#define FIFO_SIZE 32
#define DYNAMIC
#ifdef DYNAMIC
static struct kfifo test;
#else
static DECLARE_KFIFO(test, unsigned char, FIFO_SIZE);
#endif

int test_func(void)
{
	char i;
	int ret;
	unsigned char buf[6];

	printk(KERN_INFO "fifo test begin\n");	
	kfifo_in(&test, "hello", 5);
	printk(KERN_INFO "fifo len:%d\n", kfifo_len(&test));
	
	/* put values ito the fifo */
	for (i = 0; i < 10; i++) {
		kfifo_put(&test, i);
	}
	/* show the number of used elements */
	printk(KERN_INFO "fifo len:%d\n", kfifo_len(&test));

	/* get max of 5 byte from the fifo */	
	i = kfifo_out(&test, buf, 5);
	printk(KERN_INFO "buf:%.*s\n",i, buf);
	printk(KERN_INFO "fifo len:%d\n", kfifo_len(&test));

	ret = kfifo_out(&test, buf, 2);
	printk(KERN_INFO "ret:%d\n", ret);

	ret = kfifo_in(&test, buf, ret);
	printk(KERN_INFO "ret:%d\n", ret);

	printk(KERN_INFO "skip 1st element\n");
	kfifo_skip(&test);

	for (i = 20; kfifo_put(&test, i); i++);
	printk(KERN_INFO "queue len %d\n", kfifo_len(&test));
	
	if (kfifo_peek(&test, &i))
		printk(KERN_INFO "kfifo peek: %d\n", i);

	while (kfifo_get(&test, &i))
		printk(KERN_INFO "item = %d\n", i);
	
	printk(KERN_INFO "fifo test end\n");
	return 0;
}

static ssize_t fifo_read(struct file *file, char __user *buf,
			size_t count, loff_t *ppos)
{
	int ret;
	unsigned int copied;
	if (mutex_lock_interruptible(&read_lock)) {
		return -ERESTARTSYS;
	}
	ret = kfifo_to_user(&test, buf, count, &copied);
	mutex_unlock(&read_lock);
	return ret ? ret : copied;
}

static ssize_t fifo_write(struct file *file, const char __user *buf,
			size_t count, loff_t *ppos)
{
	int ret;
	unsigned int copied;

	if (mutex_lock_interruptible(&write_lock)) {
		return -ERESTARTSYS;
	}
	ret = kfifo_from_user(&test, buf, count, &copied);
	mutex_unlock(&write_lock);
	return ret ? ret : copied;
}

/*
 * Linux 5.6+ 要求 /proc 文件使用 struct proc_ops 而非 struct file_operations。
 * proc_ops 专为 procfs 设计，减少了不必要的函数指针，降低内存占用。
 */
static const struct proc_ops fifo_fops = {
	.proc_read  = fifo_read,
	.proc_write = fifo_write,
	.proc_lseek = noop_llseek,
};

static int __init mod_init(void)
{
#ifdef DYNAMIC
        int ret;

        ret = kfifo_alloc(&test, FIFO_SIZE, GFP_KERNEL);
        if (ret < 0) {
                printk(KERN_INFO "error kfifo_alloc\n");
                return ret;
        }
#else
	INIT_KFIFO(test);
#endif
	if (test_func() < 0)
		return -EIO;
	/* proc_create 第4参数在 5.6+ 内核需传入 struct proc_ops * */
	if (proc_create(PROC_FIFO, 0, NULL, &fifo_fops) == NULL) {
#ifdef DYNAMIC
                kfifo_free(&test);
#endif
		return -ENOMEM;
	}
	return 0;
}
static void __exit mod_exit(void)
{
	remove_proc_entry(PROC_FIFO, NULL);
#ifdef DYNAMIC
        kfifo_free(&test);
#endif
	return;
}

module_init(mod_init);
module_exit(mod_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie<ee.liweijie@gmail.com>");

