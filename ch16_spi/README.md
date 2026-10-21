# SPI 控制器和从设备

## 代码知识点

SPI 控制器是 `spi_controller`，从设备是 `spi_device`。一次传输是 `spi_transfer`，装进 `spi_message`，由 `spi_sync()` 提交。SPI 是全双工：发出去的每个字节同时收进一个字节。

虚拟控制器把 MOSI 环回到 MISO。`/dev/spi_virt` 的 `write` 把用户缓冲放进 `tx_buf`，环回结果留在 `rx_buf`，随后 `read` 把 `rx_buf` 交回去。

## 代码架构

```
spi_master.ko
  spi_register_controller
spi_slave.ko
  spi_new_device → probe → /dev/spi_virt
write
  spi_message + spi_sync
read
  返回环回的 rx_buf
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch16
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch16_spi_spi_master.ko
insmod ok
+ insmod /lib/modules/driver_test/ch16_spi_spi_slave.ko
insmod ok
+ ls /dev/spi_virt
/dev/spi_virt
+ echo sp > /dev/spi_virt
+ head -c 2 /dev/spi_virt
sp
+ rmmod spi_slave
rmmod ok
+ rmmod spi_master
rmmod ok
[PASS] ch16_spi
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch16_spi`
- `/dev/spi_virt` 存在，并且读回的前两个字节是 `sp`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch16
```
