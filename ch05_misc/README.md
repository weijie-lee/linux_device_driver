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

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch05_misc_misc_demo.ko
insmod ok
+ ls /dev/misc_demo
/dev/misc_demo
+ echo miscdata > /dev/misc_demo
+ head -c 8 /dev/misc_demo
miscdata
+ rmmod misc_demo
rmmod ok
[PASS] ch05_misc
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch05_misc`
- `head` 读回 `miscdata`，并且 `/dev/misc_demo` 存在。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch05
```
