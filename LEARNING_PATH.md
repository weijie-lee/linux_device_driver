# 学习顺序

`ch01`–`ch68` 按子系统各给一个可加载模块。`ch21` 以后当作索引：要查 GPIO、IIO 或 DRM 时打开对应目录。从入门到能写一块新外设，按下面的顺序，不要按目录编号一路读到 `ch68`。

## 1. 框架和字符设备

1. 读 `ch00_framework`。
2. 做 `ch01_kernel_basics` 到 `ch06_platform`：模块、`container_of`、字符设备、等待队列、定时器、misc、platform。
3. 读 `hardcode/`。数据结构的可运行短例子在 `hardcode_samples.c`，驱动里的用法指向前面的章节。

## 2. 主线

`learn/` 把接口、资源和并发接成一条驱动。QEMU 的 `init.sh` 会跑这些检查，而不只是 `insmod`。

1. `learn/01_uapi`：read/write、ioctl、poll、mmap、sysfs、debugfs。用户态程序是 `uapi_test`。
2. `learn/02_devicetree`：`virt-reg.dts` 里的 `reg`、`interrupts`、`ldd,threshold` 如何变成 `platform_get_resource` 能看见的资源。
3. `learn/03_irq`：上半部、threaded irq、workqueue。自旋锁留在上半部，mutex 留在线程里。
4. `learn/04_soc_sensor`：同一次 probe 里的 regmap、时钟和中断。
5. `learn/05_pm`：runtime PM，以及系统睡眠的 suspend/resume。
6. `learn/06_dma`：一致性内存和流式 DMA。
7. `learn/07_debug`：dyndbg、ftrace，以及 oops 的读法。

这台教学客户机是 x86 的 `5.15.0-173-generic`，内核配置里 `CONFIG_OF` 没有打开，也不能在启动时吃进 dtb。设备树章节因此在模块里填与 dts 相同的 resource。`compatible` 和 `of_match_table` 仍然写在驱动里，换到打开了 OF 的内核时 probe 不用改。

`CONFIG_PROVE_LOCKING` 也没有打开。`learn/03_irq` 用 `in_interrupt()` 和 `in_task()` 区分上下文，不依赖 lockdep 的打印。

## 3. 总线和设备类

主线之后再看具体总线：

- 输入、regmap、看门狗、RTC、PWM：`ch07_input` 到 `ch11_pwm`
- 网络：`ch13_net_virtual`、`ch14_net_mac_phy`
- I2C / SPI：`ch15_i2c`、`ch16_spi`
- 块设备和 MMC：`ch17_block`、`ch18_mmc`
- PCI / USB：`ch19_pci_bus`、`ch20_usb_bus`

`ch12_dma` 只打印 `kmalloc` / `vmalloc` 的地址。DMA API 看 `learn/06_dma`。

## 4. 子系统索引

`ch21_gpio` 到 `ch68_interconnect` 每个目录注册一种虚拟设备。需要某个子系统的注册函数时再打开，不必按顺序学。

## 不在这条路径上的目录

| 目录 | 去看 |
| --- | --- |
| `ch04_regmap_framework` | `ch08_regmap` |
| `ch05_i2c_bus` | `ch15_i2c` |
| `ch07_pci_bus` | `ch19_pci_bus` |
| `ch08_usb_bus` | `ch20_usb_bus` |

这四个目录不参与 `make all`。
