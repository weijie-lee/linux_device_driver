# Linux Device Driver — QEMU 测试报告

## 测试环境

| 项目 | 详情 |
|------|------|
| 运行方式 | `docker run --rm ldd-lab test` 与 `docker run --rm ldd-lab chapter all` |
| 镜像 | Ubuntu 22.04，GCC 11.4.0，`linux-headers-5.15.0-173-generic` |
| 测试内核 | Linux 5.15.0-173-generic（x86_64，QEMU 6.2.0，无 KVM 时用 TCG） |
| 持续集成 | `.github/workflows/docker-validation.yml`，推送到 `master` 和每个 PR 都会运行 |
| 最近一次 | 2026-10-23 |

容器里的 `uname -r` 是宿主机内核。模块只在 QEMU 客户机里 `insmod`。

## 测试结果汇总

| 套件 | 脚本 | 判据 | 结果 |
|------|------|------|------|
| 冒烟套件 | `qemu_test/init.sh` | 每个模块能 `insmod`，设备节点、sysfs 或网卡出现；`rmmod` 失败不算失败。`learn/` 与 `hardcode/` 做功能检查。`kernel/01` 核对绑定顺序。`kernel/02` 核对 ftrace 里的绑定路径和 `chrdev_open` / `vfs_read`。`kernel/03` 核对 `vfs_write` → `_copy_from_user` 和 `vfs_read` → `_copy_to_user`，并确认读回的字节与写入的相同 | **81 / 81 通过** |
| 章节实验 | `qemu_test/lab.sh` | 每章打印命令和回显。`rmmod` 失败、期望字样缺失、设备节点不是字符设备或块设备都算失败。`ch02`、`ch03`、`ch05`、`ch06`、`ch08` 另外运行 `tests/` 里的用户态程序，要求 `FAIL: 0`。`kernel01` 要求 trace 整行一致。`kernel02` 和 `kernel03` 要求 ftrace 函数顺序一致 | **79 / 79 通过** |

每一章的命令、回显和成功判据在各章 README 的「在镜像里复现」。回显是镜像里一次真实运行的记录。

`ch21`–`ch68` 多数只验证模块加载后 probe 建出自己的节点并能卸载，没有逐个驱动做读写。`ch15`、`ch16`、`ch17` 的 `tests/` 程序自己 `insmod` 当前目录的 `.ko`，不能直接对已加载的模块运行，所以实验只检查节点和回显。

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

### Bug 3：Ch03 globalfifo — 没有 platform_device，设备节点从不出现

**症状：** `insmod globalfifo.ko` 成功，`lsmod` 能看到模块，但 `/dev/global_mem_0` 不存在。早先的检查里 `echo fifo_test > /dev/global_mem_0` 在 devtmpfs 里建出了一个普通文件，`head` 又把它读了回来，所以实验显示通过。

**根本原因：** 驱动是 `module_platform_driver`，节点在 `probe` 里创建，而没有人注册名为 `globalfifo` 的 `platform_device`，`probe` 从未运行。`tests/test_globalfifo.c` 里的 `/dev/globalfifo` 和 `FIFO_CLEAR 0x1` 也与驱动不符。

**修复方案：** `module_init` 里先 `platform_device_register_simple("globalfifo")`，再 `platform_driver_probe()`。实验增加 `test -c`，并且 `want` 不再把 `ls` 的 `No such file` 当成功依据。用户态测试改用 `/dev/global_mem_0` 和 `_IO('g', 0)`。

### Bug 4：Ch08 regmap_demo — 平坦缓存把初始寄存器值盖成 0

**症状：** 读寄存器 5 得到 `0x0`，驱动注释写的是 `N * 0x10`。

**根本原因：** `REGCACHE_FLAT` 没有 `reg_defaults` 时缓存数组全是 0，第一次读命中缓存，不会走 `reg_read`。

**修复方案：** 为 256 个寄存器提供 `reg_defaults`，`regmap_init` 复制后释放临时表。

### Bug 5：learn/01 virt_uapi — `rmmod` 时 `misc_deregister` 内核 oops

**根本原因：** `dev_set_drvdata(misc.this_device, p)` 覆盖了 misc 核心放在 `drvdata` 里的 `miscdevice` 指针，`misc_devnode` 把 `virt_uapi` 的字段当成节点名。

**修复方案：** 不设 `drvdata`，`bytes_show` 直接用全局指针。

### Bug 6：learn/04 virt_sensor — `rmmod` 返回 EAGAIN

**根本原因：** 驱动自己是时钟的提供者，又 `clk_get` 持有它，模块引用计数被钉住，`exit` 不会运行。冒烟套件的 `rmmod` 失败不算失败，长期没发现。

**修复方案：** 读完频率后 `clk_disable_unprepare` + `clk_put`。

