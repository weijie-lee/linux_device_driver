# vmem_disk - 虚拟内存磁盘块设备驱动

## 知识点
实现一个基于内存的虚拟磁盘块设备驱动：
- 申请一块内存作为磁盘存储
- 实现块设备驱动的基本框架
- 支持磁盘读写操作

## 编译运行
```bash
make vmem_disk
cd vmem_disk
make
sudo insmod vmem_disk.ko
# 查看设备
ls /dev/vmem_disk*
# 测试读写
sudo dd if=/dev/zero of=/dev/vmem_disk bs=1k count=100
sudo dd if=/dev/vmem_disk of=/dev/null bs=1k count=100
sudo rmmod vmem_disk
```

## 参考
- 《Linux设备驱动开发详解》相关章节
