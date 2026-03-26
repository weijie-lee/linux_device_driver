/*
 * kfifo_demo_static.c — Linux 内核 kfifo 环形缓冲区演示
 *
 * 【功能】
 * 本模块演示如何使用 Linux 内核提供的 kfifo 数据结构实现环形缓冲区。
 * kfifo 支持动态分配和静态声明两种方式，内部使用原子操作确保并发安全。
 *
 * 【核心知识点】
 * 1. kfifo 初始化：kfifo_alloc() 动态分配，DECLARE_KFIFO() 静态声明
 * 2. 基本操作：kfifo_put/get（单个元素），kfifo_in/out（批量数据）
 * 3. 用户空间接口：kfifo_to_user/from_user 实现 procfs 读写
 * 4. 并发安全：使用 mutex 保护 read/write 操作
 *
 * 【内存布局】
 * kfifo 内部维护 in（写指针）和 out（读指针）两个索引：
 *   - in == out：缓冲区为空
 *   - (in - out) == size：缓冲区满
 *   - 通过模运算实现环形：index = (index + 1) % size
 *
 * (C) 2020.03.28 liweijie<ee.liweijie@gmail.com>
 * GPL v2
 */

#include <linux/module.h>
#include <linux/kfifo.h>
#include <linux/proc_fs.h>

/* /proc 文件系统中的条目名称 */
#define PROC_FIFO "bytestream-fifo"

/* 保护 procfs 读操作的互斥锁，防止多个用户进程同时读取 kfifo */
static DEFINE_MUTEX(read_lock);

/* 保护 procfs 写操作的互斥锁，防止多个用户进程同时写入 kfifo */
static DEFINE_MUTEX(write_lock);

/* kfifo 缓冲区大小（字节数），必须是 2 的幂次方以便模运算优化 */
#define FIFO_SIZE 32

/* 编译时开关：定义 DYNAMIC 则使用动态分配，否则使用静态声明 */
#define DYNAMIC

#ifdef DYNAMIC
    /* 动态分配模式：运行时通过 kfifo_alloc() 分配内存 */
    static struct kfifo test;
#else
    /* 静态声明模式：编译时在 .data 段分配固定大小的缓冲区
     * DECLARE_KFIFO(name, type, size) 展开为：
     *   union { struct kfifo kfifo; unsigned char buf[size]; } name
     */
    static DECLARE_KFIFO(test, unsigned char, FIFO_SIZE);
#endif

/**
 * test_func() — kfifo 功能演示函数
 *
 * 演示 kfifo 的各种操作：
 *   1. 批量入队：kfifo_in() 一次入队多个字节
 *   2. 单个入队：kfifo_put() 逐个入队元素
 *   3. 批量出队：kfifo_out() 一次出队多个字节
 *   4. 单个出队：kfifo_get() 逐个出队元素
 *   5. 查看操作：kfifo_peek() 查看队头但不删除
 *   6. 跳过操作：kfifo_skip() 跳过一个元素
 *
 * 返回值：成功返回 0，失败返回负数
 */
