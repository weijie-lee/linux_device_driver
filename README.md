# Linux Device Driver Development Guide

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/weijie-lee/linux_device_driver)
[![Tests](https://img.shields.io/badge/tests-22%2F22%20passing-brightgreen)](qemu_test/)
[![License](https://img.shields.io/badge/license-GPL%202.0-blue)](LICENSE)
[![Kernel](https://img.shields.io/badge/kernel-5.15.0%2B-orange)](https://www.kernel.org/)

一个**系统化、实践化**的 Linux 驱动开发学习项目，涵盖从内核基础到高级驱动子系统的完整知识体系。本项目包含 **20 个循序渐进的章节**，每章从基础到高阶逐步深入，配合 **QEMU 虚拟机模拟环境**，确保所有驱动代码都能在真实内核中运行和测试。

## 项目特色

**系统化的知识体系**：按照 Linux 驱动子系统的实际组织方式编排，涵盖总线子系统（PCI、USB、I2C、SPI）、存储子系统（块设备、MMC）、网络子系统、输入子系统等，帮助开发者理解驱动框架的全景。

**从基础到高阶的递进设计**：每个章节都包含基础、中阶、高阶三个层次的内容。初学者可以从基础开始，逐步掌握驱动开发的核心概念；中级开发者可以直接跳转到高阶内容，学习优化技巧和最佳实践。

**完整的 QEMU 测试基础设施**：提供自动化的 rootfs 构建、QEMU 启动和测试脚本，所有 22 个驱动模块都在 QEMU 虚拟机中验证通过（PASS: 22/22），确保代码的正确性和可移植性。

**生产级代码质量**：所有代码都经过 Copilot AI 代码审查，编译时无任何警告，错误处理完善，符合 Linux 内核编码规范。

**详细的文档和注释**：每个驱动都包含详细的功能说明、编译指南、测试方法和常见问题解答，适合自学和教学。

## 快速开始

### 环境要求

在开始之前，请确保系统满足以下要求：

```bash
# Ubuntu 22.04 LTS 或更高版本
# 安装必要的依赖
sudo apt-get install -y \
    build-essential \
    linux-headers-$(uname -r) \
    qemu-system-x86 \
    busybox-static \
    cpio gzip
```

### 编译单个驱动

每个章节都可以独立编译。以 Ch01 kfifo 为例：

```bash
cd ch01_kernel_basics
make
# 生成 kfifo.ko
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
├── ch01_kernel_basics/        # Ch01: 内核基础（kfifo、动态加载）
├── ch02_char_device/          # Ch02: 字符设备基础（globalmem）
├── ch03_char_device_advanced/ # Ch03: 字符设备高阶（globalfifo、FIFO）
├── ch04_timer/                # Ch04: 内核定时器（second 驱动）
├── ch05_misc_device/          # Ch05: misc 设备框架
├── ch06_platform_driver/      # Ch06: 平台驱动框架
├── ch07_pci_bus/              # Ch07: PCI 总线驱动（基础 + 高阶）
├── ch08_usb_bus/              # Ch08: USB 总线驱动（基础 + 高阶）
├── ch09_input_subsystem/      # Ch09: 输入子系统
├── ch10_regmap_framework/     # Ch10: regmap 寄存器映射框架
├── ch11_watchdog/             # Ch11: 看门狗设备
├── ch12_rtc/                  # Ch12: 实时时钟（RTC）
├── ch13_pwm/                  # Ch13: PWM 控制器
├── ch14_dma/                  # Ch14: DMA 内存管理
├── ch15_net_virtual/          # Ch15: 虚拟网络接口（snull）
├── ch16_net_mac_phy/          # Ch16: 以太网 MAC/PHY 驱动
├── ch17_i2c/                  # Ch17: I2C 总线驱动
├── ch18_spi/                  # Ch18: SPI 总线驱动
├── ch19_block_device/         # Ch19: 虚拟块设备（vmem_disk）
├── ch20_mmc/                  # Ch20: MMC/eMMC 卡驱动
├── qemu_test/                 # QEMU 测试基础设施
│   ├── build_rootfs.sh        # 构建 rootfs 脚本
│   ├── run_qemu.sh            # 启动 QEMU 脚本
│   ├── qemu_test_all.sh       # 一键测试入口
│   ├── init.sh                # QEMU 内 init 脚本
│   ├── README.md              # QEMU 测试说明
│   └── output/                # 构建产物目录
├── TEST_RESULTS.md            # QEMU 测试结果报告
├── REORGANIZATION_PLAN.md     # 项目重新编排计划
├── COPILOT_REVIEW_FIXES.md    # Copilot AI Code Review 修复总结
└── README.md                  # 本文件
```

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

### 第二部分：总线子系统（Ch07-Ch10）

这部分深入讲解 Linux 中最重要的几个总线子系统，包括 PCI、USB、I2C 和 SPI。

| 章节 | 标题 | 内容 | 难度 |
|------|------|------|------|
| **Ch07** | PCI 总线 | 设备枚举、BAR 映射、中断处理、DMA 传输 | ⭐⭐⭐ |
| **Ch08** | USB 总线 | 设备枚举、URB 管理、异步传输、电源管理 | ⭐⭐⭐ |
| **Ch09** | I2C 总线 | 主从驱动、时序控制、多主机支持 | ⭐⭐ |
| **Ch10** | SPI 总线 | 同步传输、DMA 支持、高速通信 | ⭐⭐ |

### 第三部分：设备驱动（Ch11-Ch16）

这部分介绍各种常见的设备驱动，包括输入设备、网络设备、存储设备等。

| 章节 | 标题 | 内容 | 难度 |
|------|------|------|------|
| **Ch11** | 输入子系统 | 键盘、鼠标、触摸屏、事件上报 | ⭐⭐ |
| **Ch12** | regmap 框架 | 寄存器映射、多种总线支持、缓存机制 | ⭐⭐ |
| **Ch13** | 看门狗 | 喂狗机制、超时处理、系统恢复 | ⭐ |
| **Ch14** | RTC 驱动 | 时间管理、闹钟、休眠唤醒 | ⭐⭐ |
| **Ch15** | PWM 控制 | 脉宽调制、频率调节、占空比控制 | ⭐⭐ |
| **Ch16** | 网络驱动 | MAC/PHY 驱动、以太网通信、NAPI 优化 | ⭐⭐⭐ |

### 第四部分：存储和高级主题（Ch17-Ch20）

这部分介绍块设备驱动、MMC 驱动等存储相关的高级主题。

| 章节 | 标题 | 内容 | 难度 |
|------|------|------|------|
| **Ch17** | DMA 框架 | DMA 缓冲区、一致性映射、IOMMU 支持 | ⭐⭐⭐ |
| **Ch18** | 虚拟网络 | snull 驱动、虚拟接口、网络栈集成 | ⭐⭐ |
| **Ch19** | 块设备 | 虚拟磁盘、分区表、文件系统支持 | ⭐⭐⭐ |
| **Ch20** | MMC 驱动 | SD 卡枚举、高速模式、电源管理 | ⭐⭐⭐ |

## QEMU 测试基础设施

### 概述

项目提供了完整的 QEMU 测试基础设施，包括自动化的 rootfs 构建、驱动加载、测试执行和结果收集。所有 22 个驱动模块都在 QEMU 虚拟机中验证通过。

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

当前测试结果：**PASS: 22/22，FAIL: 0**

所有驱动模块都在 QEMU x86_64 虚拟机（内核 5.15.0-173-generic）中成功加载和测试。详细的测试结果请参考 [TEST_RESULTS.md](TEST_RESULTS.md)。

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

```bash
# 在项目根目录运行
for dir in ch*/; do
    cd "$dir"
    make
    cd ..
done
```

### 在真实系统中加载驱动

```bash
# 加载驱动
sudo insmod ch01_kernel_basics/kfifo.ko

# 查看驱动加载情况
lsmod | grep kfifo

# 卸载驱动
sudo rmmod kfifo

# 查看驱动日志
dmesg | tail -20
```

## 代码质量

### 编译检查

所有驱动代码都经过以下检查：

- ✅ **编译无警告**：使用 `-Wall -Wextra` 编译，确保代码质量
- ✅ **代码审查**：经过 Copilot AI 代码审查，修复了 22 条 code review comments
- ✅ **运行时测试**：在 QEMU 虚拟机中完整测试，确保功能正确性

### 编码规范

所有代码都遵循 Linux 内核编码规范（Linux Kernel Coding Style），包括：

- 使用 4 空格缩进
- 行长度不超过 100 字符
- 函数注释采用内核文档格式
- 错误处理完善，无内存泄漏

## 学习路径

### 初学者路径

如果您是 Linux 驱动开发的初学者，建议按以下顺序学习：

1. **第 1-2 周**：Ch01-Ch03（内核基础和字符设备）
   - 理解模块加载机制
   - 掌握字符设备的基本操作
   - 学习等待队列和异步通知

2. **第 3-4 周**：Ch04-Ch06（定时器、misc 设备、平台驱动）
   - 学习内核定时器的使用
   - 理解平台驱动框架
   - 了解设备树的基本概念

3. **第 5-8 周**：Ch07-Ch10（总线子系统）
   - 深入理解 PCI 和 USB 总线
   - 掌握 I2C 和 SPI 的驱动开发
   - 学习中断处理和 DMA 传输

### 中级开发者路径

如果您已经有一定的驱动开发经验，可以选择性地学习：

1. **PCI 和 USB 驱动**：Ch07-Ch08
   - 学习高级的中断处理
   - 掌握 DMA 传输优化
   - 理解电源管理机制

2. **网络驱动**：Ch16-Ch18
   - 深入理解网络驱动框架
   - 学习 NAPI 优化技巧
   - 掌握虚拟网络接口的实现

3. **存储驱动**：Ch19-Ch20
   - 学习块设备驱动的实现
   - 掌握 MMC 驱动的开发
   - 理解文件系统与驱动的交互

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

**最后更新**：2026 年 3 月 25 日  
**项目版本**：1.0.0（第一阶段完成）  
**内核支持**：5.15.0+  
**QEMU 版本**：6.2.0+
