# I2C 控制器和从设备

## 代码知识点

I2C 分主控制器和从设备。控制器实现 `i2c_algorithm` 的 `master_xfer`，用 `i2c_add_adapter()` 注册，出现 `i2c-N`。从设备是 `i2c_driver`，`probe` 在地址匹配之后调用。

没有设备树时，从设备模块用 `i2c_new_client_device()` 在虚拟总线上放一个地址 `0x50` 的客户端。`/dev/i2c_virt` 的写缓冲第一字节是寄存器地址，后面是数据，对应 `i2c_master_send()`。

## 代码架构

```
i2c_master.ko
  i2c_add_adapter
i2c_slave.ko
  i2c_new_client_device(addr 0x50)
  probe → /dev/i2c_virt
write
  buf[0] = 寄存器，buf[1..] = 数据
  i2c_master_send
```

先加载 master，再加载 slave。卸载顺序相反。

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
- `/dev/i2c_virt` 存在，dmesg 有 `registered as i2c-`，写入之后有 `wrote`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch15
```
