# TTY

## 代码知识点

TTY 驱动用 `tty_alloc_driver()` 分配，填 `tty_operations` 的 `open`、`close`、`write`、`write_room`，再 `tty_register_driver()`。`tty_port` 管打开计数和缓冲。

名字是 `ttyVIRT`，类型是串口，9600 8N1。`write` 接受字节并直接返回长度，没有对端。

## 代码架构

```
tty_alloc_driver(1)
  driver_name = virt_tty
  name = ttyVIRT
tty_set_operations
tty_register_device → /dev/ttyVIRT
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch35
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch35_tty_virt_tty.ko
insmod ok
+ lsmod | grep virt_tty
virt_tty               16384  0 
+ find /sys /dev /proc -name '*ttyVIRT*' 2>/dev/null | head -n 30
/sys/class/tty/ttyVIRT0
/sys/devices/virtual/tty/ttyVIRT0
/dev/ttyVIRT0
+ rmmod virt_tty
rmmod ok
[PASS] ch35_tty
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch35_tty`
- 设备节点 `/dev/ttyVIRT` 存在。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch35
```
