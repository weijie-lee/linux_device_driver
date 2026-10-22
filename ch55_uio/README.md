# UIO

## 代码知识点

UIO 让用户空间映射设备内存并等中断。`uio_info` 填名字和 IRQ，`uio_register_device()` 创建 `/dev/uioN`。没有硬件中断时 `irq` 用 `UIO_IRQ_NONE`。

本设备没有映射真实 BAR。节点出现说明 UIO 核心接受了 `uio_info`。

## 代码架构

```
uio_info
  name = virt_uio
  irq = UIO_IRQ_NONE
uio_register_device → /dev/uioN
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch55
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch55_uio_virt_uio.ko
insmod ok
+ lsmod | grep virt_uio
virt_uio               16384  0 
uio                    20480  1 virt_uio
+ find /sys /dev /proc -name '*virt_uio*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_uio
/sys/bus/platform/devices/virt_uio
/sys/bus/platform/drivers/virt_uio
/sys/bus/platform/drivers/virt_uio/virt_uio
/sys/module/uio/holders/virt_uio
/sys/module/virt_uio
/sys/module/virt_uio/drivers/platform:virt_uio
+ rmmod virt_uio
rmmod ok
[PASS] ch55_uio
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch55_uio`
- 能找到 `virt_uio`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch55
```
