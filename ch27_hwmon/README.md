# hwmon

## 代码知识点

hwmon 用 `hwmon_chip_info` 描述有哪些传感器。`is_visible` 决定 sysfs 文件出不出现，`read` 给出数值。温度通道的 `temp1_input` 单位是毫摄氏度。

`devm_hwmon_device_register_with_info()` 注册。本传感器只有一路温度，固定 25000。

## 代码架构

```
HWMON_CHANNEL_INFO(temp, HWMON_T_INPUT)
read → *val = 25000
devm_hwmon_device_register_with_info("virt_hwmon")
/sys/class/hwmon/hwmonN/temp1_input
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch27
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch27_hwmon_virt_hwmon.ko
insmod ok
+ lsmod | grep virt_hwmon
virt_hwmon             16384  0 
+ find /sys /dev /proc -name '*virt_hwmon*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_hwmon
/sys/bus/platform/devices/virt_hwmon
/sys/bus/platform/drivers/virt_hwmon
/sys/bus/platform/drivers/virt_hwmon/virt_hwmon
/sys/module/virt_hwmon
/sys/module/virt_hwmon/drivers/platform:virt_hwmon
+ cat /sys/class/hwmon/hwmon*/temp1_input
25000
+ rmmod virt_hwmon
rmmod ok
[PASS] ch27_hwmon
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch27_hwmon`
- `temp1_input` 是 `25000`（毫摄氏度，也就是 25.0°C）。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch27
```
