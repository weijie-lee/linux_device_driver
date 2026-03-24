# Linux 设备驱动开发详解 — 学习实践仓库

[![License: GPL](https://img.shields.io/badge/License-GPL-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Kernel](https://img.shields.io/badge/Kernel-5.15%2F6.x-orange.svg)](https://kernel.org)
[![Build](https://img.shields.io/badge/Build-18%2F18%20PASS-brightgreen.svg)](#编译测试结果)
[![QEMU Test](https://img.shields.io/badge/QEMU%20Test-19%2F19%20PASS-brightgreen.svg)](#qemu-运行时测试结果)

本仓库是跟随《Linux设备驱动开发详解》（宋宝华 编著）系统学习的实践代码库，覆盖 Linux 内核驱动开发的全部主要子系统。所有驱动均基于**内核模拟设备**实现，无需真实硬件即可在标准 Linux 环境中编译、加载和验证。

每个驱动目录均包含：

- **驱动源码**（`.c`）：含详细中文注释，解释每个函数和关键机制
- **测试用例**（`tests/`）：用户态 C 测试程序 + Shell 自动化验证脚本
- **独立 README**：知识点讲解、代码结构说明、完整验证步骤

---

## 学习路径

> **建议按章节编号顺序学习**，每一章都建立在前一章的基础之上。

```
Ch00 框架总览（必读前置）
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
| [Ch00](./ch00_framework/) | `ch00_framework/` | ⭐ 必读 | 驱动框架全景、通用模板、同步原语选择指南 |
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
| [Ch17](./ch17_block/) | `ch17_block/` | ⭐⭐⭐⭐⭐ 专家 | `gendisk`、`blk-mq`、I/O 调度 |
| [Ch18](./ch18_mmc/) | `ch18_mmc/` | ⭐⭐⭐⭐⭐ 专家 | `mmc_host`、卡枚举、CMD 命令集、块 I/O |

---

## 测试环境

### 编译环境（已验证）

| 项目 | 版本 / 说明 |
|------|------------|
| **操作系统** | Ubuntu 22.04.5 LTS (Jammy Jellyfish) |
| **运行内核** | 6.1.102 x86_64（沙箱环境自定义构建） |
| **编译内核头文件** | `linux-headers-5.15.0-173-generic`（`5.15.0-173.183`） |
| **GCC** | 11.4.0（Ubuntu 11.4.0-1ubuntu1~22.04.3） |
| **GNU Make** | 4.3 |
| **GNU Binutils (ld)** | 2.38 |
| **Python** | 3.11.0rc1（用于测试脚本辅助） |
| **CPU** | Intel Xeon @ 2.50GHz，6 核 |
| **内存** | 3.8 GiB（可用 1.5 GiB） |
| **磁盘** | 42 GiB（已用 11 GiB，剩余 31 GiB） |

> **重要说明：** 本仓库所有 `Makefile` 均固定使用 `KERNEL_DIR = /lib/modules/5.15.0-173-generic/build` 进行编译。若在其他机器上编译，请执行以下命令适配本地内核：
> ```bash
> make KERNEL_DIR=/lib/modules/$(uname -r)/build
> ```

### 运行时测试环境要求

模块的 `insmod` / `rmmod` 加载测试需要运行内核与编译内核头文件**版本一致**。当前沙箱运行内核（6.1.102）与编译头文件（5.15.0-173）不同，因此 `tests/test.sh` 中的模块加载步骤需在以下环境中执行：

| 推荐环境 | 说明 |
|---------|------|
| **Ubuntu 22.04 + 标准内核** | `sudo apt install linux-image-5.15.0-173-generic` 后重启 |
| **QEMU 虚拟机** | 使用 `buildroot` 或 `debootstrap` 构建，内核版本与头文件一致 |
| **树莓派 / 开发板** | 安装对应版本内核头文件后可直接运行 |

### 编译测试结果

在上述编译环境中，全部 18 章编译通过（`18/18 PASS`）：

```
✅ ch01_kernel_basics    — kfifo_demo_static.ko
✅ ch02_char_basic       — globalmem.ko
✅ ch03_char_advanced    — globalfifo.ko
✅ ch04_timer            — second.ko
✅ ch05_misc             — misc_demo.ko
✅ ch06_platform         — platform_demo.ko
✅ ch07_input            — input_demo.ko
✅ ch08_regmap           — regmap_demo.ko
✅ ch09_watchdog         — watchdog_demo.ko
✅ ch10_rtc              — rtc_demo.ko
✅ ch11_pwm              — pwm_demo.ko
✅ ch12_dma              — dma_demo.ko
✅ ch13_net_virtual      — snull.ko
✅ ch14_net_mac_phy      — eth_mac.ko
✅ ch15_i2c              — i2c_master.ko + i2c_slave.ko
✅ ch16_spi              — spi_master.ko + spi_slave.ko
✅ ch17_block            — vmem_disk.ko
✅ ch18_mmc              — mmc_virt.ko
```

### QEMU 运行时测试结果

所有 18 个驱动章节均已在 **QEMU x86_64 虚拟机**（内核 5.15.0-173-generic）中完成运行时验证，**19 项测试全部通过（19/19 PASS）**。详细报告见 [TEST_RESULTS.md](./TEST_RESULTS.md)。

| 章节 | 驱动 | 测试结果 | 验证内容 |
|------|------|---------|--------|
| Ch01 | kfifo_demo_static | ✅ PASS | insmod/rmmod 成功 |
| Ch02 | globalmem | ✅ PASS | insmod 成功 |
| Ch03 | globalfifo | ✅ PASS | insmod 成功 |
| Ch04 | second | ✅ PASS | /dev/second 设备节点创建 |
| Ch05 | misc_demo | ✅ PASS | /dev/misc_demo 创建 |
| Ch06 | platform_demo | ✅ PASS | sysfs 条目创建 |
| Ch07 | input_demo | ✅ PASS | /dev/input/eventX 创建 |
| Ch08 | regmap_demo | ✅ PASS | insmod 成功 |
| Ch09 | watchdog_demo | ✅ PASS | /dev/watchdog 创建 |
| Ch10 | rtc_demo | ✅ PASS | /dev/rtc 创建 |
| Ch11 | pwm_demo | ✅ PASS | /sys/class/pwm 条目创建 |
| Ch12 | dma_demo | ✅ PASS | insmod 成功 |
| Ch13 | snull | ✅ PASS | 网络接口注册 |
| Ch14 | eth_mac | ✅ PASS | 网络接口注册 |
| Ch15 | i2c_master + i2c_slave | ✅ PASS | /dev/i2c_virt 创建 |
| Ch16 | spi_master + spi_slave | ✅ PASS | /dev/spi_virt 创建 |
| Ch17 | vmem_disk | ✅ PASS | /dev/vmem_disk 创建 |
| Ch18 | mmc_virt | ✅ PASS | mmc_host 注册 |

### 可选工具（用于运行时验证）

```bash
# 安装调试和验证工具
sudo apt-get install -y \
    i2c-tools \          # i2cdetect / i2cget / i2cset / i2cdump（Ch15）
    evtest \             # 输入事件查看（Ch07）
    ethtool \            # 网卡信息查询（Ch14）
    tcpdump \            # 网络抓包（Ch13/Ch14）
    iproute2 \           # ip link / ip addr（Ch13/Ch14）
    util-linux           # lsblk / fdisk / mkfs（Ch17/Ch18）
```

---

## 快速开始

```bash
# 克隆仓库
git clone git@github.com:weijie-lee/linux_device_driver.git
cd linux_device_driver

# 编译所有模块（使用系统默认内核头文件）
make all

# 或指定内核头文件路径
make KERNEL_DIR=/lib/modules/$(uname -r)/build

# 编译单个章节
make ch02_char_basic

# 运行某章节的完整测试（需内核版本匹配）
cd ch02_char_basic && sudo bash tests/test.sh

# 运行所有章节测试
sudo bash run_all_tests.sh

# 清理所有编译产物
make clean
```

---

## 测试体系

每个章节目录下的 `tests/` 子目录包含：

| 文件 | 说明 |
|------|------|
| `test_xxx.c` | 用户态 C 测试程序：验证 open/read/write/ioctl/poll 等接口，含 PASS/FAIL 断言 |
| `test.sh` | Shell 自动化脚本：编译测试程序 → 加载模块 → 执行测试 → 输出结果 → 卸载模块 |

顶层 `run_all_tests.sh` 一键运行全部 18 章测试并汇总结果。

---

## Linux 驱动子系统全景

Linux 内核将驱动按**子系统**组织，每个子系统提供统一的注册接口和抽象层，驱动只需实现规定的回调函数即可接入内核框架。

```
Linux 内核驱动子系统
├── 字符设备层        cdev + file_operations                    ← Ch02/Ch03/Ch04/Ch05
├── 块设备层          gendisk + blk-mq + request_queue          ← Ch17/Ch18
├── 网络设备层        net_device + net_device_ops + NAPI         ← Ch13/Ch14
├── 总线子系统
│   ├── Platform      platform_driver（SoC 片上外设）            ← Ch06
│   ├── I2C           i2c_driver + i2c_adapter                  ← Ch15
│   ├── SPI           spi_driver + spi_master                   ← Ch16
│   ├── USB           usb_driver + urb                          （未收录）
│   ├── PCI           pci_driver + BAR 映射                     （未收录）
│   └── SDIO/MMC      mmc_driver + mmc_host                     ← Ch18
├── 输入子系统        input_dev + evdev                          ← Ch07
├── 电源管理子系统
│   ├── Watchdog      watchdog_device                           ← Ch09
│   ├── RTC           rtc_device + rtc_ops                      ← Ch10
│   └── PWM           pwm_chip + pwm_ops                        ← Ch11
├── 框架抽象层
│   ├── Regmap        统一寄存器访问（I2C/SPI/MMIO）              ← Ch08
│   ├── IIO           工业 I/O（ADC/DAC/传感器）                 （未收录）
│   └── V4L2          视频设备                                  （未收录）
└── 内存管理
    ├── DMA           dma_alloc_coherent / dma_map_*            ← Ch12
    └── 内核工具       container_of / 链表 / kfifo               ← Ch01
```

> 本仓库覆盖 Ch00–Ch18 共 19 个章节。USB/PCI/IIO/V4L2 等高度专业化的子系统不在本仓库范围内。

---

## 参考资料

1. **《Linux设备驱动开发详解》** — 宋宝华 编著（第4版）
2. **《Linux Device Drivers》（LDD3）** — Alessandro Rubini & Jonathan Corbet，[在线免费版](https://lwn.net/Kernel/LDD3/)
3. **Linux 内核源码在线查阅** — [https://elixir.bootlin.com](https://elixir.bootlin.com)
4. **内核官方文档** — `Documentation/` 目录，[在线版](https://www.kernel.org/doc/html/latest/)
5. **内核 API 参考** — [https://www.kernel.org/doc/html/latest/driver-api/](https://www.kernel.org/doc/html/latest/driver-api/)

---

## 许可证

本仓库代码遵循 **GPL-2.0** 许可证，与 Linux 内核保持一致。
