# PPS

## 代码知识点

PPS 把一个外部脉冲当成时间源。驱动 `pps_register_source()` 得到 `pps_device`，每个脉冲调用 `pps_event()`。用户空间打开 `/dev/ppsN`，用 `PPS_FETCH` 取最后一次时间戳。

本模块注册源，不接真实引脚。节点出现就说明源已经挂上。

## 代码架构

```
pps_register_source
/dev/ppsN 或 /sys/class/pps
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch33
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch33_pps_virt_pps.ko
insmod ok
+ lsmod | grep virt_pps
virt_pps               16384  0 
+ find /sys /dev /proc -name '*pps*' 2>/dev/null | head -n 30
/sys/class/pps
/sys/class/pps/pps0
/sys/devices/platform/virt_pps
/sys/devices/platform/virt_pps/pps
/sys/devices/platform/virt_pps/pps/pps0
/sys/bus/platform/devices/virt_pps
/sys/bus/platform/drivers/virt_pps
/sys/bus/platform/drivers/virt_pps/virt_pps
/sys/module/virt_pps
/sys/module/virt_pps/drivers/platform:virt_pps
/dev/pps0
+ rmmod virt_pps
rmmod ok
[PASS] ch33_pps
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch33_pps`
- 能找到 PPS 设备。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch33
```
