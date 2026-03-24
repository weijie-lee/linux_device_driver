# Linux 设备驱动开发详解 - 学习实践仓库

[![GitHub](https://img.shields.io/github/weijie-lee/linux_device_driver)](https://github.com/weijie-lee/linux_device_driver)

这是跟随《Linux设备驱动开发详解》（宋宝华 编著）一书学习实践的驱动代码仓库，包含完整可编译的示例代码和学习笔记。所有驱动均基于 Linux 内核模拟设备实现，无需真实硬件即可验证。

## 📁 目录结构

> **新手建议**：先阅读 [framework/README.md](./framework/README.md) 了解 Linux 驱动开发的通用框架与核心机制，再逐个阅读各驱动目录的代码。

| 目录 | 内容 | 对应章节 |
|------|------|----------|
| [**framework**](./framework/) | **Linux 驱动实现框架全景指南（含通用模板）** | **必读** |
| [container_of](./container_of/) | container_of 宏使用演示 | 基础 |
| [linked_lists](./linked_lists/) | 内核链表使用示例 | 基础 |
| [kfifo](./kfifo/) | 内核 kfifo 环形缓冲使用 | 基础 |
| [global_mem](./global_mem/) | 基础字符设备驱动（含并发控制 mutex） | 第6、7章 |
| [global_fifo](./global_fifo/) | 支持 poll/epoll/fasync 异步通知 | 高级 |
| [dma](./dma/) | DMA 使用演示 | 高级 |
| [seconds](./seconds/) | 内核定时器演示 | 基础 |
| [snull](./snull/) | 虚拟网络设备驱动（sn0/sn1 互 ping） | 第17章 |
| [vmem_disk](./vmem_disk/) | 虚拟内存磁盘块设备驱动 | 块设备 |
| [spi_driver](./spi_driver/) | SPI master 控制器 + slave 外设驱动（loopback 验证） | 第15章 |
| [i2c_driver](./i2c_driver/) | I2C adapter + client 驱动（内置 256B 寄存器文件） | 第14章 |
| [eth_driver](./eth_driver/) | 以太网 MAC+PHY 驱动（NAPI + 虚拟 PHY 状态机） | 第17章 |
| [mmc_driver](./mmc_driver/) | MMC/eMMC 主控驱动（64 MiB vmalloc 模拟存储卡） | 第18章 |
| [tests](./tests/) | 用户态测试程序（poll/fasync/epoll/second） | 配套测试 |

## 🚀 编译运行

### 环境要求

- Linux 内核头文件已安装（`linux-headers-$(uname -r)`）
- GCC 编译工具链
- Make
- 验证工具：`i2c-tools`（i2cdetect/i2cget/i2cset）、`ethtool`、`tcpdump`

### 快速开始

```bash
# 克隆仓库
git clone git@github.com:weijie-lee/linux_device_driver.git
cd linux_device_driver

# 编译所有模块
make all

# 编译单个模块（示例）
make spi_driver

# 加载测试（以 global_mem 为例）
cd global_mem
sudo insmod globalmem.ko
dmesg | tail
sudo rmmod globalmem

# 清理所有编译产物
make clean
```

### 各驱动加载示例

```bash
# SPI 驱动（必须先加载 master）
sudo insmod spi_driver/spi_master.ko
sudo insmod spi_driver/spi_slave.ko
echo -n "Hello SPI" > /dev/spi_virt && cat /dev/spi_virt

# I2C 驱动（必须先加载 master）
sudo insmod i2c_driver/i2c_master.ko
sudo insmod i2c_driver/i2c_slave.ko
sudo i2cdetect -y $(dmesg | grep i2c_virt_master | grep -o 'i2c-[0-9]*' | tail -1 | tr -d 'i2c-')

# 以太网 MAC+PHY 驱动
sudo insmod eth_driver/eth_mac.ko
sudo ip link set veth0_mac up
sudo ip addr add 192.168.99.1/24 dev veth0_mac
ping -c 4 192.168.99.1 -I veth0_mac

# MMC 驱动
sudo insmod mmc_driver/mmc_virt.ko
lsblk | grep mmcblk
sudo mkfs.ext4 /dev/mmcblk0
sudo mount /dev/mmcblk0 /mnt
```

### 详细说明

每个模块目录下都有独立的 README，包含知识点说明、代码结构讲解和完整验证步骤：

- [**framework/README**](./framework/README.md) — **Linux 驱动框架全景（字符/平台/总线/网络/块设备框架、中断、内存、调试）**
- [spi_driver/README](./spi_driver/README.md) — SPI master/slave 架构图、loopback 验证
- [i2c_driver/README](./i2c_driver/README.md) — I2C 事务类型、i2cdetect/i2cdump 验证
- [eth_driver/README](./eth_driver/README.md) — NAPI 原理、ethtool/tcpdump 验证
- [mmc_driver/README](./mmc_driver/README.md) — MMC 卡枚举流程、CID/CSD 格式、格式化挂载验证
- [global_mem/README](./global_mem/README.md)
- [snull/README](./snull/README.md)
- [tests/README](./tests/README.md)

## 🔧 代码整改记录

本仓库已进行全面代码审查，修复了以下问题：

| 文件 | 优先级 | 问题说明 |
|------|--------|---------|
| `vmem_disk/vmem_disk.c` | P0 | 修复 `init` 中 `kmalloc` 成功/失败判断取反 |
| `vmem_disk/vmem_disk.c` | P0 | 修复 `setup_device` 中 `out_vfree` 标签位置错误 |
| `vmem_disk/vmem_disk.c` | P0 | 修复结构体字段拼写错误：`gedisk` → `gendisk` |
| `vmem_disk/vmem_disk.c` | P0 | 实现原本为空的 `vmem_disk_exit()` |
| `vmem_disk/vmem_disk.c` | P2 | 删除重复的 `#include` |
| `global_mem/globalmem.c` | P0 | 修复 `exit` 中 `device_destroy`/`class_destroy` 顺序（悬空指针） |
| `global_mem/globalmem.c` | P1 | 替换 `trace_printk()` 为 `pr_err()`；修复 init 失败路径资源泄漏 |
| `global_fifo/globalfifo.c` | P0 | 修复等待队列竞态（漏掉唤醒信号导致永久阻塞） |
| `global_fifo/globalfifo.c` | P0 | 修复 `remove` 中 `device_destroy`/`class_destroy` 顺序 |
| `seconds/second.c` | P1 | 修复 `cdev_add` 失败后静默继续；修复 `class_create` 失败路径泄漏 |
| `snull/snull.c` | P1 | 修复 `init` 错误处理，返回真实错误码 |
| `snull/snull.c` | P1 | 删除约 20 处冗余调试 `printk` |
| `tests/` | P2 | 将用户态测试程序移至独立 `tests/` 目录 |

## 📚 参考书籍

1. **《Linux设备驱动开发详解》** - 宋宝华 编著
2. **《Linux Device Drivers Development》(LDD3)** - Alessandro Rubini & Jonathan Corbet

## 📋 已完成

- [x] SPI 驱动（master/slave）— 基于虚拟 SPI 控制器，loopback 验证
- [x] I2C 驱动（master/slave）— 内置 256B 寄存器文件，i2cdetect/i2cget/i2cset 验证
- [x] 以太网 MAC+PHY 驱动 — NAPI + 虚拟 PHY，ping/tcpdump 验证
- [x] MMC 驱动 — 64 MiB vmalloc 存储，格式化/挂载验证

## 📄 许可证

遵循原代码的 GPL 许可证，延续开源精神。
