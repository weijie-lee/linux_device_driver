# PTP 时钟

## 代码知识点

PTP 硬件时钟是 `ptp_clock_info`。`gettime64` / `settime64` / `adjfine` 让网络时间协议调整这块钟。`ptp_clock_register()` 创建 `/dev/ptpN`。

本钟的时间存在驱动变量里，`adjfine` 接受调整但不改真实晶振。

## 代码架构

```
ptp_clock_info
  gettime64 / settime64 / adjfine
ptp_clock_register → /dev/ptpN
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch34
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch34_ptp_virt_ptp.ko
insmod ok
+ lsmod | grep virt_ptp
virt_ptp               16384  0 
+ find /sys /dev /proc -name '*ptp*' 2>/dev/null | head -n 30
/sys/kernel/tracing/events/xen/xen_mmu_alloc_ptpage
/sys/kernel/tracing/events/xen/xen_mmu_release_ptpage
/sys/kernel/debug/tracing/events/xen/xen_mmu_alloc_ptpage
/sys/kernel/debug/tracing/events/xen/xen_mmu_release_ptpage
/sys/class/ptp
/sys/class/ptp/ptp0
/sys/devices/platform/virt_ptp
/sys/devices/platform/virt_ptp/ptp
/sys/devices/platform/virt_ptp/ptp/ptp0
/sys/bus/platform/devices/virt_ptp
/sys/bus/platform/drivers/virt_ptp
/sys/bus/platform/drivers/virt_ptp/virt_ptp
/sys/module/virt_ptp
/sys/module/virt_ptp/drivers/platform:virt_ptp
/dev/ptp0
+ rmmod virt_ptp
rmmod ok
[PASS] ch34_ptp
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch34_ptp`
- 能找到 PTP 时钟设备。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch34
```
