# global_mem - 基础字符设备驱动示例

## 知识点
这是一个完整的字符设备驱动示例，展示了Linux字符设备驱动的基本框架：

1. 模块加载与卸载
2. 设备号申请（`alloc_chrdev_region`）
3. 字符设备初始化（`cdev_init` / `cdev_add`）
4. 文件操作方法实现（open / read / write / release / ioctl）
5. 并发控制（mutex互斥锁）
6. 自动创建设备节点（class / device_create）

## 编译运行
```bash
make global_mem
cd global_mem
make

sudo insmod globalmem.ko

# 查看分配的主设备号
dmesg | tail
# 假设主设备号是243，创建设备节点（系统自动创建的话不需要这步）
sudo mknod /dev/g_m_0 c 243 0

# 测试读写
echo "hello world" > /dev/g_m_0
cat /dev/g_m_0

# ioctl 清空缓冲区
./clear /dev/g_m_0

# 卸载模块
sudo rmmod globalmem
```

## 重要API
| API | 用途 |
|-----|------|
| `alloc_chrdev_region` | 动态申请设备号 |
| `cdev_init` | 初始化字符设备 |
| `cdev_add` | 将字符设备加入内核 |
| `class_create` | 创建设备类，供udev自动创建设备节点 |
| `device_create` | 创建设备节点 |
| `copy_from_user` / `copy_to_user` | 内核空间与用户空间数据拷贝 |
| `mutex_lock` / `mutex_unlock` | 并发访问控制 |

## 注意事项
- 驱动支持多个设备（CONFIG_DEBUG_DRIVER，这里定义了DEVICE_NUM=4）
- 使用`container_of`从cdev指针获取自定义设备结构体指针
- 打开设备时将设备指针保存到`filp->private_data`，后续操作直接使用

## 参考
- 《Linux设备驱动开发详解》第6章、第7章
