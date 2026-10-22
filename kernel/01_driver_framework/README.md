# 驱动程序框架

这一目录不属于 `ch*`。`ch02` 的字符设备、`ch06` 的 platform、`ch19` 的 PCI 都是某一种驱动。这里拆的是它们共用的内核框架：5.15 的 `drivers/base`。

模块 `virt_framework.ko` 自己注册一条名为 `lddfw` 的总线，先挂上两台设备，再注册驱动。一台设备的 id 是 `sensor`，和驱动匹配；另一台是 `other`，匹配失败。`/sys/bus/lddfw/trace` 把回调按发生顺序记成一行，用来核对内核真正走了哪条路径。

## 代码知识点

驱动核心里每样东西都是一个 `kobject`。`kobject` 进了 sysfs，就在 `/sys` 下有一个目录；引用计数归零时调用 `release`。总线、设备、驱动都嵌着它，所以下面三张表才能出现：

| 对象 | 5.15 的结构 | 注册之后出现在 |
| --- | --- | --- |
| 总线 | `struct bus_type`（`include/linux/device/bus.h`） | `/sys/bus/lddfw/` |
| 设备 | `struct device`（`include/linux/device.h`） | `/sys/bus/lddfw/devices/lddfw-sensor` |
| 驱动 | `struct device_driver`（`include/linux/device/driver.h`） | `/sys/bus/lddfw/drivers/lddfw_drv` |

`struct device` 很少单独出现，一般嵌在更高一层的结构里，用 `container_of` 取回来。本模块的 `lddfw_dev` 嵌着 `device`，`lddfw_driver` 嵌着 `device_driver`。platform 是 `platform_device` / `platform_driver`，PCI 是 `pci_dev` / `pci_driver`，嵌法相同。

### 注册函数和 5.15 里接着被调用的函数

头文件包里没有 `drivers/base/*.c`。下面的调用关系对的是 5.15.y 的 `drivers/base/core.c`、`bus.c`、`dd.c`。

1. `bus_register()` 建立总线的 kset，并放上 `drivers_autoprobe`、`drivers_probe`。`drivers_autoprobe` 默认是 1：之后注册驱动时，核心会主动给总线上已有的设备做匹配。
2. `device_register()` 等于 `device_initialize()` 加 `device_add()`。`device_add()` 把设备链进总线的设备表，然后 `kobject_uevent(KOBJ_ADD)`。这一步会调用 `bus->uevent`。此时驱动还没注册，`bus_probe_device()` 找不到驱动，`match` 不会跑。
3. `driver_register()` 走到 `bus_add_driver()` → `driver_attach()`。`driver_attach()` 对总线上每台设备调用 `__driver_attach()`。
4. `__driver_attach()` 先 `driver_match_device()`。这个内联函数在 `drivers/base/base.h`：总线提供了 `match` 就调用它，返回值大于 0 才算匹配上。`lddfw-other` 返回 0，probe 不发生。
5. 匹配成功后进入 `really_probe()`。它先把 `dev->driver` 指到这名驱动，再调用 `call_driver_probe()`。
6. `call_driver_probe()` 的规则是：`bus->probe` 非空就只调总线的 probe，**不再**调用 `device_driver.probe`。总线若要执行驱动的 probe，必须自己转调。`platform_probe()` 就是先做时钟和电源域，再调用 `platform_driver.probe`。本模块的 `lddfw_bus_probe()` 同样转调 `driver->probe`。
7. probe 返回 0 之后，`driver_bound()` 发出 `KOBJ_BIND`，`bus->uevent` 再被调用一次。所以匹配上的那台设备在 trace 里有两次 `uevent`：第一次是设备加入，第二次是绑定完成。

移除是对称的，写在 `drivers/base/dd.c` 的 `__device_release_driver()`：

- `bus->remove` 非空就只调它，驱动的 `remove` 要由总线转调。
- 然后 `dev->driver` 被清空，再发 `KOBJ_UNBIND`，又进一次 `bus->uevent`。
- `device_unregister()` 再发 `KOBJ_REMOVE`，最后 `put_device()`。引用归零时 `device.release` 被调用。

`device` 没有 `release`，核心会打出 `does not have a release() function`。谁分配设备，谁提供 `release`。本模块的两台设备是静态变量，`release` 只记一行日志，不 `kfree`。`root_device_register()` 创建的父设备由核心自己 `kfree`。

`device_add()` 失败之后不能 `kfree(dev)`。`device_initialize()` 已经持有一次引用，失败路径只 `put_device()`。

### 已经写过的三种驱动，分别用到这套框架的哪一层

| 章节 | 它注册的是什么 | 和本框架的关系 |
| --- | --- | --- |
| `ch02_char_basic` | `cdev` 和 `file_operations` | 不走 `bus->match`。`read` / `write` 由 VFS 经 `cdev` 分发。`device_create()` 仍会做一个 `struct device`，那只是为了 `/dev` 节点和 sysfs |
| `ch06_platform` | `platform_device`、`platform_driver` | `platform_bus_type.match` 是 `platform_match()`：先看 `driver_override`，再是设备树、ACPI、`id_table`，最后比较设备名和驱动名。`platform_bus_type.probe` 是 `platform_probe()`，里面才调用 `platform_driver.probe`。这台客户机没开 `CONFIG_OF`，名字相同就会绑上 |
| `ch19_pci_bus` | `pci_driver` | `pci_bus_type.match` 是 `pci_bus_match()`，用 `pci_device_id` 表。客户机里没有表里的那块设备，所以驱动能注册，probe 不跑 |

