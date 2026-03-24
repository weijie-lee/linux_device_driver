# Linux 设备驱动开发详解 - 学习实践仓库

[![GitHub](https://img.shields.io/github/weijie-lee/linux_device_driver)](https://github.com/weijie-lee/linux_device_driver)

这是跟随《Linux设备驱动开发详解》（宋宝华 编著）一书学习实践的驱动代码仓库，包含完整可编译的示例代码和学习笔记。

## 📁 目录结构

| 目录 | 内容 | 对应章节 |
|------|------|----------|
| [container_of](./container_of/) | container_of宏使用演示 | 基础 |
| [linked_lists](./linked_lists/) | 内核链表使用示例 | 基础 |
| [kfifo](./kfifo/) | 内核kfifo环形缓冲使用 | 基础 |
| [global_mem](./global_mem/) | 基础字符设备驱动（含并发控制mutex） | 第6、7章 |
| [global_fifo](./global_fifo/) | 支持poll/epoll/fasync异步通知 | 高级 |
| [dma](./dma/) | DMA使用演示 | 高级 |
| [seconds](./seconds/) | 内核定时器演示 | 基础 |
| [snull](./snull/) | 虚拟网络设备驱动（sn0/sn1互ping） | 第17章 |
| [vmem_disk](./vmem_disk/) | 虚拟内存磁盘块设备驱动 | 块设备 |

## 🚀 编译运行

### 环境要求
- Linux内核头文件已安装
- GCC编译工具链
- Make

### 快速开始
```bash
# 克隆仓库
git clone git@github.com:weijie-lee/linux_device_driver.git
cd linux_device_driver

# 编译所有模块
make all

# 编译单个模块（示例）
make container_of

# 加载测试（以container_of为例）
cd container_of
sudo insmod container_of.ko
dmesg | tail
sudo rmmod container_of

# 清理所有编译产物
make clean
```

### 详细说明
每个模块目录下都有独立的README，包含该模块的知识点说明和详细测试步骤：
- [container_of/README](./container_of/README.md)
- [global_mem/README](./global_mem/README.md)
- [snull/README](./snull/README.md)
- 等...

## 📚 参考书籍
1. **《Linux设备驱动开发详解》** - 宋宝华 编著
2. **《Linux Device Drivers Development》(LDD3)** - Alessandro Rubini & Jonathan Corbet

## 📋 待完成
按照原计划，还需要补充以下内容：
- [ ] SPI驱动（master/slave）
- [ ] I2C驱动（master/slave）
- [ ] 以太网MAC+PHY驱动
- [ ] MMC驱动

欢迎补充完善！

## 📄 许可证
遵循原代码的GPL许可证，延续开源精神。
