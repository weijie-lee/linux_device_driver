# Linux 驱动实现框架全景指南

> 本章节系统梳理 Linux 内核驱动开发的通用框架与核心机制，是阅读本仓库所有驱动代码的理论基础。

---

## 目录

1. [驱动与内核的关系](#1-驱动与内核的关系)
2. [字符设备驱动框架](#2-字符设备驱动框架)
3. [平台设备驱动框架](#3-平台设备驱动框架)
4. [总线设备驱动模型](#4-总线设备驱动模型)
5. [网络设备驱动框架](#5-网络设备驱动框架)
6. [块设备驱动框架](#6-块设备驱动框架)
7. [中断与并发控制](#7-中断与并发控制)
8. [内存管理](#8-内存管理)
9. [模块生命周期与资源管理](#9-模块生命周期与资源管理)
10. [调试技术](#10-调试技术)

---

## 1. 驱动与内核的关系

Linux 内核通过**分层抽象**将硬件细节与上层应用隔离。驱动程序是内核与硬件之间的桥梁，运行在内核空间，通过标准接口向用户空间暴露设备能力。

```
用户空间
  │  open() / read() / write() / ioctl()
  │  系统调用接口（syscall）
──┼──────────────────────────────────────
  │  虚拟文件系统（VFS）
  │  字符设备层 / 块设备层 / 网络协议栈
  │  ↓
  │  驱动程序（本仓库的内容）
  │  ↓
  │  硬件抽象层（HAL）
──┼──────────────────────────────────────
硬件
  │  寄存器 / DMA / 中断
```

### 驱动的三种类型

| 类型 | 描述 | 典型设备 | 本仓库示例 |
|------|------|---------|-----------|
| **字符设备** | 以字节流方式读写，无缓冲区 | 串口、键盘、传感器 | globalmem, globalfifo, second |
| **块设备** | 以固定大小块读写，有缓冲区 | 硬盘、SD 卡、eMMC | vmem_disk, mmc_driver |
| **网络设备** | 不通过文件系统，通过 socket 访问 | 以太网卡、WiFi | snull, eth_driver |

---

## 2. 字符设备驱动框架

字符设备是最基础的驱动类型，框架围绕 `struct cdev` 和 `struct file_operations` 展开。

### 2.1 核心数据结构

```c
/* 字符设备的内核表示 */
struct cdev {
    struct kobject kobj;
    struct module *owner;
    const struct file_operations *ops;  /* 操作函数表 */
    struct list_head list;
    dev_t dev;                          /* 主设备号 + 次设备号 */
    unsigned int count;
};

/* 文件操作函数表：驱动的"接口合同" */
struct file_operations {
    struct module *owner;
    loff_t  (*llseek)   (struct file *, loff_t, int);
    ssize_t (*read)     (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write)    (struct file *, const char __user *, size_t, loff_t *);
    long    (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);
    int     (*open)     (struct inode *, struct file *);
    int     (*release)  (struct inode *, struct file *);
    __poll_t (*poll)    (struct file *, struct poll_table_struct *);
    int     (*fasync)   (int, struct file *, int);
    /* ... 更多操作 ... */
};
```

### 2.2 标准初始化流程

```
module_init()
    │
    ├─ alloc_chrdev_region()    ← 动态分配主/次设备号
    │   或 register_chrdev_region()  ← 静态指定设备号
    │
    ├─ kmalloc()/kzalloc()      ← 分配设备私有数据
    │
    ├─ cdev_init()              ← 初始化 cdev，绑定 file_operations
    ├─ cdev_add()               ← 将 cdev 注册到内核（此后设备可用）
    │
    ├─ class_create()           ← 创建设备类（/sys/class/xxx）
    └─ device_create()          ← 创建设备节点（/dev/xxx，由 udev 自动创建）
```

### 2.3 标准退出流程（与初始化严格相反）

```
module_exit()
    │
    ├─ device_destroy()         ← 必须在 class_destroy() 之前！
    ├─ class_destroy()
    ├─ cdev_del()
    ├─ kfree()
    └─ unregister_chrdev_region()
```

> **常见错误**：将 `device_destroy()` 放在 `class_destroy()` 之后，导致访问悬空指针。

### 2.4 用户空间与内核空间的数据传输

```c
/* 用户空间 → 内核空间 */
copy_from_user(kernel_buf, user_buf, count);

/* 内核空间 → 用户空间 */
copy_to_user(user_buf, kernel_buf, count);

/* 单个基本类型的简化版本 */
get_user(kernel_val, user_ptr);   /* 读取用户空间单值 */
put_user(kernel_val, user_ptr);   /* 写入用户空间单值 */
```

### 2.5 私有数据访问模式

驱动通常将设备私有数据嵌入包含 `struct cdev` 的结构体，通过 `container_of` 宏从 `inode->i_cdev` 反向获取：

```c
struct my_dev {
    struct cdev cdev;       /* 必须是结构体成员，不能是指针 */
    int my_data;
    /* ... */
};

static int my_open(struct inode *inode, struct file *filp)
{
    struct my_dev *dev = container_of(inode->i_cdev, struct my_dev, cdev);
    filp->private_data = dev;   /* 保存到 file 中，后续 read/write 直接使用 */
    return 0;
}
```

---

## 3. 平台设备驱动框架

平台设备（Platform Device）是 Linux 对片上外设（SoC 内部外设）的抽象，这些设备没有自动枚举机制（不像 PCI/USB），需要通过设备树（Device Tree）或代码静态描述。

### 3.1 核心概念

```
设备树（DTS）
    │  of_match_table 匹配
    ↓
platform_device（描述硬件资源：寄存器地址、中断号、时钟等）
    │  platform_driver_register() 触发 probe
    ↓
platform_driver（实现 probe/remove/suspend/resume）
```

### 3.2 驱动注册模板

```c
/* 设备树匹配表 */
static const struct of_device_id my_dt_ids[] = {
    { .compatible = "vendor,my-device" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, my_dt_ids);

/* probe 函数：设备被发现时调用 */
static int my_probe(struct platform_device *pdev)
{
    struct resource *res;
    void __iomem *base;

    /* 获取寄存器基地址（来自设备树 reg 属性） */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(base))
        return PTR_ERR(base);

    /* 获取中断号（来自设备树 interrupts 属性） */
    int irq = platform_get_irq(pdev, 0);
    if (irq < 0)
        return irq;

    /* 使用 devm_* 系列函数自动管理资源 */
    return devm_request_irq(&pdev->dev, irq, my_irq_handler, 0, "my-dev", priv);
}

static struct platform_driver my_driver = {
    .probe  = my_probe,
    .remove = my_remove,
    .driver = {
        .name           = "my-device",
        .of_match_table = my_dt_ids,
        .pm             = &my_pm_ops,
    },
};
module_platform_driver(my_driver);  /* 自动生成 module_init/exit */
```

### 3.3 devm_* 资源管理

`devm_*` 系列函数（Device-Managed Resources）在设备解绑时自动释放资源，大幅简化错误处理：

| 传统方式 | devm_* 方式 |
|---------|------------|
| `ioremap()` + `iounmap()` | `devm_ioremap()` |
| `kmalloc()` + `kfree()` | `devm_kmalloc()` |
| `request_irq()` + `free_irq()` | `devm_request_irq()` |
| `clk_get()` + `clk_put()` | `devm_clk_get()` |

---

## 4. 总线设备驱动模型

Linux 设备模型（LDM）的核心是**总线-设备-驱动**三元组，统一管理所有总线类型（I2C、SPI、USB、PCI 等）。

### 4.1 通用模型

```
struct bus_type
    ├─ match()      ← 判断设备和驱动是否匹配
    ├─ probe()      ← 匹配成功后调用驱动的 probe
    └─ remove()     ← 设备移除时调用

struct device           struct device_driver
    │                       │
    └─── bus_type ──────────┘
         match() 成功 → driver->probe(device)
```

### 4.2 I2C 驱动框架

```c
/* I2C 从设备驱动 */
static const struct i2c_device_id my_i2c_id[] = {
    { "my-sensor", 0 },
    { }
};

static int my_i2c_probe(struct i2c_client *client,
                        const struct i2c_device_id *id)
{
    /* client->addr 是设备的 7 位 I2C 地址 */
    /* client->adapter 是所在的 I2C 总线控制器 */

    /* 读写寄存器 */
    u8 val = i2c_smbus_read_byte_data(client, REG_ADDR);
    i2c_smbus_write_byte_data(client, REG_ADDR, new_val);

    return 0;
}

static struct i2c_driver my_i2c_driver = {
    .driver = { .name = "my-sensor" },
    .probe  = my_i2c_probe,
    .remove = my_i2c_remove,
    .id_table = my_i2c_id,
};
module_i2c_driver(my_i2c_driver);
```

### 4.3 SPI 驱动框架

```c
/* SPI 从设备驱动 */
static int my_spi_probe(struct spi_device *spi)
{
    /* 配置 SPI 参数 */
    spi->max_speed_hz = 1000000;
    spi->mode = SPI_MODE_0;
    spi_setup(spi);

    /* 全双工传输 */
    struct spi_transfer xfer = {
        .tx_buf = tx_data,
        .rx_buf = rx_data,
        .len    = len,
    };
    struct spi_message msg;
    spi_message_init(&msg);
    spi_message_add_tail(&xfer, &msg);
    spi_sync(spi, &msg);

    return 0;
}
```

---

## 5. 网络设备驱动框架

网络设备驱动不通过文件系统访问，而是通过内核网络协议栈。核心结构体是 `struct net_device`。

### 5.1 核心数据结构

```c
struct net_device_ops {
    int     (*ndo_open)        (struct net_device *dev);
    int     (*ndo_stop)        (struct net_device *dev);
    netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb, struct net_device *dev);
    void    (*ndo_tx_timeout)  (struct net_device *dev, unsigned int txqueue);
    struct net_device_stats* (*ndo_get_stats)(struct net_device *dev);
    int     (*ndo_change_mtu)  (struct net_device *dev, int new_mtu);
    /* ... */
};
```

### 5.2 发送路径

```
用户空间 write()/sendmsg()
    │
    ↓ 内核网络协议栈（TCP/IP）
    │
    ↓ dev_queue_xmit()
    │
    ↓ ndo_start_xmit(skb, dev)    ← 驱动实现
    │
    ├─ 将 skb 数据写入硬件 TX FIFO 或 DMA 描述符
    ├─ 触发硬件发送
    └─ 等待 TX 完成中断 → dev_kfree_skb(skb)
```

### 5.3 接收路径（NAPI 模式）

```
硬件收到数据包 → 触发 RX 中断
    │
    ├─ 中断处理函数：
    │   ├─ 关闭 RX 中断（防止中断风暴）
    │   └─ napi_schedule(&priv->napi)  ← 调度软中断轮询
    │
    └─ NAPI poll 函数（软中断上下文）：
        ├─ 从硬件 RX FIFO 或 DMA 描述符读取包
        ├─ dev_alloc_skb() 分配 skb
        ├─ 填充 skb 数据
        ├─ netif_receive_skb(skb)  ← 上送协议栈
        └─ 处理完毕后重新开启 RX 中断
```

### 5.4 sk_buff（套接字缓冲区）

`sk_buff` 是内核网络层中数据包的统一表示，贯穿整个协议栈：

```
skb->head  ──→ [headroom] [以太网头] [IP头] [TCP头] [数据] [tailroom]
skb->data  ──────────────→ 当前层数据起始位置
skb->tail  ──────────────────────────────────────→ 数据结束位置
skb->end   ──────────────────────────────────────────────────→ 缓冲区结束

skb_push(skb, len)  ← 向头部添加协议头（封装时使用）
skb_pull(skb, len)  ← 移除头部协议头（解封装时使用）
skb_put(skb, len)   ← 向尾部追加数据
skb_reserve(skb, len) ← 预留头部空间（初始化时使用）
```

---

## 6. 块设备驱动框架

块设备以固定大小的块（通常 512 字节或 4096 字节）进行读写，内核有完整的 I/O 调度和缓冲层。

### 6.1 核心数据结构

```c
struct block_device_operations {
    int  (*open)          (struct block_device *, fmode_t);
    void (*release)       (struct gendisk *, fmode_t);
    int  (*ioctl)         (struct block_device *, fmode_t, unsigned, unsigned long);
    int  (*getgeo)        (struct block_device *, struct hd_geometry *);
    /* ... */
};

struct gendisk {
    int major;
    int first_minor;
    int minors;             /* 分区数量上限 */
    char disk_name[DISK_NAME_LEN];
    struct block_device_operations *fops;
    struct request_queue *queue;
    void *private_data;
    /* ... */
};
```

### 6.2 请求处理流程

```
用户空间 read()/write()
    │
    ↓ 文件系统层（ext4/xfs 等）
    │
    ↓ 通用块层（bio 提交）
    │
    ↓ I/O 调度器（CFQ/Deadline/BFQ）
    │
    ↓ request_fn()/blk_mq_ops.queue_rq()  ← 驱动实现
    │
    ├─ 解析 request：rq_for_each_segment() 遍历 bio 段
    ├─ 执行实际 I/O（DMA 传输或内存拷贝）
    └─ blk_end_request_all(rq, 0)  ← 通知完成
```

---

## 7. 中断与并发控制

### 7.1 中断处理机制

Linux 将中断处理分为两个阶段：

```
硬件中断触发
    │
    ↓ 上半部（Top Half）—— 硬中断上下文
    │   ├─ 不能睡眠
    │   ├─ 不能使用互斥锁
    │   ├─ 读取状态寄存器
    │   ├─ 清除中断标志
    │   └─ 调度下半部（tasklet/workqueue/softirq）
    │
    ↓ 下半部（Bottom Half）—— 软中断/进程上下文
        ├─ tasklet：软中断上下文，不能睡眠
        ├─ workqueue：进程上下文，可以睡眠
        └─ NAPI poll：软中断上下文，网络专用
```

### 7.2 同步原语选择指南

| 场景 | 推荐原语 | 说明 |
|------|---------|------|
| 进程上下文，不需要睡眠 | `spinlock_t` | `spin_lock()`/`spin_unlock()` |
| 进程上下文，可能睡眠 | `struct mutex` | `mutex_lock()`/`mutex_unlock()` |
| 中断上下文访问共享数据 | `spinlock_t` + `irqsave` | `spin_lock_irqsave()`/`spin_unlock_irqrestore()` |
| 读多写少 | `rwlock_t` 或 `rwsem` | 允许并发读 |
| 单个整数的原子操作 | `atomic_t` | `atomic_inc()`/`atomic_read()` 等 |
| 等待条件成立 | `wait_queue_head_t` | `wait_event_interruptible()` |

### 7.3 等待队列使用模式

```c
/* 声明等待队列头 */
DECLARE_WAIT_QUEUE_HEAD(my_wq);

/* 等待方（进程上下文）*/
wait_event_interruptible(my_wq, condition);
/* 等价于：
 *   while (!condition) {
 *       set_current_state(TASK_INTERRUPTIBLE);
 *       if (!condition) schedule();
 *       set_current_state(TASK_RUNNING);
 *   }
 */

/* 唤醒方（可以在中断上下文）*/
wake_up_interruptible(&my_wq);

/* 竞态陷阱：condition 检查必须在持锁状态下进行！
 * 错误写法：
 *   mutex_unlock(&lock);
 *   set_current_state(TASK_INTERRUPTIBLE);  ← 此时可能已错过唤醒信号
 *   schedule();
 *
 * 正确写法：使用 wait_event_interruptible_lock_irq()
 * 或将 set_current_state 放在 mutex_unlock 之前
 */
```

---

## 8. 内存管理

### 8.1 内核内存分配函数对比

| 函数 | 大小限制 | 物理连续 | 可睡眠 | 用途 |
|------|---------|---------|-------|------|
| `kmalloc(size, GFP_KERNEL)` | < 128KB | 是 | 是 | 通用小块内存 |
| `kzalloc(size, GFP_KERNEL)` | < 128KB | 是 | 是 | 同上，但清零 |
| `vmalloc(size)` | 任意大小 | 否 | 是 | 大块内存（如 vmem_disk） |
| `get_free_pages(GFP_KERNEL, order)` | 2^order 页 | 是 | 是 | 页对齐内存 |
| `dma_alloc_coherent()` | 受限 | 是 | 是 | DMA 一致性内存 |

### 8.2 GFP 标志

```c
GFP_KERNEL   /* 可睡眠，最常用 */
GFP_ATOMIC   /* 不可睡眠，用于中断上下文 */
GFP_DMA      /* 分配 DMA 可访问的内存（低 16MB） */
__GFP_ZERO   /* 分配后清零（kzalloc 内部使用） */
```

---

## 9. 模块生命周期与资源管理

### 9.1 错误处理的 goto 模式

内核驱动的标准错误处理模式：按资源分配的逆序释放。

```c
static int __init my_init(void)
{
    int ret;

    ret = alloc_resource_A();
    if (ret) goto fail_A;

    ret = alloc_resource_B();
    if (ret) goto fail_B;

    ret = alloc_resource_C();
    if (ret) goto fail_C;

    return 0;

fail_C:
    free_resource_B();
fail_B:
    free_resource_A();
fail_A:
    return ret;
}
```

### 9.2 模块参数

```c
static int debug = 0;
static char *name = "default";

module_param(debug, int, 0644);    /* 0644: root 可写，所有人可读 */
module_param(name, charp, 0444);   /* 0444: 只读 */

MODULE_PARM_DESC(debug, "Enable debug output (0=off, 1=on)");
```

### 9.3 必要的模块宏

```c
MODULE_LICENSE("GPL");              /* 必须，否则内核会标记为 tainted */
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Driver description");
MODULE_VERSION("1.0");
MODULE_DEVICE_TABLE(of, dt_ids);    /* 导出设备表，供 udev 自动加载 */
```

---

## 10. 调试技术

### 10.1 日志输出

```c
pr_emerg("...");    /* KERN_EMERG:   系统不可用 */
pr_alert("...");    /* KERN_ALERT:   需要立即处理 */
pr_crit("...");     /* KERN_CRIT:    严重错误 */
pr_err("...");      /* KERN_ERR:     错误 */
pr_warn("...");     /* KERN_WARNING: 警告 */
pr_notice("...");   /* KERN_NOTICE:  正常但重要 */
pr_info("...");     /* KERN_INFO:    信息 */
pr_debug("...");    /* KERN_DEBUG:   调试（需要 DEBUG 宏） */

/* 带设备信息的日志（推荐在 platform/i2c/spi 驱动中使用） */
dev_err(&pdev->dev, "Failed to probe: %d\n", ret);
dev_info(&pdev->dev, "Probed successfully\n");
```

### 10.2 动态调试

```bash
# 开启特定模块的调试输出
echo "module my_driver +p" > /sys/kernel/debug/dynamic_debug/control

# 查看当前调试配置
cat /sys/kernel/debug/dynamic_debug/control
```

### 10.3 常用调试工具

| 工具 | 用途 |
|------|------|
| `dmesg` / `journalctl -k` | 查看内核日志 |
| `lsmod` | 列出已加载模块 |
| `modinfo` | 查看模块信息 |
| `/proc/devices` | 查看已注册的设备号 |
| `/sys/class/` | 查看设备类 |
| `strace` | 跟踪系统调用 |
| `ftrace` | 内核函数跟踪 |
| `perf` | 性能分析 |

---

## 本仓库驱动与框架的对应关系

| 驱动目录 | 框架类型 | 演示的核心机制 |
|---------|---------|--------------|
| `container_of/` | 基础工具 | container_of 宏、内核链表 |
| `linked_lists/` | 基础工具 | list_head 双向链表 API |
| `kfifo/` | 基础工具 | 内核 FIFO 环形缓冲区 |
| `global_mem/` | 字符设备 | cdev 注册、copy_to/from_user |
| `global_fifo/` | 字符设备 | 等待队列、poll/fasync/epoll |
| `seconds/` | 字符设备 | 内核定时器、atomic_t |
| `snull/` | 网络设备 | net_device、skb、中断模拟 |
| `vmem_disk/` | 块设备 | gendisk、request_queue |
| `dma/` | 平台设备 | DMA 映射、dma_alloc_coherent |
| `spi_driver/` | SPI 总线 | spi_master/spi_driver |
| `i2c_driver/` | I2C 总线 | i2c_adapter/i2c_driver |
| `eth_driver/` | 网络设备 | NAPI、虚拟 PHY、ethtool |
| `mmc_driver/` | MMC 总线 | mmc_host、卡枚举、块 I/O |
