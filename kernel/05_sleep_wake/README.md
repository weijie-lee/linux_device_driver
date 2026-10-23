# 读者睡下，写者叫醒

`read` 在 `wait_event()` 里进入 `TASK_UNINTERRUPTIBLE`，`write` 放进数据后 `wake_up()`。睡着的读者在 `/proc/PID/status` 里是 `D (disk sleep)`，调用栈停在 `virt_sleep_read`。hung task 检测器只看这种状态。

## 代码知识点

**wait_event 展开在 read 里面。** `schedule()` 的调用者就是 `virt_sleep_read`，不是一个单独的等待函数：

```
virt_sleep_read <-vfs_read
schedule <-virt_sleep_read
virt_sleep_write <-vfs_write
__wake_up <-virt_sleep_write
```

等待条件只读 `sleep_len`，不拿 mutex。任务状态已经不是 `TASK_RUNNING`，这时拿 mutex 可能睡第二次。醒来之后再进锁，把数据拷出来。

**function_graph。** `wake_up()` 是宏，直接调用 `__wake_up()`。再往下是等待队列的唤醒函数。`wait_event` 用的是 `autoremove_wake_function`，它调用 `default_wake_function`，再进 `try_to_wake_up`。`max_graph_depth` 要写到 8 才收得住这一层：

```
virt_sleep_write() {
  __wake_up() {
    __wake_up_common_lock() {
      __wake_up_common() {
        autoremove_wake_function() {
          default_wake_function() {
            try_to_wake_up() {
```

**调度事件。** `sched_waking` 记“谁要被叫醒”，`sched_switch` 记“它什么时候真正上 CPU”。这两条不一定在同一个 CPU 上。事件名写进 `set_event` 时用冒号：`sched:sched_waking`。

**hung task。** 把 `hung_task_timeout_secs` 改成 2 会叫醒检测线程。读者一直停在 D 状态，下一次检查打印：

```
INFO: task head:227 blocked for more than 2 seconds.
task:head            state:D
virt_sleep_read+0x75/0x139 [virt_sleep]
```

栈顶是 `schedule`，调用者是 `virt_sleep_read`。实验结束前必须再 `write` 一次，否则读者还占着模块，`rmmod` 失败。

## 代码架构

```
module_init
  misc_register                         /dev/virt_sleep
                                        sleepers、wakeups

read
  sleepers++
  wait_event(sleep_wq, sleep_len > 0)   TASK_UNINTERRUPTIBLE，里面调用 schedule
  拷出数据，sleepers--

write
  放入数据
  wakeups++
  wake_up → __wake_up → autoremove_wake_function → default_wake_function → try_to_wake_up
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter kernel05
```

下面这份回显来自镜像里的一次实际运行。进程号每次不同。

```
======== kernel05_sleep_wake ========
+ insmod /lib/modules/driver_test/kernel_05_sleep_wake_virt_sleep.ko
insmod ok
+ cat /sys/class/misc/virt_sleep/sleepers
0
reader 162 is sleeping
+ grep State /proc/162/status
State:	D (disk sleep)
+ cat /proc/162/stack
[<0>] virt_sleep_read+0x75/0x139 [virt_sleep]
[<0>] vfs_read+0x9f/0x1a0
            head-162     [000] .....     8.220012: virt_sleep_read <-vfs_read
            head-162     [000] .....     8.220051: schedule <-virt_sleep_read
              sh-1       [000] .....     8.276193: virt_sleep_write <-vfs_write
              sh-1       [000] .....     8.276271: __wake_up <-virt_sleep_write
sleep-hits: read schedule write wake_up
sleep order ok
ping
 0)               |  virt_sleep_write [virt_sleep]() {
 0)               |    __wake_up() {
 0)               |      __wake_up_common_lock() {
 0)               |        __wake_up_common() {
 0)               |          autoremove_wake_function() {
 0)               |            default_wake_function() {
 0)               |              try_to_wake_up() {
pong
              sh-1       [000] d....    16.223748: sched_waking: comm=head pid=217 prio=120 target_cpu=000
          <idle>-0       [001] d....    16.224103: sched_switch: prev_comm=swapper/1 prev_pid=0 prev_prio=120 prev_state=R ==> next_comm=head next_pid=217 next_prio=120
sched-hits: waking switch_in
sched order ok
abcd
hung task reported
[   18.396460] INFO: task head:227 blocked for more than 2 seconds.
[   18.398046] task:head            state:D stack:    0 pid:  227 ppid:     1 flags:0x00000002
[   18.398554]  virt_sleep_read+0x75/0x139 [virt_sleep]
done
+ cat /sys/class/misc/virt_sleep/wakeups
4
+ rmmod virt_sleep
rmmod ok
[PASS] kernel05_sleep_wake
```

成功时客户机打印 `[PASS] kernel05_sleep_wake`。四次写分别是 `ping`、`pong`、`abcd`、`done`，`wakeups` 因此是 4。读者的状态行包含 `D (disk sleep)`，hung task 的栈里有 `virt_sleep_read`。
