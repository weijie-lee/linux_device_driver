# SPMI

## 代码知识点

SPMI 是电源管理芯片用的串行总线。控制器实现 `spmi_controller` 的 `cmd`、`read_cmd`、`write_cmd`，`spmi_controller_add()` 注册。从设备按地址挂在这条总线上。

本控制器的命令回调返回成功，没有 PMIC。

## 代码架构

```
spmi_controller_alloc
ops: cmd / read_cmd / write_cmd
spmi_controller_add
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch60
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch60_spmi_virt_spmi.ko
insmod ok
+ lsmod | grep virt_spmi
virt_spmi              16384  0 
spmi                   36864  1 virt_spmi
+ find /sys /dev /proc -name '*virt_spmi*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_spmi
/sys/bus/platform/devices/virt_spmi
/sys/bus/platform/drivers/virt_spmi
/sys/bus/platform/drivers/virt_spmi/virt_spmi
/sys/module/spmi/holders/virt_spmi
/sys/module/virt_spmi
/sys/module/virt_spmi/drivers/platform:virt_spmi
+ rmmod virt_spmi
rmmod ok
[PASS] ch60_spmi
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch60_spmi`
- 能找到 `virt_spmi`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch60
```
