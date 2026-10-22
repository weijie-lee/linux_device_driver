# PCI 驱动

## 代码知识点

PCI 驱动是 `struct pci_driver`。`id_table` 里是厂商号和设备号，`pci_register_driver()` 把它挂到 PCI 总线上。总线上出现匹配设备时才调用 `probe`。`probe` 里典型顺序是 `pci_enable_device`、`pci_request_regions`、`pci_iomap`。

本客户机没有厂商 `0x1234`、设备 `0x5678` 的 PCI 设备，所以 `probe` 不会跑。实验证明的是驱动注册成功，不是 BAR 已经映射。`pci_advanced` 在匹配设备上还会申请 MSI 和 DMA 缓冲，并做一个字符设备。

## 代码架构

```
pci_basic.ko
  pci_register_driver
  id_table: 1234:5678
  无匹配设备 → 只有 registered，没有 Probing
pci_advanced.ko
  同样先注册
  匹配之后才会 iomap / 字符设备
```

`ch07_pci_bus` 是这份代码的副本，不参与 `make all`。

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch19
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

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
- 两个模块的 dmesg 都有 `PCI driver registered`，并且都能 `rmmod`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch19
```
