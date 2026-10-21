# ch07_pci_bus

本目录不参与 `make all`，镜像也不会加载这里的文件。能跑的代码在 [../ch19_pci_bus](../ch19_pci_bus)。

## 代码知识点

PCI 驱动是 `pci_driver` 加 `id_table`。`pci_register_driver()` 之后，只有总线上出现匹配的厂商号和设备号才会 `probe`。

## 代码架构

注册和卸载的顺序与 [../ch19_pci_bus](../ch19_pci_bus) 里的模块相同。本目录如果留有同名 `.c`，只作对照，不要在这里 `make` 之后拿去 `insmod`。

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch19
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch19_pci_bus_pci_basic.ko
insmod ok
+ dmesg | grep -F 'PCI driver registered'
[    6.462280] [PCI BASIC] PCI driver registered successfully
+ ls /sys/bus/pci/drivers/pci_basic /sys/bus/usb/drivers/pci_basic 2>/dev/null
/sys/bus/pci/drivers/pci_basic:
bind
module
new_id
remove_id
uevent
unbind
+ rmmod pci_basic
rmmod ok
+ insmod /lib/modules/driver_test/ch19_pci_bus_pci_advanced.ko
insmod ok
+ dmesg | grep -F 'PCI driver registered'
[    6.462280] [PCI BASIC] PCI driver registered successfully
[    6.513307] [PCI ADV] PCI driver registered successfully
+ rmmod pci_advanced
rmmod ok
[PASS] ch19_pci
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch19_pci`
- 回显与可运行那一章相同，并且退出码是 0。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch19
```
