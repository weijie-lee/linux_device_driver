# Linux Device Driver Development Guide

[![License](https://img.shields.io/badge/license-GPL%202.0-blue)](LICENSE)
[![Kernel](https://img.shields.io/badge/kernel-5.15.0--173-orange)](https://www.kernel.org/)

一个面向内核 **5.15** 的 Linux 驱动教学仓库。顶层 `make all` 编译 **68 个子系统章节**（`ch01`–`ch68`）和 `learn/` 里的 7 个主线模块。子系统章节覆盖字符设备、总线、网络、存储，以及 GPIO、时钟、电源、IIO、ALSA、DRM。主线把 ioctl、设备树资源、中断、regmap、时钟、电源管理和 DMA 串成可以在 QEMU 里检查的驱动。加载验证使用 `qemu_test/`，客户机内核是 `5.15.0-173-generic`。阅读顺序见 [LEARNING_PATH.md](LEARNING_PATH.md)。

每个 5.15 `drivers/` 目录要么对应某一章，要么在 [LINUX_DRIVER_COVERAGE.md](LINUX_DRIVER_COVERAGE.md) 里写明为什么不做成外置模块（架构专属、会替换客户机正在使用的驱动、启动早期，或头文件不可链接）。本仓库不拷贝内核 `drivers/` 树。

## 项目特色

**按子系统编排**：章节目录名与内核子系统对应。`ch01`–`ch20` 是字符设备、平台设备、输入、regmap、看门狗、RTC、PWM、DMA、网络、I2C、SPI、块设备、MMC、PCI 和 USB。`ch21`–`ch68` 补齐其余可以安全做成外置模块的子系统。

**对齐 5.15 API**：示例按 Ubuntu `5.15.0-173-generic` 的头文件编写（例如 `class_create` 的两个参数、`pwm_ops.get_state` 返回 `void`、I2C/SPI 的 probe/remove 签名）。宿主机内核更新时，这些 `.ko` 的 vermagic 对不上，不能直接 `insmod`，要用下面的 QEMU 客户机。

**QEMU 加载测试**：`qemu_test/build_rootfs.sh` 把各章 `.ko` 打进 initramfs，并预加载 IIO、ALSA、HID、MTD 等 `=m` 的子系统模块。`qemu_test/init.sh` 在客户机里逐个 `insmod` / `rmmod`。

**每章自带说明**：章节目录里有驱动源码、Makefile、`README.md`，以及架构和流程图说明。

## 快速开始

### 环境要求

在开始之前，请确保系统满足以下要求：

```bash
# 教学模块按 5.15.0-173-generic 编译，不要求宿主机正在运行这个内核
sudo apt-get install -y \
    build-essential \
    linux-headers-5.15.0-173-generic \
    linux-modules-extra-5.15.0-173-generic \
    qemu-system-x86 \
    busybox-static \
    cpio gzip
```

### 编译单个驱动

每个章节都可以独立编译。以 Ch01 kfifo 为例：

```bash
cd ch01_kernel_basics
make
# 生成 kfifo_demo_static.ko
```

### 运行 QEMU 全量测试

项目提供了完整的 QEMU 测试基础设施，可以一键构建和测试所有驱动：

```bash
# 进入项目根目录
cd /path/to/linux_device_driver

# 一键运行全量测试（包括构建 rootfs、启动 QEMU、收集结果）
bash qemu_test/qemu_test_all.sh

# 或分步运行
bash qemu_test/build_rootfs.sh    # 构建 rootfs
bash qemu_test/run_qemu.sh        # 启动 QEMU 并运行测试

# 查看测试结果
cat qemu_test/output/test_summary.txt
```

### 交互式调试

如果需要在 QEMU 中进行交互式调试，可以使用以下命令：

```bash
bash qemu_test/run_qemu.sh --interactive
```

这将启动 QEMU 并连接到串口，您可以在 QEMU 内部手动加载和测试驱动。

## 项目结构

```
linux_device_driver/
├── ch00_framework/            # 驱动框架总览（文档，不编译）
├── ch01_kernel_basics/        # kfifo
├── ch02_char_basic/           # globalmem
├── ch03_char_advanced/        # globalfifo
├── ch04_timer/                # 内核定时器
├── ch05_misc/                 # misc 设备
├── ch06_platform/             # platform 驱动
├── ch07_input/                # 输入子系统
├── ch08_regmap/               # regmap
├── ch09_watchdog/             # 看门狗
├── ch10_rtc/                  # RTC
├── ch11_pwm/                  # PWM
├── ch12_dma/                  # DMA 内存
├── ch13_net_virtual/          # snull
├── ch14_net_mac_phy/          # 以太网 MAC
├── ch15_i2c/                  # I2C
├── ch16_spi/                  # SPI
├── ch17_block/                # 虚拟块设备
├── ch18_mmc/                  # MMC
├── ch19_pci_bus/              # PCI
├── ch20_usb_bus/              # USB
├── ch21_gpio/ … ch68_interconnect/  # 子系统索引，见下表
├── learn/                     # 主线：用户接口、设备树、中断、综合外设、PM、DMA、调试
├── qemu_test/                 # rootfs、QEMU、init
├── hardcode/                  # 数据结构与设计模式，带可编译短例子
├── LEARNING_PATH.md           # 建议阅读顺序
├── LINUX_DRIVER_COVERAGE.md   # drivers/ 目录对照
├── TEST_RESULTS.md            # 最近一次 QEMU 结果
└── README.md
```

不参与 `make all` 的目录：`ch04_regmap_framework`（见 `ch08_regmap`）、`ch05_i2c_bus`（见 `ch15_i2c`）、`ch07_pci_bus` 与 `ch08_usb_bus`（与 `ch19` / `ch20` 同名模块，避免重复编译）。

## 完整章节导航

### 第一部分：内核基础和字符设备（Ch01-Ch06）

这部分介绍 Linux 驱动开发的基础知识，包括内核模块、字符设备、平台驱动等核心概念。

| 章节 | 标题 | 内容 | 难度 |
|------|------|------|------|
| **Ch01** | 内核基础 | 模块加载、kfifo 工具库、动态加载机制 | ⭐ |
| **Ch02** | 字符设备基础 | 设备节点、read/write 操作、全局内存驱动 | ⭐ |
| **Ch03** | 字符设备高阶 | FIFO 缓冲区、等待队列、异步通知 | ⭐⭐ |
| **Ch04** | 内核定时器 | timer 框架、延时操作、周期性任务 | ⭐⭐ |
| **Ch05** | misc 设备 | misc 框架、简化设备注册、自动设备节点 | ⭐ |
| **Ch06** | 平台驱动 | platform 框架、设备树绑定、资源管理 | ⭐⭐ |

### 第二部分：设备类与总线（Ch07-Ch20）

| 章节 | 目录 | 模块 | 内容 |
|------|------|------|------|
| **Ch07** | `ch07_input` | `input_demo` | 输入事件 |
| **Ch08** | `ch08_regmap` | `regmap_demo` | 寄存器映射 |
| **Ch09** | `ch09_watchdog` | `watchdog_demo` | 看门狗 |
| **Ch10** | `ch10_rtc` | `rtc_demo` | 实时时钟 |
| **Ch11** | `ch11_pwm` | `pwm_demo` | PWM |
| **Ch12** | `ch12_dma` | `dma_demo` | DMA 一致性内存 |
| **Ch13** | `ch13_net_virtual` | `snull` | 虚拟网卡 |
| **Ch14** | `ch14_net_mac_phy` | `eth_mac` | 以太网 MAC 与 NAPI |
| **Ch15** | `ch15_i2c` | `i2c_master`, `i2c_slave` | I2C 控制器与从设备 |
| **Ch16** | `ch16_spi` | `spi_master`, `spi_slave` | SPI 控制器与从设备 |
| **Ch17** | `ch17_block` | `vmem_disk` | 虚拟块设备 |
| **Ch18** | `ch18_mmc` | `mmc_virt` | MMC 宿主 |
| **Ch19** | `ch19_pci_bus` | `pci_basic`, `pci_advanced` | PCI |
| **Ch20** | `ch20_usb_bus` | `usb_basic`, `usb_advanced` | USB |

### 主线（learn/）

做完 Ch01–Ch06 和 [hardcode/](hardcode/README.md) 之后读这里。`ch21`–`ch68` 是子系统索引，不是第 21 课。细节和本客户机的限制（没有 `CONFIG_OF`、没有 lockdep）写在 [LEARNING_PATH.md](LEARNING_PATH.md)。

| 目录 | 模块 | 内容 |
|------|------|------|
| `learn/01_uapi` | `virt_uapi` | read/write、ioctl、poll、mmap、sysfs、debugfs |
| `learn/02_devicetree` | `virt_dt` | 与 `virt-reg.dts` 对应的 platform 资源 |
| `learn/03_irq` | `virt_irq` | 硬中断、threaded irq、workqueue |
| `learn/04_soc_sensor` | `virt_sensor` | regmap、时钟、中断同一次 probe |
| `learn/05_pm` | `virt_pm` | runtime PM 和 suspend/resume |
| `learn/06_dma` | `virt_dma` | 一致性内存和流式 DMA |
| `learn/07_debug` | `virt_debug` | dyndbg 和 ftrace |

### 第三部分：其余子系统（Ch21-Ch68）

每章注册一个虚拟设备或子系统对象，模块名与目录对应。完整的内核目录对照见 [LINUX_DRIVER_COVERAGE.md](LINUX_DRIVER_COVERAGE.md)。

| 章节 | 子系统 | 模块 | 章节 | 子系统 | 模块 |
|------|--------|------|------|--------|------|
| Ch21 | GPIO | `virt_gpio` | Ch45 | rfkill | `virt_rfkill` |
| Ch22 | pinctrl | `virt_pinctrl` | Ch46 | hwspinlock | `virt_hwspin` |
| Ch23 | clk | `virt_clk` | Ch47 | mailbox | `virt_mbox` |
| Ch24 | regulator | `virt_regulator` | Ch48 | powercap | `virt_powercap` |
| Ch25 | reset | `virt_reset` | Ch49 | OPP | `virt_opp` |
| Ch26 | nvmem | `virt_nvmem` | Ch50 | MDIO | `virt_mdio` |
| Ch27 | hwmon | `virt_hwmon` | Ch51 | IIO | `virt_iio` |
| Ch28 | LED | `virt_led` | Ch52 | ALSA | `virt_alsa` |
| Ch29 | thermal | `virt_thermal` | Ch53 | HID | `virt_hid` |
| Ch30 | power supply | `virt_psy` | Ch54 | MTD | `virt_mtd` |
| Ch31 | backlight | `virt_backlight` | Ch55 | UIO | `virt_uio` |
| Ch32 | PHY | `virt_phy` | Ch56 | 1-Wire | `virt_w1` |
| Ch33 | PPS | `virt_pps` | Ch57 | CAN | `virt_can` |
| Ch34 | PTP | `virt_ptp` | Ch58 | mux | `virt_mux` |
| Ch35 | TTY | `virt_tty` | Ch59 | FPGA | `virt_fpga` |
| Ch36 | fbdev | `virt_fb` | Ch60 | SPMI | `virt_spmi` |
| Ch37 | MFD | `virt_mfd` | Ch61 | I3C | `virt_i3c` |
| Ch38 | extcon | `virt_extcon` | Ch62 | GNSS | `virt_gnss` |
| Ch39 | SCSI | `virt_scsi` | Ch63 | NFC | `virt_nfc` |
| Ch40 | crypto | `virt_crypto` | Ch64 | Bluetooth | `virt_bt` |
| Ch41 | virtio | `virt_virtio` | Ch65 | V4L2 | `virt_v4l2` |
| Ch42 | remoteproc | `virt_rproc` | Ch66 | counter | `virt_counter` |
| Ch43 | devfreq | `virt_devfreq` | Ch67 | DRM | `virt_drm` |
| Ch44 | dmaengine | `virt_dmaeng` | Ch68 | interconnect | `virt_icc` |

## QEMU 测试基础设施

### 概述

`qemu_test/` 在 `5.15.0-173-generic` 客户机里加载各章模块。结果以最近一次运行为准，写在 [TEST_RESULTS.md](TEST_RESULTS.md) 和 `qemu_test/output/test_summary.txt`。

### 工作流程

```
build_rootfs.sh
    ↓
编译所有驱动 → 构建 busybox rootfs → 打包 initramfs
    ↓
run_qemu.sh
    ↓
启动 QEMU → 加载驱动 → 执行测试 → 收集结果
    ↓
test_summary.txt
    ↓
显示测试结果（PASS/FAIL）
```

### 测试结果

加载测试只在 `5.15.0-173-generic` 客户机里进行。最近一次运行记录在 [TEST_RESULTS.md](TEST_RESULTS.md)。IIO、ALSA、HID、MTD、UIO、FPGA、1-Wire、CAN、mux、GNSS、NFC、Bluetooth、V4L2、counter、I3C、SPMI、DRM 在该内核上是模块，`build_rootfs.sh` 会按 `modinfo` 依赖把它们放进 `preload/`。`learn/01_uapi` 的用户态程序静态链接进 initramfs。默认等待时间是 600 秒。

### 自定义测试

如果需要修改测试逻辑或添加新的测试用例，可以编辑 `qemu_test/init.sh` 文件。该文件是 QEMU 内部运行的初始化脚本，负责加载驱动、执行测试和收集结果。

## 编译和安装

### 编译单个驱动

```bash
cd ch<number>_<name>
make
# 或指定内核源码位置
make KERNEL_DIR=/lib/modules/$(uname -r)/build
```

### 编译所有驱动

在仓库根目录执行。顶层 Makefile 用绝对路径调用 kbuild，避免子目录 Makefile 在 `make -C` 下看错 `M=`。

```bash
make KERNEL_DIR=/lib/modules/5.15.0-173-generic/build all
```

### 在 5.15 内核上加载驱动

`.ko` 的 vermagic 是 `5.15.0-173-generic`。在这版内核上（或在本仓库的 QEMU 客户机里）：

```bash
sudo insmod ch01_kernel_basics/kfifo_demo_static.ko
lsmod | grep kfifo
sudo rmmod kfifo_demo_static
dmesg | tail -20
```

## 代码质量

### 编译检查

驱动按 5.15 头文件编译，并在 QEMU 客户机里做加载和卸载。内核构建使用的编译器版本可能和宿主机 gcc 不同，kbuild 会提示这一差异，它不表示模块 API 用错。

### 编码规范

所有代码都遵循 Linux 内核编码规范（Linux Kernel Coding Style），包括：

- 使用 4 空格缩进
- 行长度不超过 100 字符
- 函数注释采用内核文档格式
- 错误处理完善，无内存泄漏

## 学习路径

顺序写在 [LEARNING_PATH.md](LEARNING_PATH.md)：`ch00` → `ch01`–`ch06` → `hardcode/` → `learn/01`–`learn/07` → `ch07`–`ch20`。`ch21`–`ch68` 按子系统查阅。`ch04_regmap_framework`、`ch05_i2c_bus`、`ch07_pci_bus`、`ch08_usb_bus` 不在这条路径上，也不参与 `make all`。

## 常见问题

### Q: 如何在我的系统上运行 QEMU 测试？

A: 首先确保安装了 QEMU 和相关依赖，然后运行 `bash qemu_test/qemu_test_all.sh`。脚本会自动检查依赖、编译驱动、构建 rootfs、启动 QEMU 并收集测试结果。

### Q: 驱动编译失败，提示找不到内核头文件？

A: 请确保安装了对应内核版本的 linux-headers 包。例如，对于 Ubuntu 系统：
```bash
sudo apt-get install linux-headers-$(uname -r)
```

### Q: 如何在 QEMU 中调试驱动？

A: 可以使用 `bash qemu_test/run_qemu.sh --interactive` 启动交互式 QEMU，然后在 QEMU 内部手动加载驱动并查看日志。

### Q: 某个驱动加载失败，如何排查问题？

A: 可以查看 QEMU 的串口输出日志：
```bash
cat qemu_test/output/qemu_serial.log
```
或在 QEMU 内部运行 `dmesg` 查看内核日志。

### Q: 如何为项目贡献新的驱动或改进？

A: 请参考 [CONTRIBUTING.md](CONTRIBUTING.md) 文件了解贡献指南。

## 贡献者

本项目由以下贡献者共同完成：

| 贡献者 | 角色 | 贡献内容 |
|--------|------|---------|
| **weijie-lee** | 项目创建者 | 项目架构、驱动代码、QEMU 测试基础设施 |
| **Manus AI** | 技术顾问 | 代码审查、文档编写、项目重新编排 |

## 许可证

本项目采用 **GPL 2.0** 许可证。详见 [LICENSE](LICENSE) 文件。

## 参考资源

学习 Linux 驱动开发的推荐资源：

- **Linux Kernel Documentation**：https://www.kernel.org/doc/
- **Linux Device Drivers, 3rd Edition**：https://lwn.net/Kernel/LDD3/
- **Linux Kernel Module Programming Guide**：https://sysprog21.github.io/lkmpg/
- **The Linux Kernel Archives**：https://www.kernel.org/

## 联系方式

如有问题或建议，欢迎通过以下方式联系：

- **GitHub Issues**：https://github.com/weijie-lee/linux_device_driver/issues
- **GitHub Discussions**：https://github.com/weijie-lee/linux_device_driver/discussions

---

**最后更新**：2026 年 10 月 21 日  
**内核目标**：5.15.0-173-generic  
**章节**：ch01–ch68 与 learn/ 参与 `make all`
