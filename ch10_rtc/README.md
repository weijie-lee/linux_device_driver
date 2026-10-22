# RTC

## 代码知识点

RTC 框架用 `rtc_device_register()`（或 `devm_rtc_allocate_device` + `rtc_register_device`）把驱动挂到 `/dev/rtcN`。用户空间用 `RTC_RD_TIME` / `RTC_SET_TIME` 这类 ioctl。驱动实现 `rtc_class_ops` 的 `read_time` 和 `set_time`。

本模块没有电池。时间是启动时刻加上 `time_offset`。闹钟用内核定时器，到期打印 `ALARM triggered`。

## 代码架构

```
platform 设备 virt_rtc
  probe → rtc_device_register
  /sys/class/rtc/rtcN/name = virt_rtc
  /dev/rtcN
read_time = 当前时间 + offset
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch10
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch10_rtc_rtc_demo.ko
insmod ok
+ cat /sys/class/rtc/rtc*/name
rtc_cmos 00:04
virt_rtc virt_rtc
+ ls /dev/rtc /dev/rtc0
ls: /dev/rtc: No such file or directory
/dev/rtc0
+ rmmod rtc_demo
rmmod ok
[PASS] ch10_rtc
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch10_rtc`
- `/sys/class/rtc/rtc*/name` 里有 `virt_rtc`，并且 `/dev/rtc0` 存在。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch10
```
