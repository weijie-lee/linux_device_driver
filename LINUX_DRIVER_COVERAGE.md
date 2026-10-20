# Linux 5.15 `drivers/` 覆盖对照

本仓库是面向内核 **5.15** 的驱动教学目录，目标是让每个可以安全做成外置模块的驱动子系统都有一章可加载示例。它不会把内核 `drivers/` 树原样拷进来，也不会替换客户机里正在使用的启动期驱动。

对照基准是 Ubuntu `5.15.0-173-generic` 的 `drivers/` 目录（与 `/lib/modules/5.15.0-173-generic/build` 一致）。顶层 `make all` 只编译下表里标了章节的目录。`ch07_pci_bus` 与 `ch08_usb_bus` 是 `ch19_pci_bus`、`ch20_usb_bus` 的同名副本，不参与顶层编译。`ch04_regmap_framework`、`ch05_i2c_bus` 只有说明，实现分别在 `ch08_regmap` 和 `ch15_i2c`。`ch00_framework` 是框架总览，没有模块。

模块按 5.15 头文件编译。宿主机若是更新的内核，`insmod` 会因 vermagic 拒绝这些 `.ko`。加载验证走 `qemu_test/`，客户机内核是 `5.15.0-173-generic`。标记为 `=m` 的子系统由 `build_rootfs.sh` 把提供者模块放进 initramfs 的 `preload/`，`init.sh` 在加载教学模块之前先插入它们。

## 已实现的子系统

| 内核目录 | 章节 | 模块 |
| --- | --- | --- |
| （内核模块 / kfifo，不在 `drivers/`） | `ch01_kernel_basics` | `kfifo_demo_static` |
| `char` | `ch02_char_basic` | `globalmem` |
| `char` | `ch03_char_advanced` | `globalfifo` |
| `kernel/time`（定时器，不在 `drivers/`） | `ch04_timer` | `second` |
| `misc` | `ch05_misc` | `misc_demo` |
| `base` / `platform` | `ch06_platform` | `platform_demo` |
| `input` | `ch07_input` | `input_demo` |
| `base/regmap` | `ch08_regmap` | `regmap_demo` |
| `watchdog` | `ch09_watchdog` | `watchdog_demo` |
| `rtc` | `ch10_rtc` | `rtc_demo` |
| `pwm` | `ch11_pwm` | `pwm_demo` |
| `dma`（DMA API / 一致性内存） | `ch12_dma` | `dma_demo` |
| `net` | `ch13_net_virtual` | `snull` |
| `net` | `ch14_net_mac_phy` | `eth_mac` |
| `i2c` | `ch15_i2c` | `i2c_master`, `i2c_slave` |
| `spi` | `ch16_spi` | `spi_master`, `spi_slave` |
| `block` | `ch17_block` | `vmem_disk` |
| `mmc` | `ch18_mmc` | `mmc_virt` |
| `pci` | `ch19_pci_bus` | `pci_basic`, `pci_advanced` |
| `usb` | `ch20_usb_bus` | `usb_basic`, `usb_advanced` |
| `gpio` | `ch21_gpio` | `virt_gpio` |
| `pinctrl` | `ch22_pinctrl` | `virt_pinctrl` |
| `clk` | `ch23_clk` | `virt_clk` |
| `regulator` | `ch24_regulator` | `virt_regulator` |
| `reset` | `ch25_reset` | `virt_reset` |
| `nvmem` | `ch26_nvmem` | `virt_nvmem` |
| `hwmon` | `ch27_hwmon` | `virt_hwmon` |
| `leds` | `ch28_leds` | `virt_led` |
| `thermal` | `ch29_thermal` | `virt_thermal` |
| `power/supply` | `ch30_power` | `virt_psy` |
| `video/backlight` | `ch31_backlight` | `virt_backlight` |
| `phy`、`net/phy` | `ch32_phy` | `virt_phy` |
| `pps` | `ch33_pps` | `virt_pps` |
| `ptp` | `ch34_ptp` | `virt_ptp` |
| `tty` | `ch35_tty` | `virt_tty` |
| `video`（fbdev） | `ch36_fb` | `virt_fb` |
| `mfd` | `ch37_mfd` | `virt_mfd` |
| `extcon` | `ch38_extcon` | `virt_extcon` |
| `scsi` | `ch39_scsi` | `virt_scsi` |
| `crypto` | `ch40_crypto` | `virt_crypto` |
| `virtio` | `ch41_virtio` | `virt_virtio` |
| `remoteproc` | `ch42_remoteproc` | `virt_rproc` |
| `devfreq` | `ch43_devfreq` | `virt_devfreq` |
| `dma`（dmaengine） | `ch44_dmaengine` | `virt_dmaeng` |
| `net/rfkill` | `ch45_rfkill` | `virt_rfkill` |
| `hwspinlock` | `ch46_hwspinlock` | `virt_hwspin` |
| `mailbox` | `ch47_mailbox` | `virt_mbox` |
| `powercap` | `ch48_powercap` | `virt_powercap` |
| `opp` | `ch49_opp` | `virt_opp` |
| `net/mdio` | `ch50_mdio` | `virt_mdio` |
| `iio` | `ch51_iio` | `virt_iio` |
| `sound/`（ALSA，不在 `drivers/`） | `ch52_alsa` | `virt_alsa` |
| `hid` | `ch53_hid` | `virt_hid` |
| `mtd` | `ch54_mtd` | `virt_mtd` |
| `uio` | `ch55_uio` | `virt_uio` |
| `w1` | `ch56_w1` | `virt_w1` |
| `net/can` | `ch57_can` | `virt_can` |
| `mux` | `ch58_mux` | `virt_mux` |
| `fpga` | `ch59_fpga` | `virt_fpga` |
| `spmi` | `ch60_spmi` | `virt_spmi` |
| `i3c` | `ch61_i3c` | `virt_i3c` |
| `gnss` | `ch62_gnss` | `virt_gnss` |
| `nfc` | `ch63_nfc` | `virt_nfc` |
| `bluetooth` | `ch64_bluetooth` | `virt_bt` |
| `media` | `ch65_v4l2` | `virt_v4l2` |
| `counter` | `ch66_counter` | `virt_counter` |
| `gpu/drm` | `ch67_drm` | `virt_drm` |
| `interconnect` | `ch68_interconnect` | `virt_icc` |

