# Linux Device Driver Development Guide

[![License](https://img.shields.io/badge/license-GPL%202.0-blue)](LICENSE)
[![Kernel](https://img.shields.io/badge/kernel-5.15.0--173-orange)](https://www.kernel.org/)

面向内核 **5.15** 的 Linux 驱动教学仓库。目标是从模块和字符设备走到能写一块小的 SoC 外设：用户接口、设备树资源、中断、regmap、时钟、电源管理和 DMA 串在同一条线上，并在 QEMU 客户机里检查。

仓库有两层：

| 层 | 目录 | 怎么用 |
| --- | --- | --- |
| 主线 | `ch00`、`ch01`–`ch06`、`hardcode/`、`learn/`、`ch07`–`ch20` | 按 [下面的顺序](#学习顺序) 读和做 |
| 索引 | `ch21`–`ch68` | 要查某个子系统的注册函数时再打开，不必按编号学完 |

顶层 `make all` 编译 `ch01`–`ch68`、`hardcode/` 和 `learn/` 的 7 个模块。每个 5.15 `drivers/` 目录要么对应某一章，要么在 [LINUX_DRIVER_COVERAGE.md](LINUX_DRIVER_COVERAGE.md) 里说明为什么不做成外置模块。本仓库不拷贝内核 `drivers/` 树。

客户机内核是 Ubuntu `5.15.0-173-generic`。最近一次 QEMU 运行是 **78 项通过、0 项失败**，记录在 [TEST_RESULTS.md](TEST_RESULTS.md)。

## 读之前要知道的三件事

**模块只能加载到 5.15.0-173-generic。** 示例按这套头文件编写（例如 `class_create` 的两个参数、`pwm_ops.get_state` 返回 `void`、I2C/SPI 的 probe/remove 签名）。宿主机若是更新的内核，vermagic 对不上，`insmod` 会被拒绝。加载和实验放在 `qemu_test/` 的客户机里。

**这台客户机没有设备树运行时。** `CONFIG_OF` 未打开，启动参数里的 dtb 不会变成 `platform_device`。`learn/02_devicetree` 的 `virt-reg.dts` 用 `dtc` 检查语法；模块按同一份 `reg`、中断号和 `ldd,threshold` 填 `struct resource`。驱动里仍有 `of_match_table`，换到打开了 OF 的内核时，probe 从设备树绑定。

**这台客户机没有 lockdep。** `CONFIG_PROVE_LOCKING` 和 `CONFIG_DEBUG_ATOMIC_SLEEP` 都未打开。`learn/03_irq` 用 `in_interrupt()` / `in_task()` 区分上半部和线程，自旋锁留在上半部，mutex 留在线程里。

## 环境与运行

```bash
sudo apt-get install -y \
    build-essential \
    linux-headers-5.15.0-173-generic \
    linux-modules-extra-5.15.0-173-generic \
    qemu-system-x86 \
    busybox-static \
    cpio gzip \
    device-tree-compiler
```

`device-tree-compiler` 用来编译 `learn/02_devicetree/virt-reg.dts` 和 `learn/04_soc_sensor/virt-sensor.dts`。教学模块不要求宿主机正在运行 5.15。

编译一章，或编译全部：

```bash
cd ch01_kernel_basics && make
# 仓库根目录
make KERNEL_DIR=/lib/modules/5.15.0-173-generic/build all
```

顶层 Makefile 用绝对路径调用 kbuild，子目录里的 `M=` 因此指向该章的绝对路径。kbuild 提示「编译器与构建内核时的 gcc 不同」时，表示宿主机 gcc 比构建这套 5.15 内核时更新；模块仍按 `5.15.0-173-generic` 的头文件编译。

QEMU 全量测试会编译模块、打 initramfs、在客户机里跑 `qemu_test/init.sh`：

```bash
bash qemu_test/qemu_test_all.sh
# 或
bash qemu_test/build_rootfs.sh
bash qemu_test/run_qemu.sh
cat qemu_test/output/test_summary.txt
```

默认等待 600 秒。IIO、ALSA、HID、MTD、UIO、FPGA、1-Wire、CAN、mux、GNSS、NFC、Bluetooth、V4L2、counter、I3C、SPMI、DRM 在该内核上是模块，`build_rootfs.sh` 按 `modinfo` 依赖把它们放进 `preload/`。`learn/01_uapi/uapi_test` 静态链接进 initramfs，因为客户机里没有动态装载器。

交互式串口：

```bash
bash qemu_test/run_qemu.sh --interactive
```

加载失败时看 `qemu_test/output/qemu_serial.log`，或在客户机里看 `dmesg`。dyndbg、ftrace 和 oops 的读法在 `learn/07_debug`。kgdb 需要在 QEMU 参数里另加 `-s -S`，普通测试脚本不加这个桩。

## 项目结构

```
linux_device_driver/
├── ch00_framework/            # 框架总览，只读，不编译
├── ch01_kernel_basics/        # container_of、链表、kfifo
├── ch02_char_basic/           # globalmem：cdev、read/write
├── ch03_char_advanced/        # globalfifo：阻塞、poll、fasync、ioctl
├── ch04_timer/                # timer_list
├── ch05_misc/                 # misc 设备
├── ch06_platform/             # platform 总线和 probe
├── hardcode/                  # 数据结构与设计模式手册
├── learn/                     # 主线七章，QEMU 会检查接口
├── ch07_input/ … ch20_usb_bus/  # 总线和设备类
├── ch21_gpio/ … ch68_interconnect/  # 子系统索引
├── qemu_test/                 # rootfs、QEMU、init.sh
├── LEARNING_PATH.md           # 顺序摘要
├── LINUX_DRIVER_COVERAGE.md   # drivers/ 目录对照
└── TEST_RESULTS.md            # 最近一次 QEMU 结果
```

不参与 `make all` 的目录：`ch04_regmap_framework`（实现在 `ch08_regmap`）、`ch05_i2c_bus`（实现在 `ch15_i2c`）、`ch07_pci_bus` 与 `ch08_usb_bus`（与 `ch19` / `ch20` 同名，避免编出两份模块）。

## 学习顺序

短表在 [LEARNING_PATH.md](LEARNING_PATH.md)。下面是每一步要看懂的东西，以及做完之后客户机里能看到什么。

### 1. 先看地图：`ch00_framework`

这一章没有模块。它把后面会反复出现的骨架放在一处：字符设备、platform、总线—设备—驱动、网络设备、块设备、中断、内存、模块生命周期、调试。先读第 1–4 节和第 7–9 节，网络和块设备可以留到 `ch13`、`ch17`。

### 2. 字符设备到 platform：`ch01`–`ch06`

| 章节 | 看什么 | 做完能确认什么 |
| --- | --- | --- |
| `ch01_kernel_basics` | `container_of`、侵入式 `list_head`、`kfifo`。节点嵌在业务结构体里，遍历得到的是节点指针，再用偏移找回对象 | `kfifo_demo_static.ko` 能加载卸载 |
| `ch02_char_basic` | `alloc_chrdev_region`、`cdev`、`file_operations`、`copy_to_user` / `copy_from_user`、mutex。`globalmem` 是一块 4KB 内存 | 对 `/dev/globalmem*` 写入后再读回 |
| `ch03_char_advanced` | FIFO 空则读者睡在等待队列上，有数据则 `wake_up`；`.poll` 里先 `poll_wait`；`fasync` 发 `SIGIO`；`MEM_CLEAR` 这个 ioctl 清空缓冲 | 对 `/dev/globalfifo*` 写入成功 |
| `ch04_timer` | `timer_list` 到期后在软中断里跑，回调里 `mod_timer()` 安排下一秒。读设备节点得到计数值 | `/dev/second*` 出现 |
| `ch05_misc` | `misc_register` 使用固定主设备号 10，自动创建设备节点，适合只需要一个次设备号的小设备 | `/dev/misc_demo*` 出现 |
| `ch06_platform` | `platform_device` 描述资源，`platform_driver.probe` 在名字匹配后被调用。本章在 `module_init` 里自己注册设备，用来看绑定过程 | `/sys/bus/platform/devices/` 里出现该设备 |

`copy_to_user` 可能睡眠，所以放在 mutex 里，不放在自旋锁里。这个约束到 `learn/01_uapi` 和 `learn/03_irq` 仍然成立。

`ch06` 的注释里出现了设备树，资源从 `reg` 和 `interrupts` 变成 `platform_get_resource` 的过程在 `learn/02_devicetree`。

### 3. 对照手册：`hardcode/`

读完 `ch01`–`ch06` 再看。它不是新的子系统，可加载的短例子是 `hardcode_samples.c`。

[data_structures.md](hardcode/data_structures.md) 覆盖这些容器，并指到仓库或内核里的真实使用处：

- 链表：`list_head`、`hlist_head`、`llist_head`、`plist_head`
- 树和查找：`rb_root`、区间树、`idr`/`ida`、radix tree、`xarray`、`hashtable`、`rhashtable`
- 缓冲：`kfifo`、`circ_buf`、`sk_buff`、`scatterlist`、`bio`
- 引用计数和等待：`kref`、`refcount_t`、`wait_queue_head_t`、`completion`、`timer_list`、`hrtimer`、`work_struct`、`notifier_block`、位图
- 锁：mutex、spinlock、seqlock、RCU

5.15 上 `plist_add` / `plist_del` 没有导出给模块，样本只做 `plist_node_init`。真正的入队在交换区和实时调度器里。

[design_patterns.md](hardcode/design_patterns.md) 把驱动里反复出现的写法收在一处：`container_of`、ops 表、probe/remove、`devm_*`、上半部/下半部、通知链、状态机。读 `learn/` 时可以回来对这几节。

### 4. 主线：`learn/`

这七章把接口、资源和并发接成一块外设。QEMU 的 `init.sh` 会跑下面的检查，通过标准不是「模块能 `insmod`」。每章目录里有源码、`README.md`、`architecture.md` 和 `flowchart.md`。

#### `learn/01_uapi` — 用户怎么碰到这块设备

`ch02` 是 read/write，`ch03` 加上了阻塞、poll、fasync 和清空命令。这一章把一块页上的缓冲同时暴露成六种接口，并带用户态程序 `uapi_test.c`。

| 接口 | 什么时候用 | 本模块 |
| --- | --- | --- |
| read/write | 字节流 | 读写这一页，长度记在 `len` |
| ioctl | 一次调用带命令，不是字节流 | `VIRT_UAPI_GET_LEN`、`VIRT_UAPI_CLEAR`，命令号在 `virt_uapi_cmd.h`，内核和用户态共用 |
| poll | 要和别的 fd 一起等 | 有数据返回 `POLLIN`；`CLEAR` 之后超时返回 |
| mmap | 大块数据不想每次拷贝 | `remap_pfn_range` 映射同一页。用户改第一个字节后，`read` 读到 `Hello-uapi` |
| sysfs | 一个属性一个文件，给脚本 | `/sys/class/misc/virt_uapi/bytes` |
| debugfs | 开发时看，不承诺稳定 | `/sys/kernel/debug/virt_uapi/flags` |

客户机检查：`/bin/uapi_test` 退出码为 0。它依次做写、读、ioctl、sysfs、debugfs、mmap 写回、poll 有数据、`CLEAR` 之后 poll 超时。

#### `learn/02_devicetree` — 板级描述怎么进 probe

SoC 上的平台设备没有 PCI 那种枚举。`virt-reg.dts` 写的是：

```
compatible = "ldd,virt-reg";
reg = <0x0 0x10000000 0x0 0x1000>;
interrupts = <19>;
ldd,threshold = <40>;
```

| 设备树 | 驱动里 |
| --- | --- |
| `compatible` | `of_device_id`。本客户机靠驱动名 `virt_dt` 绑定 |
| `reg` | `IORESOURCE_MEM`。实验用一页 RAM 的物理地址代替 `0x10000000`，probe 里 `memremap` 后 `readl` / `writel` |
| `interrupts = <19>` | `IORESOURCE_IRQ`。只读编号，不 `request_irq(19)`，避免占用客户机上真实的 19 号中断 |
| `ldd,threshold` | `of_property_read_u32`。OF 关闭时这个桩返回错误，驱动改用 `platform_data` 里的同一个值 40 |

客户机检查：`threshold` 为 40，`id` 为 `4c444454`，`irq` 为 19。宿主机上可以单独跑 `dtc -I dts -O dtb -o /tmp/virt-reg.dtb learn/02_devicetree/virt-reg.dts`。

#### `learn/03_irq` — 硬中断、线程和工作队列

客户机没有多余的硬件中断线。模块建一个只有 1 个 hwirq 的 `irq_domain`，写 `kick` 时用 `IRQ_WORK_HARD` 在硬中断里调用 `generic_handle_domain_irq()`。

1. 上半部只拿自旋锁，增加 `hard_count`，返回 `IRQ_WAKE_THREAD`。
2. threaded irq 在进程上下文拿 mutex，增加 `thread_count`，再 `schedule_work()`。
3. 工作队列再拿同一把 mutex，增加 `work_count`。

`/sys/devices/platform/virt_irq/counts` 的六个数是 `hard thread work hard_in_interrupt thread_in_task raise_errno`。通过标准是前五个都至少为 1，且 `raise_errno` 为 0。`hard_in_interrupt` 为 1 说明上半部在中断上下文，`thread_in_task` 为 1 说明线程在进程上下文。

#### `learn/04_soc_sensor` — 一次 probe 里的三件事

GPIO、时钟、regmap 在 `ch21`–`ch23` 和 `ch08` 里各注册各的。真实 SoC 驱动在同一次 probe 里把它们接上。这一章是一块虚拟温度传感器：

1. `devm_regmap_init` 读写 ID、CTRL、TEMP、THRESHOLD 四个寄存器。
2. 注册 24 MHz 固定钟，用 `devm_clk_hw_register_clkdev` 登记连接名 `sensor`，再 `devm_clk_get(dev, "sensor")` 和 `clk_prepare_enable()`。这是消费者侧的查找。
3. 与上一章相同的 irqdomain。写 `temperature` 后在硬中断里触发，threaded handler 里 `regmap_read`。上半部只返回 `IRQ_WAKE_THREAD`。

`virt-sensor.dts` 是同一块硬件的设备树写法（`reg`、`interrupts`、`clocks`）。本客户机仍靠平台设备名绑定。

客户机检查：写入 37 后，`temperature` 为 37，`clock_hz` 为 24000000，`irq_count` 至少为 1，`last_temp` 为 handler 读到的 37。

#### `learn/05_pm` — 运行时挂起和系统睡眠

`dev_pm_ops` 上有两对回调。

系统睡眠的 `suspend` / `resume` 把寄存器 `ctrl` 从 3 清成 0，唤醒时写回。生产环境里它们由 `pm_suspend()` 调用。实验不把客户机送进 mem 睡眠；sysfs 的 `sleep` 调用的是驱动结构体里的同一对函数指针。

runtime PM 在 probe 里 `pm_runtime_set_active()` 然后 `pm_runtime_enable()`。写 `idle` 调用 `pm_runtime_idle()`。usage 为 0 时进入 `runtime_suspend`。把 `power/control` 写成 `on` 会 `pm_runtime_forbid()` 并 resume。`runtime_marker` 由驱动自己的 runtime 回调更新，用来确认走的是这对函数。

客户机检查：suspend 后 `ctrl` 为 0、`sleeping` 为 1；resume 后 `ctrl` 回到 3；idle 后 `runtime_status` 和 `runtime_marker` 都是 `suspended`；`power/control` 写成 `on` 之后两者都是 `active`。

#### `learn/06_dma` — 一致性内存和流式 DMA

`ch12_dma/dma_demo.c` 打印 `kmalloc` / `vmalloc` 的虚拟地址和物理地址。DMA 映射在这里。平台设备设置 64 位 `dma_mask` 后做三次映射，成功就解除，sysfs 只留下结果：

| API | 内存从哪来 | 同步 |
| --- | --- | --- |
| `dma_alloc_coherent` | DMA 层分配，CPU 和设备同时可见 | 不调用 `dma_sync_*` |
| `dma_map_single` | `kmalloc` | 交给设备前 `dma_sync_single_for_device`，CPU 再读之前 `dma_sync_single_for_cpu`，然后 `dma_unmap_single` |
| `dma_map_sg` | `alloc_page` 放进 `scatterlist` | `dma_unmap_sg` |

客户机检查：`/sys/devices/platform/virt_dma/report` 含有 `coherent:ok`、`stream:ok`、`sg:ok`。

#### `learn/07_debug` — 打印、跟踪、以及 oops 怎么读

`virt_debug_ping()` 标了 `noinline`。`CONFIG_DYNAMIC_DEBUG` 打开时，`pr_debug` 默认不打印，`pr_info` 会进 ring buffer。

客户机先写一次 `trigger`，此时 dmesg 里有 `ping info`、没有 `ping debug`。然后：

```bash
echo 'module virt_debug +p' > /sys/kernel/debug/dynamic_debug/control
```

再触发一次，`ping debug` 出现。

ftrace 要先选定 tracer，再写过滤器。顺序反了时，过滤器会在切到 function tracer 时被清掉，trace 里就找不到这个函数：

```bash
echo 0 > /sys/kernel/tracing/tracing_on
echo nop > /sys/kernel/tracing/current_tracer
echo function > /sys/kernel/tracing/current_tracer
echo virt_debug_ping > /sys/kernel/tracing/set_ftrace_filter
echo 1 > /sys/kernel/tracing/tracing_on
echo 1 > /sys/devices/platform/virt_debug/trigger
echo 0 > /sys/kernel/tracing/tracing_on
grep virt_debug_ping /sys/kernel/tracing/trace
echo nop > /sys/kernel/tracing/current_tracer
```

读 oops 时先看 `RIP`（出错指令）、`CR2`（访问的地址）和 `Call Trace`。`ch29_thermal` 曾在客户机里缺页：热管理核心在 `ops->critical` 为空时写入这个函数指针，而 ops 表放在只读的 `.rodata`。修法是提供 `virt_critical()`，并去掉 `const`。教学模块不再主动触发这次写入。

### 5. 总线和设备类：`ch07`–`ch20`

主线之后再按设备类看。这些章的 QEMU 检查多数是「加载成功，并且 sysfs 或设备节点出现」。

| 章节 | 目录 | 模块 | 这一章增加的东西 |
| --- | --- | --- | --- |
| Ch07 | `ch07_input` | `input_demo` | `input_dev`，事件从 `/dev/input/event*` 出去 |
| Ch08 | `ch08_regmap` | `regmap_demo` | 自定义总线的 `regmap_read` / `write` / `update_bits`，寄存器在内存里 |
| Ch09 | `ch09_watchdog` | `watchdog_demo` | 看门狗设备节点和喂狗接口 |
| Ch10 | `ch10_rtc` | `rtc_demo` | `rtc_device`，时间从 `/dev/rtc*` 读 |
| Ch11 | `ch11_pwm` | `pwm_demo` | `pwm_chip`，5.15 的 `pwm_ops.get_state` 返回 `void` |
| Ch12 | `ch12_dma` | `dma_demo` | `kmalloc` 与 `vmalloc` 的地址转换。DMA API 看 `learn/06_dma` |
| Ch13 | `ch13_net_virtual` | `snull` | 一对虚拟网卡，`net_device` 和 `ndo_start_xmit` |
| Ch14 | `ch14_net_mac_phy` | `eth_mac` | 以太网 MAC 和 NAPI：`container_of` 从 `napi_struct` 找回私有数据 |
| Ch15 | `ch15_i2c` | `i2c_master`, `i2c_slave` | 先加载控制器，再加载从设备。从设备节点在 `/dev/i2c_virt*` |
| Ch16 | `ch16_spi` | `spi_master`, `spi_slave` | 与 I2C 相同的控制器/从设备配对 |
| Ch17 | `ch17_block` | `vmem_disk` | 虚拟块设备，请求队列和 bio |
| Ch18 | `ch18_mmc` | `mmc_virt` | MMC 宿主，`/sys/class/mmc_host/` |
| Ch19 | `ch19_pci_bus` | `pci_basic`, `pci_advanced` | PCI 枚举、配置空间和 BAR。没有真实设备时只走到驱动注册 |
| Ch20 | `ch20_usb_bus` | `usb_basic`, `usb_advanced` | USB 驱动注册和 URB 的形状 |

I2C/SPI 要按「控制器然后从设备」的顺序加载，`init.sh` 就是这样做的。

### 6. 子系统索引：`ch21`–`ch68`

每一章注册一个虚拟设备或子系统对象，用来看这个子系统的注册函数和最小 ops 表。打开目录之前先看它的 `README.md`。完整的内核目录对照，包括故意不做的 ACPI、早期启动和架构专属代码，在 [LINUX_DRIVER_COVERAGE.md](LINUX_DRIVER_COVERAGE.md)。

按用途找：

| 要看的东西 | 章节 |
| --- | --- |
| 引脚、时钟、复位、调节器 | Ch21 GPIO、Ch22 pinctrl、Ch23 clk、Ch24 regulator、Ch25 reset |
| 非易失存储和传感器框架 | Ch26 nvmem、Ch27 hwmon、Ch51 IIO |
| 灯、热、电源、背光 | Ch28 LED、Ch29 thermal、Ch30 power supply、Ch31 backlight、Ch48 powercap、Ch43 devfreq、Ch49 OPP |
| 链路和时钟同步 | Ch32 PHY、Ch33 PPS、Ch34 PTP、Ch50 MDIO |
| 终端和显示 | Ch35 TTY、Ch36 fbdev、Ch67 DRM、Ch65 V4L2 |
| 多功能器件和连接器 | Ch37 MFD、Ch38 extcon |
| 存储 | Ch39 SCSI、Ch54 MTD |
| 更多总线 | Ch56 1-Wire、Ch57 CAN、Ch58 mux、Ch60 SPMI、Ch61 I3C |
| 声音、HID、无线 | Ch52 ALSA、Ch53 HID、Ch45 rfkill、Ch64 Bluetooth、Ch63 NFC |
| 虚拟化和协处理器 | Ch41 virtio、Ch42 remoteproc、Ch59 FPGA |
| 其余 | Ch40 crypto、Ch44 dmaengine、Ch46 hwspinlock、Ch47 mailbox、Ch55 UIO、Ch62 GNSS、Ch66 counter、Ch68 interconnect |

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

这些章在 QEMU 里的检查是 `insmod` 然后 `rmmod`。要看注册是否真的绑上，读该章 `probe` 里的 `dev_info`，以及 `dmesg`。

## QEMU 里每类检查在看什么

| 范围 | 通过意味着 |
| --- | --- |
| `ch01`–`ch18` 里带设备节点的章 | 模块加载，并且 `/dev` 或 sysfs 上出现预期节点。`ch02` 还会写入再读回 |
| `ch19`–`ch68`、`hardcode` | 模块能加载并卸载。依赖 `=m` 的子系统时，先插入 `preload/` |
| `learn/01`–`learn/07` | 用户态程序或 sysfs 读回与上面各节的预期一致，包括 dyndbg 和 ftrace |

`init.sh` 在 busybox `sh` 下跑，作为 PID 1。改检查时改这个文件，再重新 `build_rootfs.sh`。

## 编码

内核模块按 Linux 内核编码风格编写：缩进用 Tab，行宽尽量不超过 100 列。GPL 符号（`irq_domain`、`pm_runtime`、`regmap` 等）要求模块声明 `MODULE_LICENSE("GPL")`。

probe 失败时沿着已经拿到的资源往回放。`learn/` 里能用 `devm_*` 的时钟、regmap 和内存映射交给设备管理；irqdomain 不是 devm 资源，在 `remove` 里 `free_irq`、`irq_dispose_mapping`、`irq_domain_remove`。

## 常见问题

### 编译提示找不到内核头文件

安装的是教学内核的头文件，不是当前宿主机的 `uname -r`：

```bash
sudo apt-get install linux-headers-5.15.0-173-generic
```

### 宿主机上 insmod 报 vermagic 或 Exec format error

模块是给 `5.15.0-173-generic` 编的。用 `bash qemu_test/run_qemu.sh --interactive`，在客户机里加载。

### 设备树章节没有看到 of_node

这台客户机的 `CONFIG_OF` 未打开。`virt-reg.dts` 用 `dtc` 检查；运行时的资源来自模块里的 `platform_device_info`。`threshold`、`id`、`irq` 三个 sysfs 文件用来确认 probe 读到了和 dts 相同的数。

### ftrace 里没有 virt_debug_ping

先 `echo function > current_tracer`，再写 `set_ftrace_filter`，然后打开 `tracing_on`。过滤器写在 tracer 仍是 `nop` 的时候，切到 function tracer 会把它清掉。

### 想单步进模块

`learn/07_debug` 的 README 写了 kgdb：QEMU 加 `-s -S`，宿主机 `target remote :1234`。Ubuntu 的 `vmlinuz` 没有调试信息时，用 `/sys/module/<name>/sections/.text` 的地址对 `.ko` 做 `add-symbol-file`。

## 贡献与许可

问题和使用讨论放在 GitHub Issues 和 Discussions。贡献方式见 [CONTRIBUTING.md](CONTRIBUTING.md)。

| 贡献者 | 角色 | 贡献内容 |
|--------|------|---------|
| **weijie-lee** | 项目创建者 | 项目架构、驱动代码、QEMU 测试基础设施 |
| **Manus AI** | 技术顾问 | 代码审查、文档编写、项目重新编排 |

本项目采用 **GPL 2.0** 许可证，见 [LICENSE](LICENSE)。

进一步阅读：

- [Linux Kernel Documentation](https://www.kernel.org/doc/)
- [Linux Device Drivers, 3rd Edition](https://lwn.net/Kernel/LDD3/)
- [Linux Kernel Module Programming Guide](https://sysprog21.github.io/lkmpg/)

- GitHub Issues：https://github.com/weijie-lee/linux_device_driver/issues
- GitHub Discussions：https://github.com/weijie-lee/linux_device_driver/discussions

---

**最后更新**：2026 年 10 月 21 日  
**内核目标**：5.15.0-173-generic  
**章节**：`ch01`–`ch68` 与 `learn/` 参与 `make all`
