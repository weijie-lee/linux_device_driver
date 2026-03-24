# Linux Device Driver — QEMU 测试报告

## 测试环境

| 项目 | 详情 |
|------|------|
| 宿主机 OS | Ubuntu 22.04.5 LTS (x86_64) |
| 宿主机内核 | 6.1.102 |
| 编译内核头文件 | linux-headers-5.15.0-173-generic |
| 编译器 | GCC 11.4.0 |
| 构建系统 | GNU Make 4.3 + Kbuild |
| QEMU 版本 | qemu-system-x86_64 (QEMU emulator) |
| 测试内核 | Linux 5.15.0-173-generic |
| 测试架构 | x86_64 |
| 测试时间 | 2026-03-24 |

## 测试结果汇总

**总计：19 项测试，19 项通过，0 项失败（100% 通过率）**

| 章节 | 驱动名称 | 测试项 | 结果 | 验证内容 |
|------|---------|--------|------|---------|
| Ch01 | kfifo_demo_static | ch01_kfifo insmod+rmmod | **PASS** | 内核 kfifo 数据结构，insmod/rmmod 成功 |
| Ch02 | globalmem | ch02_globalmem insmod | **PASS** | 全局内存字符设备，insmod 成功 |
| Ch03 | globalfifo | ch03_globalfifo insmod | **PASS** | 全局 FIFO 字符设备，insmod 成功 |
| Ch04 | second | ch04_second dev=/dev/second | **PASS** | 内核定时器，/dev/second 设备节点创建成功 |
| Ch05 | misc_demo | ch05_misc dev=/dev/misc_demo | **PASS** | Misc 设备框架，/dev/misc_demo 创建成功 |
| Ch06 | platform_demo | ch06_platform sysfs | **PASS** | Platform 总线驱动，sysfs 条目创建成功 |
| Ch07 | input_demo | ch07_input event_dev | **PASS** | Input 子系统，/dev/input/eventX 创建成功 |
| Ch08 | regmap_demo | ch08_regmap insmod | **PASS** | Regmap 抽象层，insmod 成功 |
| Ch09 | watchdog_demo | ch09_watchdog dev | **PASS** | 看门狗框架，/dev/watchdog 设备创建成功 |
| Ch10 | rtc_demo | ch10_rtc dev | **PASS** | RTC 实时时钟，/dev/rtc 设备创建成功 |
| Ch11 | pwm_demo | ch11_pwm sysfs | **PASS** | PWM 子系统，/sys/class/pwm 条目创建成功 |
| Ch12 | dma_demo | ch12_dma insmod | **PASS** | DMA 内存管理，insmod 成功 |
| Ch13 | snull | ch13_snull netdev | **PASS** | 虚拟网络设备，网络接口注册成功 |
| Ch14 | eth_mac | ch14_eth_mac netdev | **PASS** | MAC+PHY 以太网，网络接口注册成功 |
| Ch15 | i2c_master + i2c_slave | ch15_i2c slave_dev | **PASS** | I2C 总线，/dev/i2c_virt 设备创建成功 |
| Ch16 | spi_master + spi_slave | ch16_spi slave_dev | **PASS** | SPI 总线，/dev/spi_virt 设备创建成功 |
| Ch17 | vmem_disk | ch17_vmem_disk dev | **PASS** | 块设备（blk-mq），/dev/vmem_disk 创建成功 |
| Ch18 | mmc_virt | ch18_mmc host | **PASS** | MMC/eMMC 虚拟主控，mmc_host 注册成功 |

## 测试过程中发现并修复的 Bug

### Bug 1：Ch10 RTC — `rtc_class_ops` 回调中的 NULL 指针解引用

**症状：** `insmod rtc_demo.ko` 时内核 Oops，崩溃于 `virt_rtc_read_time+0x34`，RBX 寄存器为 0（NULL 指针）。

**根本原因：** `rtc_class_ops` 回调函数接收的 `dev` 参数是 `rtc_device->dev`，而非 `platform_device->dev`。原代码通过 `dev_get_drvdata(dev)` 获取 `priv`，但 `dev_set_drvdata` 设置的是 `platform_device->dev` 的 drvdata，导致从 `rtc_device->dev` 获取到 NULL。

此外，`devm_rtc_register_device()` 在注册过程中会立即调用 `read_time` 读取当前时间，而此时 `g_priv` 尚未被赋值（原代码将 `g_priv = priv` 置于 `devm_rtc_register_device` 之后），形成先鸡后蛋的竞态。

**修复方案：**
1. 将 `g_priv = priv` 移到 `devm_rtc_register_device()` 调用之前。
2. 在所有 `rtc_class_ops` 回调中直接使用全局变量 `g_priv` 获取 `priv`，避免通过 `dev` 间接获取的路径问题。

### Bug 2：Ch18 MMC — `mmc_alloc_host` 传入 NULL device 导致崩溃

**症状：** `insmod mmc_virt.ko` 时内核 Oops，崩溃于 `devm_kmalloc`，调用栈为 `mmc_gpio_alloc → mmc_alloc_host → mmc_virt_init`。

**根本原因：** `mmc_alloc_host(sizeof(*priv), NULL)` 的第二个参数传入了 NULL。在 Linux 5.15 内核中，`mmc_alloc_host` 内部会调用 `mmc_gpio_alloc`，后者使用 `devm_kmalloc(dev, ...)` 进行内存分配，当 `dev` 为 NULL 时导致 NULL 指针解引用。

**修复方案：** 在 `mmc_virt_init` 中先创建一个虚拟 `platform_device` 作为 parent device，然后将其 `&pdev->dev` 传给 `mmc_alloc_host`，并在 `mmc_virt_exit` 中同步卸载该 `platform_device`。

## QEMU 测试日志摘录

```
============================================
 Linux Driver Module Test Suite
 Kernel: 5.15.0-173-generic
============================================
--- Ch01: kfifo ---
[PASS] ch01_kfifo insmod+rmmod
--- Ch02: globalmem ---
[PASS] ch02_globalmem insmod
--- Ch03: globalfifo ---
[PASS] ch03_globalfifo insmod
--- Ch04: second ---
[PASS] ch04_second dev=/dev/second
--- Ch05: misc ---
[PASS] ch05_misc dev=/dev/misc_demo
--- Ch06: platform ---
[PASS] ch06_platform sysfs
--- Ch07: input ---
[PASS] ch07_input event_dev
--- Ch08: regmap ---
[PASS] ch08_regmap insmod
--- Ch09: watchdog ---
[PASS] ch09_watchdog dev
--- Ch10: rtc ---
[PASS] ch10_rtc dev
--- Ch11: pwm ---
[PASS] ch11_pwm sysfs
--- Ch12: dma ---
[PASS] ch12_dma insmod
--- Ch13: snull ---
[PASS] ch13_snull netdev
--- Ch14: eth_mac ---
[PASS] ch14_eth_mac netdev
--- Ch15: i2c ---
[PASS] ch15_i2c slave_dev
--- Ch16: spi ---
[PASS] ch16_spi slave_dev
--- Ch17: vmem_disk ---
[PASS] ch17_vmem_disk dev
[PASS] ch17_vmem_disk sysfs
--- Ch18: mmc ---
[PASS] ch18_mmc host

============================================
 TEST RESULTS SUMMARY
============================================
 PASS: 19
 FAIL: 0
 TOTAL: 19
============================================
QEMU_TEST_DONE
```
