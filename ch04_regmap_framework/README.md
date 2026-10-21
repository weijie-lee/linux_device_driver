# ch04_regmap_framework

本目录不参与 `make all`，镜像也不会加载这里的文件。能跑的代码在 [../ch08_regmap](../ch08_regmap)。

## 代码知识点

regmap 把 `regmap_read` / `regmap_write` 从 I2C、SPI、MMIO 后面抽出来。自定义总线时用 `regmap_init()`，配 `reg_read` 和 `reg_write`。

## 代码架构

注册和卸载的顺序与 [../ch08_regmap](../ch08_regmap) 里的模块相同。本目录如果留有同名 `.c`，只作对照，不要在这里 `make` 之后拿去 `insmod`。

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
- 回显与可运行那一章相同，并且退出码是 0。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch08
```
