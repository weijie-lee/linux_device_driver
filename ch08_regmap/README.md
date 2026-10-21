# regmap

## 代码知识点

regmap 把寄存器读写收成 `regmap_read` / `regmap_write` / `regmap_update_bits`。底层可以是 I2C、SPI 或 MMIO。本模块没有总线，用 `regmap_init()` 配一对自定义 `reg_read` / `reg_write`，寄存器是一块内存数组，寄存器 N 的初值是 `N * 0x10`。

debugfs 在 `/sys/kernel/debug/regmap/regmap_demo/registers`。用户程序走 `/dev/regmap_demo` 的 ioctl。

## 代码架构

```
kzalloc 寄存器数组
regmap_init(自定义 bus)
misc_register → /dev/regmap_demo
ioctl
  regmap_read / regmap_write
rmmod
  regmap_exit + misc_deregister
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch08
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch08_regmap_regmap_demo.ko
insmod ok
+ ls /dev/regmap_demo
/dev/regmap_demo
+ dmesg | grep regmap_demo
[    4.451751] regmap_demo: initialized, 256 registers (32-bit each)
[    4.451794] regmap_demo: debugfs: /sys/kernel/debug/regmap/regmap_demo/registers
+ rmmod regmap_demo
rmmod ok
[PASS] ch08_regmap
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch08_regmap`
- `/dev/regmap_demo` 存在，并且 dmesg 里有 `initialized`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch08
```