## 有意不做成外置模块的目录

这些目录出现在 5.15 的 `drivers/` 里。不给它们单独写 `.ko` 的原因是：架构专属、会替换客户机正在使用的驱动、属于启动早期、或者公开头文件里没有可链接的注册接口。

| 目录 | 原因 |
| --- | --- |
| `accessibility` | 控制台辅助输出，绑在 VT 上。输入事件见 `ch07_input`。 |
| `acpi` | 固件与启动期电源管理。替换客户机 ACPI 驱动会使虚拟机无法启动。 |
| `amba` | ARM PrimeCell 总线。测试客户机是 x86_64。 |
| `android` | Binder / ashmem 等 Android IPC，不是通用设备类。 |
| `ata` | libata 持有客户机磁盘。块层示例是 `ch17_block`。 |
| `atm` | 传统 ATM 网卡。5.15 上没有适合短示例的虚拟 ATM 控制器。 |
| `auxdisplay` | `charlcd` 头文件不在 `linux-headers` 安装结果里，外置模块无法编译。 |
| `bcma` | Broadcom SoC 总线，客户机没有对应硬件。 |
| `bus` | 各 SoC 片内总线集合。通用片内互连见 `ch68_interconnect`。 |
| `cdrom` | 统一光盘层，建在 SCSI/ATA 之上。传输层示例是 `ch39_scsi`。 |
| `clocksource` | 时间维护核心，不能由模块替换。 |
| `comedi` | 数据采集用户态 ABI。工业量测见 `ch51_iio`。 |
| `connector` | netlink 连接器，是内核 IPC，不是设备类。 |
| `cpufreq` | 替换系统 cpufreq 会改整机频率策略。同一种时钟/电压模式见 `ch43_devfreq` 与 `ch49_opp`。 |
| `cpuidle` | 空闲策略与调度器、架构绑在一起。 |
| `cxl` | CXL 主机拓扑。本 QEMU 客户机没有 CXL 设备。 |
| `dax` | 持久内存直接访问，依赖平台提供的 pmem 区域。 |
| `dca` | 传统 Intel DCA，没有虚拟设备。 |
| `dio` | HP DIO，m68k / parisc。 |
| `dma-buf` | 缓冲区共享 API，由 `ch65_v4l2` 与 `ch67_drm` 使用，本身不是驱动类。 |
| `edac` | ECC 报告。`linux-headers` 不提供可链接的公共注册头。 |
| `eisa` | 传统 EISA 总线。 |
| `firewire` | IEEE 1394。客户机没有 OHCI 控制器，虚拟主机容易把 FireWire 核心带进错误路径。 |
| `firmware` | EFI、PSCI、memmap 等启动期固件加载。 |
| `fsi` | POWER FSI 总线。 |
| `greybus` | Project Ara 总线，依赖一组宿主模块，不适合作为短示例。 |
| `hsi` | 调制解调器高速同步串口，SoC 专属。 |
| `hv` | Hyper-V VMBus。本客户机是 QEMU。虚拟总线示例是 `ch41_virtio`。 |
| `hwtracing` | CoreSight / Intel PT，架构调试设施。 |
| `idle` | 与 `cpuidle` 相同，属于空闲驱动。 |
| `infiniband` | RDMA 栈需要完整的 HCA 与 `ib_core`。没有安全的微型虚拟 HCA。 |
| `iommu` | 替换 IOMMU 操作会破坏客户机全部 DMA。 |
| `ipack` | IndustryPack 总线。 |
| `irqchip` | 中断控制器在启动早期注册。 |
| `isdn` | 传统 ISDN / CAPI，没有干净的外置虚拟设备。 |
| `macintosh` | Apple 硬件。 |
| `mcb` | MEN Chameleon 总线。 |
| `md` | 软件 RAID 会认领真实磁盘。 |
| `memory` | 内存热插拔控制器，启动期设施。 |
| `memstick` | Sony MemoryStick，客户机没有宿主控制器。 |
| `message` | Fusion MPT RAID，硬件适配器。 |
| `most` | 汽车 MOST 总线，5.15 头文件里没有适合短示例的虚拟核心。 |
| `ntb` | 非透明桥，需要两端 PCI 硬件。 |
| `nubus` | m68k Macintosh 总线。 |
| `nvdimm` | 持久内存区域由平台创建。 |
| `nvme` | 会绑定客户机 NVMe。块层示例是 `ch17_block`。 |
| `of` | 设备树核心是内建的，由 platform 驱动使用。 |
| `parisc` | PA-RISC 架构。 |
| `parport` | PC 并口。客户机没有并口硬件，`parport` 核心在该内核上是模块。 |
| `pcmcia` | 传统 CardBus，客户机没有桥。 |
| `perf` | 性能事件子系统，不是设备驱动。 |
| `pnp` | PNP BIOS，x86 固件。 |
| `ps3` | PlayStation 3。 |
| `rapidio` | RapidIO 交换网。 |
| `ras` | 可靠性报告，走 ACPI/APEI。 |
| `rpmsg` | 远程处理器的 virtio 传输，挂在 remoteproc 上。处理器生命周期见 `ch42_remoteproc`。 |
| `s390` | s390 架构。 |
| `sbus` | SPARC SBUS。 |
| `sh` | SuperH。 |
| `siox` | Siemens SIOX 工业总线。 |
| `slimbus` | MIPI SLIMbus，SoC 音频。 |
| `soc` | 各 SoC 专属驱动集合。 |
| `soundwire` | MIPI SoundWire。公开头文件不足以做一个不碰真实声卡的短示例。音频类见 `ch52_alsa`。 |
| `ssb` | Broadcom Sonics 总线。 |
| `staging` | 暂存目录，不是一个子系统。 |
| `target` | SCSI target / LIO。发起端示例是 `ch39_scsi`。 |
| `tc` | Microchip 流量分类硬件。 |
| `tee` | 可信执行环境，没有 OP-TEE 就没有可注册的设备。 |
| `thunderbolt` | 需要 Thunderbolt 域，并和 IOMMU 交互。 |
| `vdpa` | vDPA 数据面，会碰到 virtio 核心。虚拟设备注册见 `ch41_virtio`。 |
| `vfio` | 把设备交给用户态，不适合放进这个教学客户机。 |
| `vhost` | virtio 宿主数据面，与 `vdpa` 同一类。 |
| `virt` | ACRN、机密计算等虚拟化宿主。通用虚拟设备是 `drivers/virtio`，见 `ch41_virtio`。 |
| `visorbus` | Unisys s-Par。 |
| `vlynq` | TI VLYNQ。 |
| `vme` | VME 总线。 |
| `xen` | Xen 虚拟化后端与前端。 |
| `zorro` | Amiga Zorro 总线。 |

`drivers/base` 没有单独模块：驱动模型、总线、设备和 `platform` 由 `ch06_platform` 以及其余各章共同使用。
