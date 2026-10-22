# 1-Wire

## 代码知识点

1-Wire 主机实现 `w1_bus_master` 的触碰位时序，`w1_add_master_device()` 注册。核心会搜索总线上的从设备家族号。

本主机的位操作是空的，总线上不会出现真实从设备。实验看主机设备已经加上。

## 代码架构

```
w1_bus_master
w1_add_master_device
/sys/bus/w1/
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch56
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch56_w1_virt_w1.ko
insmod ok
+ lsmod | grep virt_w1
virt_w1                16384  0 
wire                   49152  1 virt_w1
+ find /sys /dev /proc -name '*virt_w1*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_w1
/sys/bus/platform/devices/virt_w1
/sys/bus/platform/drivers/virt_w1
/sys/bus/platform/drivers/virt_w1/virt_w1
/sys/module/wire/holders/virt_w1
/sys/module/virt_w1
/sys/module/virt_w1/drivers/platform:virt_w1
+ rmmod virt_w1
rmmod ok
[PASS] ch56_w1
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch56_w1`
- 能找到 `virt_w1`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch56
```
