# ch05_i2c_bus

本目录不参与 `make all`，镜像也不会加载这里的文件。能跑的代码在 [../ch15_i2c](../ch15_i2c)。

## 代码知识点

I2C 控制器实现 `i2c_algorithm.master_xfer` 并 `i2c_add_adapter()`。从设备是 `i2c_driver`，没有设备树时用 `i2c_new_client_device()` 放到总线上。

## 代码架构

注册和卸载的顺序与 [../ch15_i2c](../ch15_i2c) 里的模块相同。本目录如果留有同名 `.c`，只作对照，不要在这里 `make` 之后拿去 `insmod`。

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch15
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch15_i2c_i2c_master.ko
insmod ok
+ insmod /lib/modules/driver_test/ch15_i2c_i2c_slave.ko
insmod ok
+ ls /dev/i2c_virt
/dev/i2c_virt
+ dmesg | grep i2c_virt
[    4.975742] i2c_virt_master: registered as i2c-0, virtual slave at 0x50
[    4.982274] i2c_virt_slave 0-0050: device ID register (0x00) = 0x50
[    4.982314] i2c_virt_slave 0-0050: I2C slave probed at addr 0x50 on adapter i2c-virt-master, /dev/i2c_virt created
[    4.982659] i2c_virt_slave: module loaded, /dev/i2c_virt ready
+ echo ab > /dev/i2c_virt
+ dmesg | grep i2c_virt_slave
[    4.982274] i2c_virt_slave 0-0050: device ID register (0x00) = 0x50
[    4.982314] i2c_virt_slave 0-0050: I2C slave probed at addr 0x50 on adapter i2c-virt-master, /dev/i2c_virt created
[    4.982659] i2c_virt_slave: module loaded, /dev/i2c_virt ready
[    5.011232] i2c_virt_slave: wrote 2 byte(s) starting at reg 0x61
+ rmmod i2c_slave
rmmod ok
+ rmmod i2c_master
rmmod ok
[PASS] ch15_i2c
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch15_i2c`
- 回显与可运行那一章相同，并且退出码是 0。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch15
```