int test_func(void)
{
    char i;
    int ret;
    unsigned char buf[6];

    printk(KERN_INFO "fifo test begin\n");

    /* 演示 1：批量入队 "hello" 字符串（5 字节） */
    kfifo_in(&test, "hello", 5);
    printk(KERN_INFO "fifo len:%d\n", kfifo_len(&test));  /* 预期输出：5 */

    /* 演示 2：单个入队 0-9 共 10 个字节 */
    for (i = 0; i < 10; i++) {
        kfifo_put(&test, i);
    }
    /* 此时 kfifo 中有 5 + 10 = 15 个字节 */
    printk(KERN_INFO "fifo len:%d\n", kfifo_len(&test));  /* 预期输出：15 */

    /* 演示 3：批量出队最多 5 字节，应该得到 "hello" */
    i = kfifo_out(&test, buf, 5);
    printk(KERN_INFO "buf:%.*s\n", i, buf);  /* 预期输出：hello */
    printk(KERN_INFO "fifo len:%d\n", kfifo_len(&test));  /* 预期输出：10 */

    /* 演示 4：出队 2 个字节（应该是 0 和 1） */
    ret = kfifo_out(&test, buf, 2);
    printk(KERN_INFO "ret:%d\n", ret);  /* 预期输出：2 */

    /* 演示 5：将刚出队的 2 个字节重新入队 */
    ret = kfifo_in(&test, buf, ret);
    printk(KERN_INFO "ret:%d\n", ret);  /* 预期输出：2 */

    /* 演示 6：跳过队头的一个元素（不出队，直接删除） */
    printk(KERN_INFO "skip 1st element\n");
    kfifo_skip(&test);

    /* 演示 7：入队 20-31 共 12 个字节，直到 kfifo 满 */
    for (i = 20; kfifo_put(&test, i); i++);
    printk(KERN_INFO "queue len %d\n", kfifo_len(&test));  /* 预期输出：32（满） */

    /* 演示 8：查看队头元素（不删除） */
    if (kfifo_peek(&test, &i))
        printk(KERN_INFO "kfifo peek: %d\n", i);  /* 预期输出：20 */

    /* 演示 9：逐个出队所有元素并打印 */
    while (kfifo_get(&test, &i))
        printk(KERN_INFO "item = %d\n", i);  /* 预期输出：20-31 */

    printk(KERN_INFO "fifo test end\n");
    return 0;
}

/**
 * fifo_read() — /proc/bytestream-fifo 读操作处理函数
 *
 * 当用户进程读取 /proc/bytestream-fifo 时调用此函数。
 * 功能：从 kfifo 中读取数据并拷贝到用户空间。
 *
 * 参数：
 *   @file：打开的文件结构体
 *   @buf：用户空间缓冲区指针
 *   @count：请求读取的字节数
 *   @ppos：文件读取位置（procfs 不使用）
 *
 * 返回值：成功返回读取的字节数，失败返回负数
 */
static ssize_t fifo_read(struct file *file, char __user *buf,
            size_t count, loff_t *ppos)
{
    int ret;
    unsigned int copied;

    /* 尝试获取互斥锁，如果被中断则返回 -ERESTARTSYS 让用户进程重试 */
    if (mutex_lock_interruptible(&read_lock)) {
        return -ERESTARTSYS;
    }

    /* kfifo_to_user() 从 kfifo 中读取数据并直接拷贝到用户空间
     * 参数：
     *   @test：源 kfifo
     *   @buf：目标用户空间缓冲区
     *   @count：最多读取的字节数
     *   @copied：输出参数，实际拷贝的字节数
     * 返回值：成功返回 0，失败返回错误码
     */
    ret = kfifo_to_user(&test, buf, count, &copied);
    mutex_unlock(&read_lock);

    /* 返回实际拷贝的字节数，或者错误码 */
    return ret ? ret : copied;
}

/**
 * fifo_write() — /proc/bytestream-fifo 写操作处理函数
 *
 * 当用户进程写入 /proc/bytestream-fifo 时调用此函数。
 * 功能：从用户空间读取数据并写入 kfifo。
 *
 * 参数：
 *   @file：打开的文件结构体
 *   @buf：用户空间缓冲区指针
 *   @count：请求写入的字节数
 *   @ppos：文件写入位置（procfs 不使用）
 *
 * 返回值：成功返回写入的字节数，失败返回负数
 */
