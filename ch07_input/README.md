# input 子系统

## 代码知识点

输入设备不自己发明记录格式。驱动填 `input_dev` 的事件位图，用 `input_report_key()` 加上 `input_sync()` 上报。用户空间读 `/dev/input/eventN`，每条是 `struct input_event`。

本模块注册一块虚拟键盘，再在 `/proc/virt_kbd` 上收一个键码。写入 `30` 就是上报键 30 的按下和松开。

## 代码架构

```
input_allocate_device
  set_bit(EV_KEY)
  set_bit(KEY_A) 等
input_register_device → /dev/input/eventN
/proc/virt_kbd 的 write
  input_report_key(press) + input_sync
  input_report_key(release) + input_sync
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch07
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch07_input_input_demo.ko
insmod ok
+ ls /dev/input
event0
event1
event2
mice
+ echo 'key 30' > /proc/virt_kbd
+ dmesg | grep virt_kbd
[    4.311604] virt_kbd: registered, use /proc/virt_kbd to inject key events
[    4.325661] virt_kbd: reported key 30 (press+release)
+ rmmod input_demo
rmmod ok
[PASS] ch07_input
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch07_input`
- `/dev/input` 里出现 `event`，并且 dmesg 有 `reported key 30`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch07
```
