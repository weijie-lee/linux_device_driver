# 驱动调用内核

`kernel/02_kernel_interface` 看的是内核走进驱动：`really_probe` 调用总线 probe，`vfs_read` 调用 `file_operations.read`。这一步看反方向。驱动拿到用户态指针之后，自己调用内核把字节搬过地址空间边界。

模块用 `misc_register()` 登记 `/dev/virt_call`。缓冲只有 64 字节。一次 `write` 放进内核，下一次打开再 `read` 出来。

## 代码知识点

交互有两段，都发生在同一次系统调用里。

**内核调用驱动。** `vfs_write()` 在 `f_op->write` 非空时直接调用它。跟踪里是 `virt_call_write <-vfs_write`。`vfs_read()` 同样直接调用 `virt_call_read`。

**驱动调用内核。** 用户态指针不能在内核里直接解引用。5.15 的 x86 上，`copy_from_user()` 和 `copy_to_user()` 是内联函数，真正搬字节的是 `_copy_from_user` 和 `_copy_to_user`。这台客户机的 `set_ftrace_filter` 和 `kprobe_events` 都不接受这两个名字。

长度不是编译期常量时，内联进去的 `check_copy_size()` 会先调用 `__check_object_size()`。这是 `CONFIG_HARDENED_USERCOPY` 打开之后的检查，调用者就是驱动函数。跟踪里是：

```
virt_call_write <-vfs_write
__check_object_size <-virt_call_write
```

读的时候调用者换成 `virt_call_read`。`_copy_from_user` 紧跟在这次检查之后执行，只是这一项不会出现在函数跟踪里。

拷贝一结束，驱动调用 `tracing_off()`。否则 shell 接着写 `tracing_on` 时，那一次 `write` 也会进入 `__check_object_size`，窗口里就会多出一条。

`copy_from_user()` 可能睡眠，所以缓冲用 mutex 保护，不放在自旋锁里。超过 64 字节的写入只保留前 64 字节，返回值是实际拷进内核的长度。这个长度一路回到 `write` 系统调用的返回值。

第二次打开设备时偏移从 0 开始，所以读到的是整段缓冲。实验把 `head` 的输出写到文件，避免 shell 自己的 `vfs_read` 混进跟踪窗口。

## 代码架构

```
module_init
  misc_register                         /dev/virt_call

printf ping > /dev/virt_call            ftrace 已经打开
  vfs_write → virt_call_write
    __check_object_size                 然后 _copy_from_user 把字节放进 call_buf
    tracing_off

head -c 4 /dev/virt_call
  vfs_read → virt_call_read
    __check_object_size                 然后 _copy_to_user 把 call_buf 交回用户态
    tracing_off

module_exit
  misc_deregister
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter kernel03
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、CPU 号、进程号每次都不同，对照函数名和 `<-` 后面的调用者即可。

打开 `/dev/virt_call` 时，shell 解析路径会走 `strncpy_from_user`，那几次 `__check_object_size` 的调用者不是驱动。判定只保留调用者是 `virt_call_write` 或 `virt_call_read` 的那一条。

```
======== kernel03_calls ========
+ insmod /lib/modules/driver_test/kernel_03_driver_calls_virt_call.ko
insmod ok
+ test -c /dev/virt_call -o -b /dev/virt_call
node ok
+ printf ping > /dev/virt_call
+ echo 0 > /sys/kernel/tracing/tracing_on
+ echo nop > /sys/kernel/tracing/current_tracer
+ echo function > /sys/kernel/tracing/current_tracer
+ echo virt_call_write > /sys/kernel/tracing/set_ftrace_filter
+ echo __check_object_size >> /sys/kernel/tracing/set_ftrace_filter
+ echo > /sys/kernel/tracing/trace
+ echo 1 > /sys/kernel/tracing/tracing_on
write ok
+ echo 0 > /sys/kernel/tracing/tracing_on
+ head -n 20 /sys/kernel/tracing/trace
# tracer: function
#
# entries-in-buffer/entries-written: 4/4   #P:2
#
#                                _-----=> irqs-off
#                               / _----=> need-resched
#                              | / _---=> hardirq/softirq
#                              || / _--=> preempt-depth
#                              ||| / _-=> migrate-disable
#                              |||| /     delay
#           TASK-PID     CPU#  |||||  TIMESTAMP  FUNCTION
#              | |         |   |||||     |         |
              sh-1       [000] .....     8.217649: __check_object_size <-strncpy_from_user
              sh-1       [000] .....     8.218436: __check_object_size <-strncpy_from_user
              sh-1       [000] .....     8.218966: virt_call_write <-vfs_write
              sh-1       [000] .....     8.218986: __check_object_size <-virt_call_write
+ echo 0 > /sys/kernel/tracing/tracing_on
+ echo nop > /sys/kernel/tracing/current_tracer
write-hits: virt_call_write check_object_size
write order ok
+ head -c 4 /dev/virt_call > /tmp/call_out
+ echo 0 > /sys/kernel/tracing/tracing_on
+ echo nop > /sys/kernel/tracing/current_tracer
+ echo function > /sys/kernel/tracing/current_tracer
+ echo virt_call_read > /sys/kernel/tracing/set_ftrace_filter
+ echo __check_object_size >> /sys/kernel/tracing/set_ftrace_filter
+ echo > /sys/kernel/tracing/trace
+ echo 1 > /sys/kernel/tracing/tracing_on
+ echo 0 > /sys/kernel/tracing/tracing_on
+ head -n 20 /sys/kernel/tracing/trace
# tracer: function
#
# entries-in-buffer/entries-written: 6/6   #P:2
#
#                                _-----=> irqs-off
#                               / _----=> need-resched
#                              | / _---=> hardirq/softirq
#                              || / _--=> preempt-depth
#                              ||| / _-=> migrate-disable
#                              |||| /     delay
#           TASK-PID     CPU#  |||||  TIMESTAMP  FUNCTION
#              | |         |   |||||     |         |
              sh-1       [000] .....     8.661238: __check_object_size <-strncpy_from_user
            head-157     [001] .....     8.663753: __check_object_size <-strncpy_from_user
            head-157     [001] .....     8.663930: __check_object_size <-strncpy_from_user
            head-157     [001] .....     8.664056: __check_object_size <-strncpy_from_user
            head-157     [001] .....     8.664199: virt_call_read <-vfs_read
            head-157     [001] .....     8.664240: __check_object_size <-virt_call_read
+ echo 0 > /sys/kernel/tracing/tracing_on
+ echo nop > /sys/kernel/tracing/current_tracer
read-hits: virt_call_read check_object_size
read order ok
+ cat /tmp/call_out
ping
read ok
+ rmmod virt_call
rmmod ok
+ dmesg | grep virt_call:
[    1.954782] virt_call: loading out-of-tree module taints kernel.
[    1.954956] virt_call: module verification failed: signature and/or required key missing - tainting kernel
[    8.219090] virt_call: stored 4
[    8.664278] virt_call: returned 4
[PASS] kernel03_calls
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] kernel03_calls`
- `write-hits` 整行是 `virt_call_write check_object_size`
- 跟踪原文里能看到 `virt_call_write <-vfs_write` 和 `__check_object_size <-virt_call_write`
- `read-hits` 整行是 `virt_call_read check_object_size`
- 跟踪原文里能看到 `virt_call_read <-vfs_read` 和 `__check_object_size <-virt_call_read`
- `/tmp/call_out` 的内容是 `ping`
- `dmesg` 里有 `virt_call: stored 4` 和 `virt_call: returned 4`

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter kernel03
```
