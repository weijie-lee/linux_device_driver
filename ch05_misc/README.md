# misc 杂项设备

## 代码知识点

杂项设备共用主设备号 10。`misc_register()` 一次做完次设备号、`cdev` 和 `/dev` 节点，不用自己 `alloc_chrdev_region` 和 `class_create`。适合只有一个节点的小设备。

本模块的缓冲区在 `misc_demo_priv` 里，`mutex` 盖住读写。`write` 覆盖整段缓冲并记下长度，`read` 按 `*ppos` 往后读，读完返回 0。

## 代码架构

```
misc_register(miscdevice)
  .name = misc_demo → /dev/misc_demo
  .fops = open/read/write/ioctl
write → 拷进内核缓冲
read  → copy_to_user，直到 data_len
rmmod → misc_deregister
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch05
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch05_misc_misc_demo.ko
insmod ok
+ ls /dev/misc_demo
/dev/misc_demo
+ test -c /dev/misc_demo -o -b /dev/misc_demo
node ok
+ echo miscdata > /dev/misc_demo
+ head -c 8 /dev/misc_demo
miscdata
+ /bin/test_misc
=== Ch05 Misc Device Driver Test ===

[TC01] open/close
  [PASS] open /dev/misc_demo
[TC02] write
  [PASS] write returns correct byte count
[TC03] read
  [PASS] read returns same byte count as written
  [PASS] read data matches written data
[TC04] ioctl MISC_GET_LEN
  [PASS] ioctl MISC_GET_LEN returns 0
  [PASS] MISC_GET_LEN returns correct length
  data_len = 19 (expected 19)
[TC05] ioctl MISC_CLEAR
  [PASS] ioctl MISC_CLEAR returns 0
[TC06] read after clear
  [PASS] read after clear returns 0 (EOF)

=== Test Summary ===
PASS: 8  FAIL: 0
+ rmmod misc_demo
rmmod ok
[PASS] ch05_misc
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch05_misc`
- `node ok`，`head` 读回 `miscdata`
- 用户态程序 `test_misc` 的结尾是 `PASS: 8  FAIL: 0`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch05
```
