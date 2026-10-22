# 中断和并发

## 代码知识点

客户机没有多余的硬件中断线。本模块注册一个只有一个 hwirq 的 `irq_domain`，写 `kick` 时用 `IRQ_WORK_HARD` 在硬中断上下文调用 `generic_handle_domain_irq()`。

三条路径：

1. **上半部** `virt_irq_hard`：只拿自旋锁，增加 `hard_count`，返回 `IRQ_WAKE_THREAD`。这里不能拿 mutex。
2. **threaded irq** `virt_irq_thread`：进程上下文，拿 mutex，增加 `thread_count`，然后 `schedule_work()`。
3. **工作队列** `virt_work_fn`：再拿同一把 mutex，增加 `work_count`。

`/sys/devices/platform/virt_irq/counts` 的六个数是：

```
hard thread work hard_in_interrupt thread_in_task raise_errno
```

`raise_errno` 为 0 表示 `generic_handle_domain_irq()` 成功。`hard_in_interrupt` 为 1 表示上半部确实在中断上下文，`thread_in_task` 为 1 表示线程在进程上下文。

这版 5.15 通用内核没有打开 `CONFIG_PROVE_LOCKING`。lockdep 不会在这里打印报告。若把 `mutex_lock` 放进上半部，调试内核会指出在原子上下文里睡眠。本模块把自旋锁和 mutex 拆开，就是为了避免这一点。

```bash
echo 1 > /sys/devices/platform/virt_irq/kick
cat /sys/devices/platform/virt_irq/counts
```

## 代码架构

```
probe
  INIT_WORK(virt_work_fn)
  irq_domain_add_linear(1 个 hwirq) → irq_create_mapping
  request_threaded_irq(virt_irq_hard, virt_irq_thread)
echo 1 > kick
  irq_work_queue                              IRQ_WORK_HARD，在硬中断上下文里
    generic_handle_domain_irq
      virt_irq_hard    spinlock，hard_count++，IRQ_WAKE_THREAD
      virt_irq_thread  mutex，thread_count++，schedule_work
        virt_work_fn   mutex，work_count++
remove
  irq_work_sync → free_irq → irq_dispose_mapping → irq_domain_remove
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter learn03
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/learn_03_irq_virt_irq.ko
insmod ok
+ echo 1 > /sys/devices/platform/virt_irq/kick
+ cat /sys/devices/platform/virt_irq/counts
1 1 1 1 1 0
+ rmmod virt_irq
rmmod ok
[PASS] learn03_irq
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] learn03_irq`
- `counts` 前五项都大于等于 1，第六项 `raise_errno` 是 0。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter learn03
```
