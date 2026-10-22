# Ch02: 字符设备基础

## 章节概述

本章介绍 Linux 字符设备驱动的完整开发流程。字符设备是 Linux 中最常见的设备类型，包括终端、串口、磁盘等。本章通过实现一个具有 4096 字节内存缓冲区的虚拟字符设备，展示了字符设备开发的所有核心步骤，包括设备号管理、设备注册、文件操作、并发控制等。

## 知识点详解

### 1. 字符设备的基本概念

**定义**：字符设备是指按字节流进行读写的设备。与块设备不同，字符设备不支持随机访问（某些情况下除外），数据以字符流的形式传输。

**特点**：

- **字节流访问**：数据以单个字节为单位读写
- **无缓冲**：通常没有块设备那样的缓冲机制
- **顺序访问**：虽然支持 seek，但通常是顺序访问
- **设备号**：由主设备号和次设备号组成，主设备号标识驱动程序，次设备号标识具体设备

### 2. 设备号管理

**主设备号与次设备号**：

- **主设备号**（Major Number）：标识驱动程序，范围 0-255（某些内核版本扩展到更大范围）
- **次设备号**（Minor Number）：标识同一驱动程序管理的不同设备实例
- **设备号编码**：`dev_t` 是 32 位整数，高 12 位是主设备号，低 20 位是次设备号

**设备号申请方式**：

| 方式 | API | 说明 |
|------|-----|------|
| 静态申请 | `register_chrdev_region()` | 需要提前知道主设备号，容易与系统冲突 |
| 动态申请 | `alloc_chrdev_region()` | 内核自动分配主设备号，推荐使用 |

**代码示例**：

```c
dev_t devno;
int ret = alloc_chrdev_region(&devno, 0, DEVICE_NUM, "globalmem");
if (ret < 0) {
    pr_err("alloc_chrdev_region failed\n");
    return ret;
}
// devno 中现在包含了分配的主设备号和起始次设备号
```

### 3. 字符设备的注册流程

**标准的字符设备注册步骤**：

```
1. alloc_chrdev_region()    → 申请设备号
2. kzalloc()                → 分配设备结构体内存
3. cdev_init()              → 初始化 cdev 结构体
4. cdev_add()               → 将 cdev 添加到内核
5. class_create()           → 创建设备类（供 udev 识别）
6. device_create()          → 创建设备文件（/dev/xxx）
```

**关键数据结构**：

```c
struct cdev {
    struct kobject kobj;
    struct module *owner;
    const struct file_operations *ops;  // 文件操作函数指针
    struct list_head list;
    dev_t dev;
    unsigned int count;
};

struct global_mem_dev {
    struct cdev cdev;                       // 嵌入 cdev
    unsigned char mem[GLOBALMEM_SIZE];      // 设备内存
    struct mutex mutex;                     // 并发控制
};
```

### 4. 文件操作接口

**file_operations 结构体**：字符设备驱动必须实现的核心接口。

| 函数 | 说明 |
|------|------|
| `open()` | 打开设备，初始化设备状态 |
| `release()` | 关闭设备，清理资源 |
| `read()` | 从设备读取数据 |
| `write()` | 向设备写入数据 |
| `llseek()` | 改变文件位置指针 |
| `unlocked_ioctl()` | 设备控制命令 |

**关键函数说明**：

#### open() 函数

```c
static int global_mem_open(struct inode *inode, struct file *filp)
{
    // 从 inode->i_cdev 反推出设备结构体（container_of 用法）
    struct global_mem_dev *dev = container_of(inode->i_cdev,
                                              struct global_mem_dev, cdev);
    // 保存到文件私有数据，后续操作直接使用
    filp->private_data = dev;
    return 0;
}
```

**关键点**：
- 使用 `container_of` 从嵌入的 `cdev` 反推出整个设备结构体
- 将设备指针存入 `filp->private_data`，后续 read/write/ioctl 可直接获取

#### read() 函数

```c
static ssize_t global_mem_read(struct file *filp, char __user *buf,
                               size_t size, loff_t *ppos)
{
    struct global_mem_dev *dev = filp->private_data;
    unsigned long p = *ppos;
    unsigned int count = size;

    // 越界检查
    if (p >= GLOBALMEM_SIZE)
        return 0;
    if (count > GLOBALMEM_SIZE - p)
        count = GLOBALMEM_SIZE - p;

    // 加锁保护
    mutex_lock(&dev->mutex);
    
    // 内核空间→用户空间的安全拷贝
    if (copy_to_user(buf, dev->mem + p, count)) {
        mutex_unlock(&dev->mutex);
        return -EFAULT;
    }
    
    *ppos += count;
    mutex_unlock(&dev->mutex);
    return count;
}
```