`/sys/bus/platform` 和 `/sys/bus/pci` 是内核自带的总线。`/sys/bus/lddfw` 是这个模块注册的，布局和它们相同：下面都有 `devices/` 和 `drivers/`。绑定成功时，设备目录里会出现 `driver` 符号链接，驱动目录里会出现这台设备的名字。`lddfw-other` 没有这个链接。

## 代码架构

```
module_init
  bus_register                         drivers/base/bus.c
  device_register(lddfw-sensor)        drivers/base/core.c device_add
    kobject_uevent(KOBJ_ADD)           bus->uevent
    bus_probe_device                   总线上还没有驱动，match 不跑
  device_register(lddfw-other)         同上
  driver_register                      bus_add_driver → driver_attach
    match lddfw-sensor 1               bus->match
    bus_probe → driver_probe           really_probe → call_driver_probe
    uevent lddfw-sensor                driver_bound 的 KOBJ_BIND
    match lddfw-other 0                返回 0，不 probe
module_exit
  driver_unregister
    bus_remove → driver_remove         __device_release_driver
    uevent                             KOBJ_UNBIND
  device_unregister
    uevent                             KOBJ_REMOVE
    release                            引用归零
  bus_unregister
```

`/sys/bus/lddfw/trace` 只在总线还在时可读。绑定顺序在 `rmmod` 之前读；`remove`、`release`、`bus_unregister` 发生在拆除过程中，从 `dmesg` 里核对。

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter kernel01
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/kernel_01_driver_framework_virt_framework.ko
insmod ok
+ cat /sys/bus/lddfw/trace
bus_register > uevent lddfw-sensor > uevent lddfw-other > match lddfw-sensor 1 > bus_probe lddfw-sensor > driver_probe lddfw-sensor > uevent lddfw-sensor > match lddfw-other 0
trace ok
+ cat /sys/bus/lddfw/drivers_autoprobe
1
+ cat /sys/bus/lddfw/devices/lddfw-sensor/bound /sys/bus/lddfw/devices/lddfw-sensor/id
1
sensor
+ cat /sys/bus/lddfw/devices/lddfw-other/bound /sys/bus/lddfw/devices/lddfw-other/id
0
other
+ test -e /sys/bus/lddfw/devices/lddfw-sensor/driver
sensor bound
+ test ! -e /sys/bus/lddfw/devices/lddfw-other/driver
other unbound
+ ls /sys/bus/lddfw/drivers/lddfw_drv
bind
lddfw-sensor
module
uevent
unbind
+ ls /sys/bus
acpi
auxiliary
clockevents
clocksource
container
counter
cpu
dax
edac
event_source
gpio
hid
i2c
i3c
iio
isa
lddfw
machinecheck
mdio_bus
media
memory
mipi-dsi
mmc
nd
node
nvmem
pci
pci-epf
pci_express
platform
pnp
rapidio
scsi
sdio
serial
serio
spi
spmi
usb
virtio
vme
w1
workqueue
xen
xen-backend
+ rmmod virt_framework
rmmod ok
+ dmesg | grep lddfw:
[    1.889175] lddfw: bus_register
[    1.889452] lddfw: uevent lddfw-sensor
[    1.889559] lddfw: uevent lddfw-other
[    1.889630] lddfw: match lddfw-sensor 1
[    1.889711] lddfw: bus_probe lddfw-sensor
[    1.889734] lddfw: driver_probe lddfw-sensor
[    1.889830] lddfw: uevent lddfw-sensor
[    1.889846] lddfw: match lddfw-other 0
[    1.993423] lddfw: bus_remove lddfw-sensor
[    1.993465] lddfw: driver_remove lddfw-sensor
[    1.993566] lddfw: uevent lddfw-sensor
[    1.993804] lddfw: uevent lddfw-sensor
[    1.993829] lddfw: release lddfw-sensor
[    1.993889] lddfw: uevent lddfw-other
[    1.993899] lddfw: release lddfw-other
[    1.994016] lddfw: bus_unregister
[PASS] kernel01_framework
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] kernel01_framework`
- `/sys/bus/lddfw/trace` 的整行是 `bus_register > uevent lddfw-sensor > uevent lddfw-other > match lddfw-sensor 1 > bus_probe lddfw-sensor > driver_probe lddfw-sensor > uevent lddfw-sensor > match lddfw-other 0`
- `lddfw-sensor` 的 `bound` 是 `1`，`id` 是 `sensor`，并且存在 `driver` 链接
- `lddfw-other` 的 `bound` 是 `0`，`id` 是 `other`，没有 `driver` 链接，也不出现在 `lddfw_drv` 目录里
- `/sys/bus` 里同时能看到 `lddfw`、`platform`、`pci`
- `rmmod` 之后 `dmesg` 里有 `bus_remove lddfw-sensor`、`driver_remove lddfw-sensor`、`release lddfw-sensor`、`release lddfw-other`、`bus_unregister`

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter kernel01
```
