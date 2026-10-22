# V4L2

## 代码知识点

V4L2 设备是 `video_device`。`v4l2_device_register()` 登记驱动，`video_register_device()` 得到 `/dev/videoN`。`v4l2_file_operations` 至少要有 `unlocked_ioctl` 走到 `video_ioctl2`，`vidioc_querycap` 报告驱动名。

本设备报告能力，不采集帧。节点在 `/sys/class/video4linux/`。

## 代码架构

```
v4l2_device_register
video_device
  fops + ioctl_ops.querycap
video_register_device → /dev/videoN
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch65
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch65_v4l2_virt_v4l2.ko
insmod ok
+ lsmod | grep virt_v4l2
virt_v4l2              16384  0 
videodev              258048  1 virt_v4l2
+ find /sys /dev /proc -name '*virt_v4l2*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_v4l2
/sys/bus/platform/devices/virt_v4l2
/sys/bus/platform/drivers/virt_v4l2
/sys/bus/platform/drivers/virt_v4l2/virt_v4l2
/sys/module/videodev/holders/virt_v4l2
/sys/module/virt_v4l2
/sys/module/virt_v4l2/drivers/platform:virt_v4l2
+ rmmod virt_v4l2
rmmod ok
[PASS] ch65_v4l2
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch65_v4l2`
- 能找到 `virt_v4l2`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch65
```