**关键点**：
- 必须使用 `copy_to_user()` 而不是 `memcpy()`，因为用户空间指针可能无效
- 在持锁期间完成拷贝，防止并发修改
- 返回实际读取的字节数，或错误码

#### write() 函数

```c
static ssize_t global_mem_write(struct file *filp, const char __user *buf,
                                size_t size, loff_t *ppos)
{
    struct global_mem_dev *dev = filp->private_data;
    unsigned long p = *ppos;
    unsigned int count = size;

    if (p >= GLOBALMEM_SIZE)
        return 0;
    if (count > GLOBALMEM_SIZE - p)
        count = GLOBALMEM_SIZE - p;

    mutex_lock(&dev->mutex);
    
    // 用户空间→内核空间的安全拷贝
    if (copy_from_user(dev->mem + p, buf, count)) {
        mutex_unlock(&dev->mutex);
        return -EFAULT;
    }
    
    *ppos += count;
    mutex_unlock(&dev->mutex);
    return count;
}
```

**关键点**：
- 使用 `copy_from_user()` 从用户空间读取数据
- 同样需要加锁和错误检查

#### ioctl() 函数

```c
static long global_mem_ioctl(struct file *filp, unsigned int cmd,
                             unsigned long arg)
{
    struct global_mem_dev *dev = filp->private_data;

    switch (cmd) {
    case MEM_CLEAR:
        mutex_lock(&dev->mutex);
        memset(dev->mem, 0, GLOBALMEM_SIZE);
        mutex_unlock(&dev->mutex);
        pr_info("globalmem: buffer cleared\n");
        break;
    default:
        return -EINVAL;
    }
    return 0;
}
```

**关键点**：
- ioctl 用于实现设备特定的控制命令
- 命令号由魔数和序号组成，避免与其他驱动冲突
- 需要加锁保护共享资源

### 5. 并发控制

**互斥锁（Mutex）**：

```c
struct mutex {
    atomic_t count;
    spinlock_t wait_lock;
    struct list_head wait_list;
};

// 初始化
mutex_init(&dev->mutex);

// 加锁（可休眠）
mutex_lock(&dev->mutex);
// ... 临界区代码 ...
mutex_unlock(&dev->mutex);

// 尝试加锁（不休眠）
if (mutex_trylock(&dev->mutex)) {
    // ... 临界区代码 ...
    mutex_unlock(&dev->mutex);
}
```

**为什么需要并发控制**：

- 多个用户进程可能同时打开同一设备
- 多个进程可能同时读写设备内存
- 不加锁会导致数据竞争和不一致

### 6. 设备文件的自动创建

**传统方法**（已过时）：

```bash
# 手动创建设备文件
sudo mknod /dev/globalmem c 243 0
```

**现代方法**（推荐）：

```c
// 创建设备类
globalmem_class = class_create(THIS_MODULE, "globalmem");
if (IS_ERR(globalmem_class)) {
    pr_err("class_create failed\n");
    return PTR_ERR(globalmem_class);
}

// 为每个设备创建设备文件
for (i = 0; i < DEVICE_NUM; i++) {
    device_create(globalmem_class, NULL, 
                  MKDEV(MAJOR(devno), i),
                  NULL, chr_dev_name[i]);
}
```

**工作流程**：

1. `class_create()` 在 `/sys/class/globalmem/` 下创建类目录
2. `device_create()` 在 `/sys/class/globalmem/global_mem_0/` 等下创建设备目录
3. udev 守护进程监听这些事件
4. udev 根据规则在 `/dev/` 下自动创建设备文件

## 代码架构

### 核心数据结构

```c
struct global_mem_dev {
    struct cdev cdev;                       // 字符设备结构体（嵌入）
    unsigned char mem[GLOBALMEM_SIZE];      // 4KB 内存缓冲区
    struct mutex mutex;                     // 并发控制锁
};

static struct global_mem_dev *global_mem_devp;  // 设备数组指针
static dev_t devno;                             // 设备号
static struct class *globalmem_class;           // 设备类
```

### 初始化流程

1. **申请设备号**：`alloc_chrdev_region(&devno, 0, DEVICE_NUM, "globalmem")`
2. **分配内存**：`global_mem_devp = kzalloc(sizeof(...) * DEVICE_NUM, GFP_KERNEL)`
3. **初始化每个设备**：
   - `mutex_init(&dev->mutex)`
   - `cdev_init(&dev->cdev, &global_mem_fops)`
   - `cdev_add(&dev->cdev, MKDEV(MAJOR(devno), i), 1)`
