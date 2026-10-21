# GNSS

## 代码知识点

GNSS 框架把接收机暴露成字符设备，NMEA 句子从这里读。`gnss_allocate_device()` 分配，`gnss_register_device()` 注册。驱动在收到芯片数据时 `gnss_insert_raw()`。

本接收机的 `write` 接受命令，不输出真实定位。

## 代码架构

```
gnss_allocate_device
gnss_ops: open / close / write_raw
gnss_register_device
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch62
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch62_gnss_virt_gnss.ko
insmod ok
+ lsmod | grep virt_gnss
virt_gnss              16384  0 
gnss                   16384  1 virt_gnss
+ find /sys /dev /proc -name '*virt_gnss*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_gnss
/sys/bus/platform/devices/virt_gnss
/sys/bus/platform/drivers/virt_gnss
/sys/bus/platform/drivers/virt_gnss/virt_gnss
/sys/module/virt_gnss
/sys/module/virt_gnss/drivers/platform:virt_gnss
/sys/module/gnss/holders/virt_gnss
+ rmmod virt_gnss
rmmod ok
[PASS] ch62_gnss
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch62_gnss`
- 能找到 `virt_gnss`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch62
```
