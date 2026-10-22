# 内核常用数据结构与设计模式

## 代码知识点

这里是一份对照手册，不是新的驱动子系统。读完 `ch01`–`ch06` 之后、进入 `learn/` 之前看它。目标内核是 **5.15**。每一节都做两件事：指出本仓库或内核里正在使用它的代码，并给出能在 5.15 头文件下编译的用法。

可加载的短例子在 `hardcode_samples.c`。`make KERNEL_DIR=/lib/modules/5.15.0-173-generic/build hardcode` 会把它编成 `hardcode_samples.ko`。宿主机内核不是 5.15 时不要 `insmod`。

| 文档 | 内容 |
| --- | --- |
| [data_structures.md](data_structures.md) | 链表、树、查找表、缓冲区、引用计数、等待与推迟执行、锁 |
| [design_patterns.md](design_patterns.md) | `container_of`、ops 表、probe/remove、`devm`、上下半部、通知链、状态机 |

和现有章节的关系：`ch01` 已经演示了 `list_head`、`container_of` 和 `kfifo`。手册不把那几章再写一遍，只指出该去看哪一个函数。

## 代码架构

```
insmod hardcode_samples.ko                    不注册设备，只在 init 里各走一遍
  list_head / hlist / llist / plist           增、遍历、删
  rbtree / idr / radix_tree / xarray          插入、查找、删除
  kfifo / bitmap                              入队出队、位操作
  kref                                        get、put，最后一次 put 调 release
  workqueue + completion                      schedule_work 之后 wait_for_completion
  timer_list / hrtimer                        回调里只做短操作
  atomic_notifier_chain                       register → call_chain → unregister
  mutex / spinlock / seqlock / RCU            各自的临界区写法
  → dmesg: "hardcode list key=…"、"hardcode llist popped"、"hardcode samples ok"
rmmod hardcode_samples
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter hardcode
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/hardcode_hardcode_samples.ko
insmod ok
+ dmesg | grep hardcode
[   96.226595] hardcode list key=1
[   96.226709] hardcode llist popped
[   96.265169] hardcode samples ok
+ rmmod hardcode_samples
rmmod ok
[PASS] hardcode
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] hardcode`
- dmesg 里有 `hardcode samples ok` 和 `hardcode list key=`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter hardcode
```
