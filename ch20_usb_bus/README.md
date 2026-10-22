# USB 驱动

## 代码知识点

USB 设备驱动是 `struct usb_driver`，`usb_register()` 按 `id_table` 匹配接口。`probe` 收到 `usb_interface`，从端点描述符里找 bulk in/out。数据面用 URB：填 `usb_fill_bulk_urb`，再 `usb_submit_urb`。

本客户机没有匹配的 USB 设备，`probe` 不会跑。实验确认驱动挂上了 USB 总线。`usb_advanced` 在真正匹配时还会做字符设备和 URB。

## 代码架构

```
usb_basic.ko
  usb_register
  /sys/bus/usb/drivers/usb_basic
usb_advanced.ko
  /sys/bus/usb/drivers/usb_advanced
无匹配设备 → 没有 Probing 行
```

`ch08_usb_bus` 是这份代码的副本，不参与 `make all`。

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch20
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

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
- dmesg 有 `registered`，`/sys/bus/usb/drivers/` 下有 `usb_basic` 和 `usb_advanced`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch20
```
