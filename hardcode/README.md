# 内核常用数据结构与设计模式

这里是一份对照手册，不是新的驱动子系统。读完 `ch01`–`ch06` 之后、进入 `learn/` 之前看它。目标内核是 **5.15**。每一节都做两件事：指出本仓库或内核里正在使用它的代码，并给出能在 5.15 头文件下编译的用法。

可加载的短例子在 `hardcode_samples.c`。`make KERNEL_DIR=/lib/modules/5.15.0-173-generic/build hardcode` 会把它编成 `hardcode_samples.ko`。宿主机内核不是 5.15 时不要 `insmod`。

| 文档 | 内容 |
| --- | --- |
| [data_structures.md](data_structures.md) | 链表、树、查找表、缓冲区、引用计数、等待与推迟执行、锁 |
| [design_patterns.md](design_patterns.md) | `container_of`、ops 表、probe/remove、`devm`、上下半部、通知链、状态机 |

和现有章节的关系：`ch01` 已经演示了 `list_head`、`container_of` 和 `kfifo`。手册不把那几章再写一遍，只指出该去看哪一个函数。
