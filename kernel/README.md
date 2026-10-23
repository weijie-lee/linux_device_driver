# 内核主线

`ch*` 和 `learn/` 是一块块具体的驱动。`kernel/` 反过来读操作系统：驱动核心怎么表示总线、设备、驱动，以及绑定是在哪一层发生的。

这一层不插进章节编号。

1. [`01_driver_framework`](01_driver_framework)：拆开 5.15 `drivers/base` 的注册、匹配、probe 和移除。
2. [`02_kernel_interface`](02_kernel_interface)：用 ftrace 看内核从哪个函数走进这些回调，以及 `chrdev_open` / `vfs_read` 怎么走进 `file_operations`。
3. [`03_driver_calls`](03_driver_calls)：反过来看驱动调用内核。`copy_from_user` / `copy_to_user` 在这台内核上内联，函数跟踪里能看见的是 `__check_object_size`，调用者是驱动自己的 read/write。
4. [`04_user_paths`](04_user_paths)：ioctl、32 位 `compat_ptr_ioctl`、sysfs show、proc seq_file，以及 `capable(CAP_SYS_ADMIN)`。
5. [`05_sleep_wake`](05_sleep_wake)：`wait_event` / `wake_up`、`schedule`、function_graph、`sched_waking` / `sched_switch`、hung task。
6. [`06_irq_bottom`](06_irq_bottom)：irqdomain 加 irq_work。硬中断、tasklet、中断线程、工作队列，外加自定义 `TRACE_EVENT`。
7. [`07_lock_context`](07_lock_context)：mutex 和自旋锁、`preempt_count`、每 CPU 计数，以及一次 soft lockup。
8. [`08_pci_mmio`](08_pci_mmio)：QEMU edu 的 BAR0。识别字 `0x010000ed`，活性寄存器写入后读回按位取反。
9. [`09_msi_irq`](09_msi_irq)：同一块 edu 用 MSI 算阶乘。写入 5，读回 120，`/proc/interrupts` 加一。
