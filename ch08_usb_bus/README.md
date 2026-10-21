# ch08_usb_bus

本目录不参与 `make all`，镜像也不会加载这里的文件。能跑的代码在 [../ch20_usb_bus](../ch20_usb_bus)。

## 代码知识点

USB 接口驱动是 `usb_driver`。`usb_register()` 按 `id_table` 匹配，`probe` 里从端点描述符找 bulk，再用 URB 收发。

## 代码架构

注册和卸载的顺序与 [../ch20_usb_bus](../ch20_usb_bus) 里的模块相同。本目录如果留有同名 `.c`，只作对照，不要在这里 `make` 之后拿去 `insmod`。

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch20
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch20_usb_bus_usb_basic.ko
insmod ok
+ dmesg | grep -F USB
[    0.771060] ACPI: bus type USB registered
[    1.023043] ehci_hcd: USB 2.0 'Enhanced' Host Controller (EHCI) Driver
[    1.023426] ohci_hcd: USB 1.1 'Open' Host Controller (OHCI) Driver
[    1.023716] uhci_hcd: USB Universal Host Controller Interface driver
[    6.571146] [USB BASIC] Initializing USB Basic Driver
[    6.571378] [USB BASIC] USB driver registered successfully
+ ls -d /sys/bus/usb/drivers/usb_basic
/sys/bus/usb/drivers/usb_basic
+ rmmod usb_basic
rmmod ok
+ insmod /lib/modules/driver_test/ch20_usb_bus_usb_advanced.ko
insmod ok
+ ls -d /sys/bus/usb/drivers/usb_advanced
/sys/bus/usb/drivers/usb_advanced
+ rmmod usb_advanced
rmmod ok
[PASS] ch20_usb
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch20_usb`
- 回显与可运行那一章相同，并且退出码是 0。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch20
```
