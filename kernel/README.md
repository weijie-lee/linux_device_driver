# 内核主线

`ch*` 和 `learn/` 是一块块具体的驱动。`kernel/` 反过来读操作系统：驱动核心怎么表示总线、设备、驱动，以及绑定是在哪一层发生的。

这一层不插进章节编号。

1. [`01_driver_framework`](01_driver_framework)：拆开 5.15 `drivers/base` 的注册、匹配、probe 和移除。
2. [`02_kernel_interface`](02_kernel_interface)：用 ftrace 看内核从哪个函数走进这些回调，以及 `chrdev_open` / `vfs_read` 怎么走进 `file_operations`。
3. [`03_driver_calls`](03_driver_calls)：反过来看驱动调用内核。`vfs_write` 进驱动之后，驱动用 `_copy_from_user` 把字节搬进内核；`vfs_read` 进驱动之后，驱动用 `_copy_to_user` 搬回去。
