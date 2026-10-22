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

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch08_regmap_regmap_demo.ko
insmod ok
+ ls /dev/regmap_demo
/dev/regmap_demo
+ test -c /dev/regmap_demo -o -b /dev/regmap_demo
node ok
+ dmesg | grep regmap_demo
[    4.530168] regmap_demo: initialized, 256 registers (32-bit each)
[    4.530217] regmap_demo: debugfs: /sys/kernel/debug/regmap/regmap_demo/registers
+ /bin/test_regmap
=== Ch08 Regmap Driver Test ===

[TC01] Read initial register value
  [PASS] ioctl READ returns 0
  reg[5] = 0x50 (expected 0x50)
  [PASS] reg[5] initial value is 0x50
[TC02] Write register
  [PASS] ioctl WRITE returns 0
[TC03] Read back written value
  reg[10] = 0xdeadbeef (expected 0xDEADBEEF)
  [PASS] read back matches written value
[TC04] Update bits (mask=0xFF0000FF, val=0xAB000000)
  [PASS] ioctl UPDBITS returns 0
[TC05] Verify update_bits result
  reg[10] = 0xabadbe00 (expected 0xABADBE00)
  [PASS] update_bits result is correct

=== Test Summary ===
PASS: 6  FAIL: 0
+ rmmod regmap_demo
rmmod ok
[PASS] ch08_regmap
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch08_regmap`
- `node ok`，dmesg 里有 `initialized`
- 用户态程序 `test_regmap` 的结尾是 `PASS: 6  FAIL: 0`，其中 `reg[5] = 0x50`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch08
```
