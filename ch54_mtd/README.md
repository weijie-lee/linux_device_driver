# MTD

## 代码知识点

MTD 是原始闪存或 RAM 的线性映射，不带文件系统。`mtd_info` 填 `size`、`erasesize`、`_read`、`_write`、`_erase`，`mtd_device_register()` 得到 `/dev/mtdN`。

本设备 64 KiB，擦除块 4 KiB，初值全是 `0xff`。擦除把区间写成 `0xff`，读写是 `memcpy`。

## 代码架构

```
mtd_info
  name = virt-mtd
  size = 64 KiB，erasesize = 4096
  _read / _write / _erase
mtd_device_register
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch54
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch54_mtd_virt_mtd.ko
insmod ok
+ lsmod | grep virt_mtd
virt_mtd               16384  0 
mtd                    77824  2 virt_mtd
+ find /sys /dev /proc -name '*virt-mtd*' 2>/dev/null | head -n 30

+ cat /sys/class/mtd/mtd*/name
virt-mtd
+ rmmod virt_mtd
rmmod ok
[PASS] ch54_mtd
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch54_mtd`
- `/sys/class/mtd/mtd*/name` 是 `virt-mtd`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch54
```
