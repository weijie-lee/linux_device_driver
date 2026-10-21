# OPP

## 代码知识点

OPP 是一组「频率、电压」工作点。`dev_pm_opp_add()` 往设备的表里加一项。devfreq 和 cpufreq 从这张表里挑点。

本模块给自己的 platform 设备加一个工作点，证明表能建起来。和 `ch43_devfreq` 配在一起看。

## 代码架构

```
platform 设备 virt_opp
dev_pm_opp_add(频率, 电压)
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch49
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch49_opp_virt_opp.ko
insmod ok
+ lsmod | grep virt_opp
virt_opp               16384  0 
+ find /sys /dev /proc -name '*virt_opp*' 2>/dev/null | head -n 30
/sys/kernel/debug/opp/platform-virt_opp
/sys/devices/platform/virt_opp
/sys/bus/platform/devices/virt_opp
/sys/bus/platform/drivers/virt_opp
/sys/bus/platform/drivers/virt_opp/virt_opp
/sys/module/virt_opp
/sys/module/virt_opp/drivers/platform:virt_opp
+ rmmod virt_opp
rmmod ok
[PASS] ch49_opp
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch49_opp`
- 能找到 `virt_opp`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch49
```
