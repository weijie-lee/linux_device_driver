# 学习顺序

每一步要看的接口、客户机里的通过标准，以及这台 5.15 内核的限制，写在 [README.md](README.md) 的「学习顺序」一节。这里只保留目录顺序。

`ch01`–`ch68` 按子系统各给一个可加载模块。`ch21` 以后当作索引：要查 GPIO、IIO 或 DRM 时打开对应目录。从入门到能写一块新外设，按下面的顺序读。

## 内核主线

具体驱动在 `ch*` 和 `learn/`。要拆操作系统怎么管理驱动，从 `kernel/` 读，不按章节编号插进去。

1. `kernel/01_driver_framework`：5.15 `drivers/base` 的 `bus_type`、`device`、`device_driver`。先注册总线和两台设备，再注册驱动。匹配上的设备走 `bus->probe` 再进驱动的 probe；另一台 `match` 返回 0。`/sys/bus/lddfw/trace` 记录这个顺序。
2. `kernel/02_kernel_interface`：同一条绑定路径上的内核函数，以及字符设备的 `file_operations`。ftrace 里要看到 `really_probe` 调用总线 probe、`chrdev_open` 调用驱动的 open、`vfs_read` 调用驱动的 read。
3. `kernel/03_driver_calls`：驱动调用内核搬数据。写 `/dev/virt_call` 时要看到 `virt_call_write <-vfs_write` 和 `__check_object_size <-virt_call_write`。读回来时调用者换成 `virt_call_read`。读到的字节和写入的相同。
4. `kernel/04_user_paths`：ioctl、compat ioctl、sysfs show、proc seq_file。`UPATH_SET` 在降权后返回 `EPERM`。
5. `kernel/05_sleep_wake`：读者停在 `wait_event`，状态是 `D`。写者 `wake_up` 之后 function_graph 里能看到 `try_to_wake_up`。把 hung task 阈值改成 2 秒，检测器打印读者的栈。
6. `kernel/06_irq_bottom`：一次软件中断穿过硬中断、tasklet、中断线程和工作队列。`counts` 是 `1 1 1 1 1 1 1 0`。
7. `kernel/07_lock_context`：进程里 `preempt_count` 是 0，硬中断里是 65536。空转触发 `soft lockup`，RIP 落在 `virt_lock_spin`。
8. `kernel/08_pci_mmio`：edu 设备 `1234:11e8`。`ident` 是 `010000ed`。往活性寄存器写 `0x12345678`，读回 `edcba987`。`ioread32` 不在函数跟踪过滤表里。
9. `kernel/09_msi_irq`：同一块卡的 MSI。`echo 5` 之后 `fact=120`，`/proc/interrupts` 里 `virt_msi` 的计数加一。

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
