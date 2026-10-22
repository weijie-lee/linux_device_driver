# 背光

## 代码知识点

背光设备是 `backlight_device`。`backlight_ops.update_status` 在亮度改变时被调用，`get_brightness` 返回当前档。`devm_backlight_device_register()` 注册。

sysfs 在 `/sys/class/backlight/`。本设备只记录亮度，不推屏幕。

## 代码架构

```
backlight_ops
  update_status / get_brightness
devm_backlight_device_register
/sys/class/backlight/...
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch31
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch31_backlight_virt_backlight.ko
insmod ok
+ lsmod | grep virt_backlight
virt_backlight         20480  0 
+ find /sys /dev /proc -name '*virt_backlight*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_backlight
/sys/bus/platform/devices/virt_backlight
/sys/bus/platform/drivers/virt_backlight
/sys/bus/platform/drivers/virt_backlight/virt_backlight
/sys/module/virt_backlight
/sys/module/virt_backlight/drivers/platform:virt_backlight
+ rmmod virt_backlight
rmmod ok
[PASS] ch31_backlight
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch31_backlight`
- 能找到 `virt_backlight`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch31
```
