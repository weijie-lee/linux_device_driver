# global_mem - 基础字符设备驱动示例

这是Linux字符设备驱动的**完整标准模板**，包含了字符设备开发所有核心步骤。

## 知识点
- ✅ 模块加载与卸载框架
- ✅ 动态设备号申请（`alloc_chrdev_region`）
- ✅ 字符设备初始化（`cdev_init` / `cdev_add`）
- ✅ 文件操作方法实现（open / read / write / release / ioctl / llseek）
- ✅ 并发控制（mutex互斥锁保护共享数据）
- ✅ 自动创建设备节点（class / device_create，udev自动生成/dev）
- ✅ container_of 经典用法（从内嵌cdev获取自定义设备结构体）

## 代码结构讲解

### 设备结构体
```c
struct global_mem_dev {
	struct cdev cdev;             // 内核字符设备结构，嵌入自定义结构体
	unsigned char mem[GLOBALMEM_SIZE];  // 设备就是一块4KB内存
	struct mutex mutex;           // 并发访问互斥锁
};
```

### 字符设备开发标准步骤
| 阶段 | API | 作用 |
|------|-----|------|
| 初始化 | `alloc_chrdev_region` | 动态申请设备号，不与系统冲突 |
| 初始化 | `kzalloc` | 分配设备结构体内存 |
| 初始化 | `mutex_init` | 初始化并发锁 |
| 初始化 | `cdev_init` | 初始化cdev，绑定file_operations |
| 初始化 | `cdev_add` | 将设备正式注册到内核 |
| 初始化 | `class_create` + `device_create` | 创建设备类，udev自动创建设备节点 |
| 退出 | 反向释放 | cdev_del → kfree → unregister_chrdev_region → device_destroy → class_destroy |

### open 函数 - container_of 经典用法
```c
struct global_mem_dev *dev = container_of(inode->i_cdev, 
                                           struct global_mem_dev, cdev);
filp->private_data = dev;  // 保存到文件私有数据，后续操作直接用
```
**这是Linux驱动开发的标准范式！** 记住这个写法。

## 编译运行
```bash
# 编译
make global_mem
cd global_mem
make

# 加载模块
sudo insmod globalmem.ko

# 查看分配的主设备号
dmesg | tail
# 输出: chrdev alloc success, major:243, minor:0

# 测试读写（系统自动创建设备节点 /dev/global_mem_0）
echo "Hello, Linux Driver!" > /dev/global_mem_0
cat /dev/global_mem_0
# 输出: Hello, Linux Driver!

# 使用ioctl清空缓冲区（需要简单测试程序）
# 编译运行test_clear后:
./test_clear
# 再cat就是空的了

# 卸载模块
sudo rmmod globalmem
```

## 测试程序示例 (test_clear.c)
```c
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define GLOBAL_MEM_MAGIC 'g'
#define MEM_CLEAR _IO(GLOBAL_MEM_MAGIC, 0)

int main(int argc, char **argv)
{
    int fd = open("/dev/global_mem_0", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    ioctl(fd, MEM_CLEAR, 0);
    printf("Buffer cleared!\n");
    close(fd);
    return 0;
}
```

## 核心要点

| 要点 | 说明 |
|------|------|
| **container_of** | 从内嵌的cdev指针反推出整个自定义结构体指针 - 这是内核开发基本功 |
| **copy_from/to_user** | 必须用这两个函数和用户空间拷贝数据，不能直接memcpy |
| **mutex** | 共享数据一定要加并发保护，防止竞态条件 |
| **llseek** | 实现llseek让用户能lseek定位读写，更灵活 |
| **ioctl** | 用于扩展命令，这里实现了MEM_CLEAR清空缓冲区 |
| **错误处理** | 初始化失败要正确回滚，用goto做错误处理是内核标准写法 |

## 支持多设备
这个驱动支持4个设备：`/dev/global_mem_0` ~ `/dev/global_mem_3`，每个设备有独立的内存缓冲区。

## 参考
- 《Linux设备驱动开发详解》第6章、第7章
