# Linux 设备驱动开发详解 — 学习实践仓库

[![License: GPL](https://img.shields.io/badge/License-GPL-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Kernel](https://img.shields.io/badge/Kernel-5.x%2F6.x-orange.svg)](https://kernel.org)

本仓库是跟随《Linux设备驱动开发详解》（宋宝华 编著）系统学习的实践代码库，覆盖 Linux 内核驱动开发的全部主要子系统。所有驱动均基于**内核模拟设备**实现，无需真实硬件即可在标准 Linux 环境中编译、加载和验证。

每个驱动目录均包含：
- **驱动源码**（`.c`）：含详细中文注释，解释每个函数和关键机制
- **测试用例**（`tests/`）：用户态 C 测试程序 + Shell 自动化验证脚本
- **独立 README**：知识点讲解、代码结构说明、完整验证步骤

---

## 学习路径

> **建议按章节编号顺序学习**，每一章都建立在前一章的基础之上。

```
Ch00 框架总览（必读）
  └─ Ch01 内核基础工具
       └─ Ch02 字符设备（基础）
            └─ Ch03 字符设备（进阶：阻塞/poll/fasync）
                 └─ Ch04 内核定时器与时间管理
                      └─ Ch05 Misc 杂项设备
                           └─ Ch06 Platform 平台设备
                                └─ Ch07 Input 输入子系统
                                     └─ Ch08 Regmap 寄存器抽象
                                          ├─ Ch09 Watchdog 看门狗
                                          ├─ Ch10 RTC 实时时钟
                                          ├─ Ch11 PWM 脉宽调制
                                          ├─ Ch12 DMA 内存管理
                                          ├─ Ch13 网络设备（虚拟 snull）
                                          ├─ Ch14 网络设备（MAC+PHY）
                                          ├─ Ch15 I2C 总线驱动
                                          ├─ Ch16 SPI 总线驱动
                                          ├─ Ch17 块设备驱动
                                          └─ Ch18 MMC 存储驱动
```

---

## 目录结构

| 章节 | 目录 | 难度 | 核心知识点 |
|------|------|------|-----------|
| [Ch00](./framework/) | `framework/` | ⭐ 必读 | 驱动框架全景、通用模板、同步原语选择指南 |
| [Ch01](./ch01_kernel_basics/) | `ch01_kernel_basics/` | ⭐ 入门 | `container_of`、内核链表、`kfifo` 环形缓冲 |
| [Ch02](./ch02_char_basic/) | `ch02_char_basic/` | ⭐⭐ 基础 | `cdev`、`file_operations`、`copy_to/from_user` |
| [Ch03](./ch03_char_advanced/) | `ch03_char_advanced/` | ⭐⭐ 基础 | 等待队列、`poll`/`select`/`epoll`、`fasync` 异步通知 |
| [Ch04](./ch04_timer/) | `ch04_timer/` | ⭐⭐ 基础 | `timer_list`、`jiffies`、`atomic_t`、`hrtimer` |
| [Ch05](./ch05_misc/) | `ch05_misc/` | ⭐⭐ 基础 | `misc_register`、最简字符设备变体 |
| [Ch06](./ch06_platform/) | `ch06_platform/` | ⭐⭐⭐ 进阶 | `platform_driver`、`devm_*` 资源管理、设备树匹配 |
| [Ch07](./ch07_input/) | `ch07_input/` | ⭐⭐⭐ 进阶 | `input_dev`、事件上报、`evdev` 接口 |
| [Ch08](./ch08_regmap/) | `ch08_regmap/` | ⭐⭐⭐ 进阶 | `regmap_config`、寄存器缓存、I2C/SPI 统一访问 |
| [Ch09](./ch09_watchdog/) | `ch09_watchdog/` | ⭐⭐⭐ 进阶 | `watchdog_device`、喂狗机制、`nowayout` |
| [Ch10](./ch10_rtc/) | `ch10_rtc/` | ⭐⭐⭐ 进阶 | `rtc_device`、`rtc_ops`、时间读写、闹钟中断 |
| [Ch11](./ch11_pwm/) | `ch11_pwm/` | ⭐⭐⭐ 进阶 | `pwm_chip`、占空比配置、`sysfs` 接口 |
| [Ch12](./ch12_dma/) | `ch12_dma/` | ⭐⭐⭐⭐ 高级 | DMA 映射、一致性内存、`dma_alloc_coherent` |
| [Ch13](./ch13_net_virtual/) | `ch13_net_virtual/` | ⭐⭐⭐⭐ 高级 | `net_device`、`sk_buff`、中断模拟、ARP 处理 |
| [Ch14](./ch14_net_mac_phy/) | `ch14_net_mac_phy/` | ⭐⭐⭐⭐ 高级 | NAPI、虚拟 PHY 状态机、`ethtool` 接口 |
| [Ch15](./ch15_i2c/) | `ch15_i2c/` | ⭐⭐⭐⭐ 高级 | `i2c_adapter`、`i2c_driver`、SMBus 协议 |
| [Ch16](./ch16_spi/) | `ch16_spi/` | ⭐⭐⭐⭐ 高级 | `spi_master`、`spi_driver`、全双工传输 |
| [Ch17](./ch17_block/) | `ch17_block/` | ⭐⭐⭐⭐⭐ 专家 | `gendisk`、`request_queue`、I/O 调度 |
| [Ch18](./ch18_mmc/) | `ch18_mmc/` | ⭐⭐⭐⭐⭐ 专家 | `mmc_host`、卡枚举、CMD 命令集、块 I/O |

---

## 环境要求

```bash
# Ubuntu/Debian
sudo apt-get install linux-headers-$(uname -r) build-essential \
    i2c-tools spi-tools evtest libevdev-dev

# 验证内核头文件
ls /lib/modules/$(uname -r)/build
```

---

## 快速开始

```bash
# 克隆仓库
git clone git@github.com:weijie-lee/linux_device_driver.git
cd linux_device_driver

# 编译所有模块
make all

# 编译单个章节
make ch02_char_basic

# 运行某章节的完整测试
cd ch02_char_basic && sudo bash tests/test.sh

# 清理所有编译产物
make clean
```

---

## 测试体系

每个章节目录下的 `tests/` 子目录包含：

| 文件 | 说明 |
|------|------|
| `test_xxx.c` | 用户态 C 测试程序：验证 open/read/write/ioctl/poll 等接口 |
| `test.sh` | Shell 自动化脚本：加载模块 → 执行测试 → 输出 PASS/FAIL → 卸载模块 |

运行所有章节的测试：

```bash
# 逐章运行测试
for dir in ch0*/; do
    echo "=== Testing $dir ==="
    cd "$dir" && make && sudo bash tests/test.sh; cd ..
done
```

---

## Linux 驱动子系统全景

Linux 内核将驱动按**子系统**组织，每个子系统提供统一的注册接口和抽象层，驱动只需实现规定的回调函数即可接入内核框架。

```
Linux 内核驱动子系统
├── 字符设备层        cdev + file_operations
├── 块设备层          gendisk + request_queue + blk-mq
├── 网络设备层        net_device + net_device_ops + NAPI
├── 总线子系统
│   ├── Platform      platform_driver（SoC 片上外设）
│   ├── I2C           i2c_driver + i2c_adapter
│   ├── SPI           spi_driver + spi_master
│   ├── USB           usb_driver + urb
│   ├── PCI           pci_driver + BAR 映射
│   └── SDIO/MMC      mmc_driver + mmc_host
├── 输入子系统        input_dev + evdev
├── 电源管理子系统
│   ├── Watchdog      watchdog_device
│   ├── RTC           rtc_device + rtc_ops
│   └── PWM           pwm_chip + pwm_ops
├── 框架抽象层
│   ├── Regmap        统一寄存器访问（I2C/SPI/MMIO）
│   ├── IIO           工业 I/O（ADC/DAC/传感器）
│   └── V4L2          视频设备
└── 内存管理
    ├── DMA           dma_alloc_coherent / dma_map_*
    └── IOMMU         DMA 地址转换
```

> 本仓库覆盖加粗标注的子系统（Ch00–Ch18），USB/PCI/IIO/V4L2 等高度专业化的子系统不在本仓库范围内。

---

## 参考资料

1. **《Linux设备驱动开发详解》** — 宋宝华 编著（第4版）
2. **《Linux Device Drivers》（LDD3）** — Alessandro Rubini & Jonathan Corbet，[在线免费版](https://lwn.net/Kernel/LDD3/)
3. **Linux 内核源码** — [https://elixir.bootlin.com](https://elixir.bootlin.com)
4. **内核文档** — `Documentation/` 目录，[在线版](https://www.kernel.org/doc/html/latest/)

---

## 许可证

本仓库代码遵循 **GPL-2.0** 许可证，与 Linux 内核保持一致。
