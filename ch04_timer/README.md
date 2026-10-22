# 内核定时器：/dev/second

## 代码知识点

`struct timer_list` 挂在内核定时器轮上。到期后在软中断里调用回调。`timer_setup()` 绑定回调，`add_timer()` 开始计时，周期定时器在回调里 `mod_timer(jiffies + HZ)`。回调不能睡眠。秒数用 `atomic_t` 累加，因为回调和 `read` 可能在不同 CPU 上。

`/dev/second` 的 `read` 用 `put_user()` 把这个整数交给用户空间，所以它是 4 字节二进制，不是 ASCII。`open` 时把计数清零并 `add_timer`。`release` 必须 `del_timer_sync()`，等回调结束再返回，避免卸载后回调还在跑。

## 代码架构

```
insmod second.ko
  alloc_chrdev_region → cdev_add → class_create → device_create("/dev/second")
open
  atomic_cmpxchg(busy, 0, 1)：已经有人打开则返回 -EBUSY
  timer_setup + add_timer，counter = 0
定时器回调（每秒）
  atomic_inc + mod_timer
read
  put_user(counter)
release
  del_timer_sync，busy = 0
```

只有一个 `timer_list`。定时器还活着时再 `add_timer` 会把同一个节点重复挂上链表，定时器轮在 `LIST_POISON` 上崩溃。所以 `open` 用 `busy` 只放行一个打开者，第二次返回 `-EBUSY`。实验打开一次，等两秒，从同一个文件描述符里读，并且在它还开着时再试一次 `open`，应当被拒绝。

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch04
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch04_timer_second.ko
insmod ok
+ ls /dev/second
/dev/second
+ test -c /dev/second -o -b /dev/second
node ok
+ exec 3<>/dev/second
+ sleep 2
+ if ( : < /dev/second ) 2>/dev/null; then echo second-open-allowed; else echo second-open-refused; fi
second-open-refused
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
- `node ok`
- 第一个文件描述符还开着时，第二次 `open` 被拒绝，回显 `second-open-refused`
- 读到的秒数大于等于 1，并且 `rmmod second` 打印 `rmmod ok`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch04
```
