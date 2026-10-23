# 锁、抢占计数和软锁

这台内核是 `PREEMPT_VOLUNTARY`，没有打开 `CONFIG_PREEMPT_COUNT`。`spin_lock()` 不增加 `preempt_count` 的低 8 位，`preempt_disable()` 只剩编译屏障。硬中断入口仍然把 `HARDIRQ_OFFSET`（`1 << 16`，十进制 65536）加进同一个计数，所以 `in_hardirq()` 在硬中断里仍为真。

## 代码知识点

**三种上下文的计数。** 进程里读到 0。拿着自旋锁读，还是 0。拿着 mutex 读，还是 0。`IRQ_WORK_HARD` 里读到 65536，`in_hardirq()` 为 1：

```
task=0 spin=0 mutex=0 hard=65536 hard_in_irq=1
```

**每 CPU 和原子变量。** `on_each_cpu()` 在两个 CPU 上各加一次。每 CPU 变量加起来等于原子变量：

```
cpu0=1 cpu1=1 sum=2 atomic=2
```

**mutex 可以睡。** `msleep()` 进 `schedule_timeout()`。function_graph 从 `virt_lock_mutex_sleep` 展开：

```
virt_lock_mutex_sleep() {
  msleep() {
    schedule_timeout() {
```

**空转不能睡，软锁检测器会看见。** `virt_lock_spin()` 开着中断空转 5 秒，中间不调用 `schedule()`。`watchdog_thresh` 写成 1 之后，软锁阈值是 2 秒。检测器打印：

```
watchdog: BUG: soft lockup - CPU#1 stuck for 3s! [sh:1]
RIP: 0010:virt_lock_spin+0x24/0x2c [virt_lock]
```

调用栈从 `lockup_store` 走回 `dev_attr_store` ← `sysfs_kf_write`。实验先装好 function_graph，再改阈值。阈值如果在切换 tracer 之前就降下来，切换本身会先被判成一次 soft lockup。

## 代码架构

```
module_init
  on_each_cpu                         每个 CPU 的 per-cpu 计数和 atomic 各加 1
  misc_register                       /dev/virt_lock
                                      context、spread、mutex_sleep、lockup

cat context
  进程 / spin_lock / mutex 里读 preempt_count
  irq_work(IRQ_WORK_HARD) 里再读一次

echo 1 > mutex_sleep
  virt_lock_mutex_sleep → msleep → schedule_timeout

echo 1 > /proc/sys/kernel/watchdog_thresh
echo 1 > lockup
  virt_lock_spin 空转，软锁检测器在中断里看到这个 CPU 没有调度
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter kernel07
```

下面这份回显来自镜像里的一次实际运行。

```
======== kernel07_lock_context ========
+ insmod /lib/modules/driver_test/kernel_07_lock_context_virt_lock.ko
insmod ok
+ cat /sys/class/misc/virt_lock/context
task=0 spin=0 mutex=0 hard=65536 hard_in_irq=1
+ cat /sys/class/misc/virt_lock/spread
cpu0=1 cpu1=1 sum=2 atomic=2
 1)               |  virt_lock_mutex_sleep [virt_lock]() {
 1)               |    msleep() {
 1)               |      schedule_timeout() {
 1)               |  virt_lock_spin [virt_lock]() {
[   17.221691] watchdog: BUG: soft lockup - CPU#1 stuck for 3s! [sh:1]
[   17.223621] RIP: 0010:virt_lock_spin+0x24/0x2c [virt_lock]
[   17.224272] Call Trace:
[   17.224472]  ? ftrace_graph_caller+0xc0/0xc0
[   17.224550]  lockup_store+0x1f/0x39 [virt_lock]
[   17.224593]  dev_attr_store+0x17/0x30
[   17.224593]  sysfs_kf_write+0x3e/0x50
[   17.224593]  kernfs_fop_write_iter+0x13b/0x1c0
+ rmmod virt_lock
rmmod ok
[PASS] kernel07_lock_context
```

成功时客户机打印 `[PASS] kernel07_lock_context`。`context` 里进程、自旋锁、mutex 的计数都是 0，硬中断是 65536。`spread` 是 `cpu0=1 cpu1=1 sum=2 atomic=2`。软锁那一行的 RIP 是 `virt_lock_spin`。function_graph 里 `virt_lock_spin` 下面没有 `schedule`。