static ssize_t fifo_write(struct file *file, const char __user *buf,
            size_t count, loff_t *ppos)
{
    int ret;
    unsigned int copied;

    /* 尝试获取互斥锁，如果被中断则返回 -ERESTARTSYS 让用户进程重试 */
    if (mutex_lock_interruptible(&write_lock)) {
        return -ERESTARTSYS;
    }

    /* kfifo_from_user() 从用户空间读取数据并直接写入 kfifo
     * 参数：
     *   @test：目标 kfifo
     *   @buf：源用户空间缓冲区
     *   @count：最多写入的字节数
     *   @copied：输出参数，实际拷贝的字节数
     * 返回值：成功返回 0，失败返回错误码
     */
    ret = kfifo_from_user(&test, buf, count, &copied);
    mutex_unlock(&write_lock);

    /* 返回实际写入的字节数，或者错误码 */
    return ret ? ret : copied;
}

/**
 * fifo_fops — /proc 文件操作结构体
 *
 * Linux 5.6+ 要求 /proc 文件使用 struct proc_ops 而非 struct file_operations。
 * proc_ops 专为 procfs 设计，只包含必要的函数指针，减少内存占用。
 *
 * 字段说明：
 *   .proc_read：处理 read() 系统调用
 *   .proc_write：处理 write() 系统调用
 *   .proc_lseek：处理 seek() 系统调用（noop_llseek 表示不支持 seek）
 */
static const struct proc_ops fifo_fops = {
    .proc_read  = fifo_read,
    .proc_write = fifo_write,
    .proc_lseek = noop_llseek,  /* procfs 通常不支持 seek 操作 */
};

/**
 * mod_init() — 模块初始化函数
 *
 * 功能：
 *   1. 初始化 kfifo（动态分配或静态初始化）
 *   2. 执行 kfifo 功能测试
 *   3. 创建 /proc/bytestream-fifo 文件供用户空间访问
 *
 * 返回值：成功返回 0，失败返回负数
 */
static int __init mod_init(void)
{
#ifdef DYNAMIC
    int ret;

    /* 动态分配模式：运行时分配 FIFO_SIZE 字节的缓冲区
     * GFP_KERNEL 表示可以休眠等待内存（仅在初始化时调用，安全）
     */
    ret = kfifo_alloc(&test, FIFO_SIZE, GFP_KERNEL);
    if (ret < 0) {
        printk(KERN_INFO "error kfifo_alloc\n");
        return ret;
    }
#else
    /* 静态声明模式：初始化已分配的缓冲区
     * INIT_KFIFO(test) 将 in 和 out 指针设为 0
     */
    INIT_KFIFO(test);
#endif

    /* 执行 kfifo 功能测试，打印各种操作的结果到内核日志 */
    if (test_func() < 0)
        return -EIO;

    /* 创建 /proc/bytestream-fifo 文件
     * 参数说明：
     *   PROC_FIFO：文件名
     *   0：权限（0 表示使用默认权限）
     *   NULL：父目录（NULL 表示在 /proc 根目录）
     *   &fifo_fops：文件操作结构体指针
     */
    if (proc_create(PROC_FIFO, 0, NULL, &fifo_fops) == NULL) {
#ifdef DYNAMIC
        /* 如果创建失败，需要释放已分配的 kfifo 内存 */
        kfifo_free(&test);
#endif
        return -ENOMEM;
    }

    return 0;
}

/**
 * mod_exit() — 模块退出函数
 *
 * 功能：
 *   1. 删除 /proc/bytestream-fifo 文件
 *   2. 释放 kfifo 占用的内存（动态分配模式）
 *
 * 注意：必须在模块卸载时清理所有资源，否则会导致内存泄漏或系统崩溃
 */
static void __exit mod_exit(void)
{
    /* 从 /proc 文件系统中删除 bytestream-fifo 条目
     * 参数说明：
     *   PROC_FIFO：文件名
     *   NULL：父目录（NULL 表示在 /proc 根目录）
     */
    remove_proc_entry(PROC_FIFO, NULL);

#ifdef DYNAMIC
    /* 动态分配模式：释放 kfifo 占用的内存 */
    kfifo_free(&test);
#endif

    return;
}

/* 注册模块初始化和退出函数 */
module_init(mod_init);
module_exit(mod_exit);

/* 模块许可证和作者信息 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie<ee.liweijie@gmail.com>");
