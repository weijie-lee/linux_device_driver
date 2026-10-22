# Ch03: 字符设备高阶

## 章节概述

`ch02` 的 `globalmem` 是一块可以随时读写的内存。本章把它改成 FIFO：缓冲区空时读者睡眠，满时写者睡眠，同时支持非阻塞、`poll`/`select`/`epoll` 和 `SIGIO` 异步通知。设备节点是 `/dev/global_mem_0` 到 `/dev/global_mem_3`，每个节点各有一个 4096 字节的缓冲区。

## 代码知识点

### 1. 阻塞读写与等待队列

`struct global_mem_dev` 在 `ch02` 的基础上多了三项：`current_len`（缓冲区里的字节数）、`r_wait`（读等待队列）、`w_wait`（写等待队列）。

- 读：`current_len == 0` 时，非阻塞（`O_NONBLOCK`）返回 `-EAGAIN`，阻塞则把自己挂到 `r_wait`，`schedule()` 睡眠。
- 写：`current_len == 4096` 时同理，挂在 `w_wait`。
- 读到数据后 `wake_up_interruptible(&w_wait)`，写入数据后 `wake_up_interruptible(&r_wait)`。

睡眠前的顺序是关键：先 `__set_current_state(TASK_INTERRUPTIBLE)`，再 `mutex_unlock()`，最后 `schedule()`。如果先解锁，写者可能在解锁之后、`schedule()` 之前调用 `wake_up()`，读者那时还是 `TASK_RUNNING`，唤醒丢失，读者永远睡下去。状态先设好，`wake_up()` 会把它改回 `TASK_RUNNING`，`schedule()` 立刻返回。条件检查放在 `while` 里，被唤醒后重新判断。收到信号时返回 `-ERESTARTSYS`。

### 2. poll / select / epoll

`.poll` 里对 `r_wait` 和 `w_wait` 各调一次 `poll_wait()`，它只登记等待队列，不睡眠。随后根据 `current_len` 返回掩码：非空是 `POLLIN | POLLRDNORM`，未满是 `POLLOUT | POLLWRNORM`。真正的睡眠发生在内核的 `select`/`poll`/`epoll` 实现里。

### 3. fasync 与 SIGIO

用户程序对文件描述符设置 `O_ASYNC`，内核调用 `.fasync`，驱动用 `fasync_helper()` 维护 `async_queue`。写入成功后 `kill_fasync(&async_queue, SIGIO, POLL_IN)` 通知所有登记的进程。`release` 里必须 `fasync_helper(-1, filp, 0)` 摘除，否则发信号时会访问已关闭的 `file`。

### 4. ioctl

`MEM_CLEAR` 是 `_IO('g', 0)`：加锁后把缓冲区清零，`current_len` 置 0。未知命令返回 `-EINVAL`。

### 5. 为什么要在 init 里注册 platform_device

驱动是 `platform_driver`，设备节点在 `probe` 里创建。`probe` 只有在总线上出现名字匹配的 `platform_device` 时才会被调用。没有设备树和 ACPI 时没有人创建它，所以 `module_init` 先 `platform_device_register_simple("globalfifo")`，再用 `platform_driver_probe()` 同步地跑一次 `probe`。少了这一步，`insmod` 成功，`lsmod` 能看到模块，但 `/dev/global_mem_*` 不会出现。

## 代码架构

```
应用程序   read / write / poll / ioctl / fcntl(O_ASYNC)
              │
VFS        file_operations
              │
globalfifo struct global_mem_dev × 4
             ├─ cdev、mem[4096]、current_len、mutex
             ├─ r_wait：读者在这里睡
             ├─ w_wait：写者在这里睡
             └─ async_queue：登记了 O_ASYNC 的进程
              │
内核       等待队列、进程状态、信号
```

```
insmod globalfifo.ko
  platform_device_register_simple("globalfifo")
  platform_driver_probe → probe
    alloc_chrdev_region → kzalloc → 4 × (mutex_init、init_waitqueue_head×2、cdev_init、cdev_add)
    class_create → 4 × device_create        /dev/global_mem_0 .. 3
read
  空：O_NONBLOCK 返回 -EAGAIN，否则睡在 r_wait
  有数据：copy_to_user → 剩余数据前移 → wake_up(w_wait)
write
  满：O_NONBLOCK 返回 -EAGAIN，否则睡在 w_wait
  有空间：copy_from_user → wake_up(r_wait) → kill_fasync(SIGIO)
rmmod globalfifo
  platform_driver_unregister → remove：device_destroy → class_destroy → cdev_del → kfree → unregister_chrdev_region
  platform_device_unregister
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch03
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch03_char_advanced_globalfifo.ko
insmod ok
+ ls /dev/global_mem_0
/dev/global_mem_0
+ test -c /dev/global_mem_0 -o -b /dev/global_mem_0
node ok
+ echo fifo_test > /dev/global_mem_0
+ head -c 9 /dev/global_mem_0
fifo_test
+ /bin/test_globalfifo
=== Ch03 Globalfifo Character Device Test ===

  [PASS] open /dev/global_mem_0 (blocking + nonblocking)
[TC01] Basic read/write
  [PASS] write returns correct byte count
  [PASS] read returns correct byte count
  [PASS] read data matches written data
[TC02] Non-blocking read on empty FIFO
  [PASS] non-blocking read on empty FIFO returns EAGAIN
[TC03] poll writable
  [PASS] poll reports writable when FIFO has space
[TC04] poll readable after write
  [PASS] poll reports readable after write
[TC05] ioctl FIFO_CLEAR
  [PASS] ioctl FIFO_CLEAR returns 0
  [PASS] after clear, non-blocking read returns EAGAIN

=== Test Summary ===
PASS: 9  FAIL: 0
+ rmmod globalfifo
rmmod ok
[PASS] ch03_globalfifo
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch03_globalfifo`
- `ls` 能列出 `/dev/global_mem_0`，并且 `node ok`。这个节点由 probe 创建，probe 要等到 `module_init` 注册了同名 `platform_device` 才会运行
- `head -c 9 /dev/global_mem_0` 是 `fifo_test`。读完就停，不要用 `cat`，空 FIFO 上的 `read` 会睡着
- 用户态程序 `test_globalfifo` 的结尾是 `PASS: 9  FAIL: 0`，其中包含非阻塞读空 FIFO 得到 `EAGAIN`、`poll` 可读可写、`MEM_CLEAR`

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch03
```

## 常见问题

### Q: 如何调试驱动？

A: 可以使用以下方法：
- 查看内核日志：`dmesg | tail -f`
- 使用 printk 添加调试信息
- 使用 kgdb 进行内核调试
- 使用 ftrace 进行性能分析

### Q: 如何处理驱动中的错误？

A: Linux 驱动中的错误处理通常使用：
- 返回错误码（-ERRNO）
- 使用 goto 进行错误清理
- 使用 devm_* 函数自动管理资源

### Q: 如何支持多个设备实例？

A: 通过以下方式：
- 使用设备列表管理多个实例
- 为每个实例分配独立的资源
- 使用次设备号区分不同实例

## 参考资源

- **Linux Kernel Documentation**：https://www.kernel.org/doc/html/latest/
- **Linux Device Drivers, 3rd Edition**：https://lwn.net/Kernel/LDD3/
- **Kernel Source Code**：https://elixir.bootlin.com/linux/v5.15/

---

**最后更新**：2026 年 3 月 26 日  
**章节版本**：1.0.0  
**内核支持**：5.15.0+  
**测试状态**：✅ PASS (QEMU)
