# 主线

子系统示例在 `ch01`–`ch68`。这条目录是把它们串成驱动的部分，按编号读。

| 目录 | 主题 |
| --- | --- |
| `01_uapi` | read/write、ioctl、poll、mmap、sysfs、debugfs，带用户态程序 |
| `02_devicetree` | `reg` / `interrupts` / 自定义属性如何变成 platform 资源 |
| `03_irq` | 硬中断、threaded irq、workqueue，以及自旋锁和 mutex 的分工 |
| `04_soc_sensor` | 一次 probe 里的 regmap、时钟和中断 |
| `05_pm` | runtime PM 和系统睡眠回调 |
| `06_dma` | `dma_alloc_coherent`、`dma_map_single`、`dma_map_sg` |
| `07_debug` | dyndbg、ftrace，以及 oops 怎么读 |

建议顺序写在 [LEARNING_PATH.md](../LEARNING_PATH.md)。QEMU 客户机是 `5.15.0-173-generic`，`CONFIG_OF` 没有打开，所以设备树一章在模块里填与 `virt-reg.dts` 相同的 resource，而不是启动时加载 dtb。
