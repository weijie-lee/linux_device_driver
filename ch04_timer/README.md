# 内核定时器：/dev/second

## 代码知识点

`struct timer_list` 挂在内核定时器轮上。到期后在软中断里调用回调。`timer_setup()` 绑定回调，`add_timer()` 开始计时，周期定时器在回调里 `mod_timer(jiffies + HZ)`。回调不能睡眠。秒数用 `atomic_t` 累加，因为回调和 `read` 可能在不同 CPU 上。

`/dev/second` 的 `read` 用 `put_user()` 把这个整数交给用户空间，所以它是 4 字节二进制，不是 ASCII。`open` 时把计数清零并 `add_timer`。`release` 必须 `del_timer_sync()`，等回调结束再返回，避免卸载后回调还在跑。

## 代码架构

```
insmod second.ko
  alloc_chrdev_region → cdev_add → class_create → device_create("/dev/second")
open
  timer_setup + add_timer，counter = 0
定时器回调（每秒）
  atomic_inc + mod_timer
read
  put_user(counter)
release
  del_timer_sync
```

本驱动没有打开计数。定时器还活着时再 `open` 一次会把同一个 `timer_list` 重复挂上链表。实验只打开一次，等两秒，从同一个文件描述符里读。

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch04
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch04_timer_second.ko
insmod ok
+ ls /dev/second
/dev/second
+ exec 3<>/dev/second
+ sleep 2
+ head -c 4 <&3 > /tmp/sec
+ od -An -tu4 /tmp/sec
          1
+ rmmod second
rmmod ok
[PASS] ch04_second
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch04_second`
- 读到的秒数大于等于 1，并且 `rmmod second` 打印 `rmmod ok`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch04
```
