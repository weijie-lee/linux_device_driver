# 内核主线

`ch*` 和 `learn/` 是一块块具体的驱动。`kernel/` 反过来读操作系统：驱动核心怎么表示总线、设备、驱动，以及绑定是在哪一层发生的。

这一层不插进章节编号。现在只有第一步。

1. [`01_driver_framework`](01_driver_framework)：拆开 5.15 `drivers/base` 的注册、匹配、probe 和移除。