4. **创建设备类**：`class_create(THIS_MODULE, "globalmem")`
5. **创建设备文件**：`device_create(globalmem_class, NULL, MKDEV(...), NULL, name)`

### 退出流程（必须严格逆序）

```
device_destroy()
    ↓
class_destroy()
    ↓
cdev_del()
    ↓
kfree()
    ↓
unregister_chrdev_region()
```

**为什么必须逆序**：
- 如果先 `class_destroy()`，再 `device_destroy()` 会访问已释放的内存
- 必须先销毁设备文件，再销毁类，最后释放驱动程序

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch02
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch02_char_basic_globalmem.ko
insmod ok
+ ls /dev/global_mem_0 /dev/global_mem_1 /dev/global_mem_2 /dev/global_mem_3
/dev/global_mem_0
/dev/global_mem_1
/dev/global_mem_2
/dev/global_mem_3
+ test -c /dev/global_mem_0 -o -b /dev/global_mem_0
node ok
+ echo test_data > /dev/global_mem_0
+ head -c 9 /dev/global_mem_0
test_data
+ /bin/test_globalmem
=== Ch02 Globalmem Character Device Test ===

[TC01] open/close
  [PASS] open /dev/global_mem_0
[TC02] write
  [PASS] write returns correct byte count
[TC03] read
  [PASS] read returns correct byte count
  [PASS] read data matches written data
[TC04] lseek
  [PASS] read after lseek returns 9 bytes
  [PASS] lseek read content correct
[TC05] Large write (boundary test)
  [PASS] write MEM_SIZE bytes succeeds
[TC06] Multiple open
  [PASS] second open succeeds

=== Test Summary ===
PASS: 8  FAIL: 0
+ rmmod globalmem
rmmod ok
[PASS] ch02_globalmem
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch02_globalmem`
- `ls` 列出 `global_mem_0` 到 `global_mem_3`，并且 `node ok`（它们是字符设备，不是普通文件）
- `head -c 9 /dev/global_mem_0` 是 `test_data`
- 用户态程序 `test_globalmem` 的结尾是 `PASS: 8  FAIL: 0`

设备节点是 `global_mem_N`，不是 `/dev/globalmem`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch02
```

## 常见问题

### Q: 为什么必须使用 copy_to_user/copy_from_user？

A: 用户空间指针可能无效、指向内核空间、或者指向不可访问的内存。直接使用 `memcpy()` 会导致页错误或安全漏洞。`copy_to_user/copy_from_user` 会进行必要的检查和异常处理，确保安全。

### Q: container_of 是如何工作的？

A: `container_of(ptr, type, member)` 通过计算成员在结构体中的偏移量，从成员指针反推出整个结构体的首地址。公式是：`结构体首地址 = 成员地址 - 成员偏移量`。这是 Linux 内核的侵入式设计模式。

### Q: 为什么需要加 mutex 锁？

A: 多个用户进程可能同时读写同一设备。不加锁会导致数据竞争：一个进程在读取时，另一个进程可能在修改数据，导致读到不一致的状态。Mutex 确保同一时刻只有一个进程访问共享资源。

### Q: 如何支持多个设备实例？

A: 通过申请多个次设备号。本驱动申请了 4 个次设备号（0-3），创建了 4 个独立的设备实例，每个实例有自己的内存缓冲区和 mutex。用户可以同时打开多个设备文件。

### Q: ioctl 命令号是如何编码的？

A: Linux 内核定义了一套宏来编码 ioctl 命令：
- `_IO(type, nr)`：不传递数据
- `_IOW(type, nr, size)`：写入数据（内核读取）
- `_IOR(type, nr, size)`：读取数据（内核写入）
- `_IOWR(type, nr, size)`：读写数据

其中 `type` 是魔数（通常是单个字符），`nr` 是命令序号。这样可以避免与其他驱动的命令冲突。

## 参考资源

- **Linux Kernel Documentation**：https://www.kernel.org/doc/html/latest/driver-api/index.html
- **Linux Device Drivers, 3rd Edition**：https://lwn.net/Kernel/LDD3/（第 3 章：字符设备）
- **Kernel Source - include/linux/cdev.h**：https://elixir.bootlin.com/linux/v5.15/source/include/linux/cdev.h
- **Kernel Source - include/linux/fs.h**：https://elixir.bootlin.com/linux/v5.15/source/include/linux/fs.h

---

**最后更新**：2026 年 3 月 26 日  
**章节版本**：1.0.0  
**内核支持**：5.15.0+  
**测试状态**：✅ PASS (QEMU)
