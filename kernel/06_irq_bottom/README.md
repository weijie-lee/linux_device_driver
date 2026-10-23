# 硬中断、软中断、中断线程、工作队列

客户机没有空闲的硬件中断线。模块建一个只有 1 个 hwirq 的 irqdomain，从 `IRQ_WORK_HARD` 里调用 `generic_handle_domain_irq()`。上半部计数后 `tasklet_schedule()`，返回 `IRQ_WAKE_THREAD`。线程里再 `schedule_work()`。

每一步另外打 `virt_bottom_step` 事件。事件打在函数末尾，所以 `step=hard` 出现在 `step=raise` 前面：raise 的事件要等 `generic_handle_domain_irq()` 返回才发出，硬中断处理函数在这次调用里面。

## 代码知识点

函数跟踪里的顺序：

```
virt_bottom_raise <-irq_work_single
handle_irq_event <-handle_level_irq
virt_bottom_hard <-__handle_irq_event_percpu
virt_bottom_tasklet_fn <-tasklet_action_common.constprop.0
virt_bottom_thread <-irq_thread_fn
process_one_work <-worker_thread
virt_bottom_work_fn <-process_one_work
```

`irq_work_single` 在硬中断里跑，跟踪标志是 `d.h.`。tasklet 的调用者是 `tasklet_action_common`，标志里有 `s`，`in_serving_softirq()` 为真。中断线程是普通任务，`in_task()` 为真，调用者是 `irq_thread_fn`。工作队列的调用者是 `process_one_work`。

这台内核没有 `CONFIG_PREEMPT_COUNT` 这个选项名，但硬中断入口仍会把 `HARDIRQ_OFFSET` 加进 `preempt_count`。`counts` 的后四项是 `hard_in_irq tasklet_in_softirq thread_in_task raise_errno`，成功时是 `1 1 1 0`。前四项是四个回调各自的次数。

`handle_irq_event` 也会被别的中断碰到。判定只要求本模块的 `raise` 之后还能按顺序看到 `handle_irq_event`、`hard`、`tasklet`、`thread`、`process_one_work`、`work`。我们的域用的是 `handle_level_irq`，别的中断多半是 `handle_edge_irq`。

## 代码架构

```
module_init
  platform_device "virt_bottom"
  irq_domain_add_linear + request_threaded_irq

echo 1 > kick
  irq_work_queue(IRQ_WORK_HARD)
    irq_work_single → virt_bottom_raise
      generic_handle_domain_irq
        handle_level_irq → handle_irq_event → virt_bottom_hard
          tasklet_schedule
          return IRQ_WAKE_THREAD
    软中断 tasklet_action_common → virt_bottom_tasklet_fn
    irq_thread_fn → virt_bottom_thread → schedule_work
    worker_thread → process_one_work → virt_bottom_work_fn
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter kernel06
```

下面这份回显来自镜像里的一次实际运行。中断号和进程号每次不同。

```
======== kernel06_irq_bottom ========
+ insmod /lib/modules/driver_test/kernel_06_irq_bottom_virt_bottom.ko
insmod ok
+ echo 1 > /sys/devices/platform/virt_bottom/kick
bottom halves finished
              sh-1       [000] d.h..     9.237929: virt_bottom_raise <-irq_work_single
              sh-1       [000] d.h..     9.238151: handle_irq_event <-handle_level_irq
              sh-1       [000] d.h..     9.238164: virt_bottom_hard <-__handle_irq_event_percpu
              sh-1       [000] d.h..     9.238435: virt_bottom_step: step=hard n=1
              sh-1       [000] dNh..     9.239263: virt_bottom_step: step=raise n=1
              sh-1       [000] .Ns..     9.239489: virt_bottom_tasklet_fn <-tasklet_action_common.constprop.0
              sh-1       [000] .Ns..     9.239549: virt_bottom_step: step=tasklet n=1
 irq/25-virt_bot-156     [000] .....     9.239819: virt_bottom_thread <-irq_thread_fn
 irq/25-virt_bot-156     [000] .....     9.239879: virt_bottom_step: step=thread n=1
     kworker/0:1-18      [000] d....     9.240271: process_one_work <-worker_thread
     kworker/0:1-18      [000] .....     9.240285: virt_bottom_work_fn <-process_one_work
     kworker/0:1-18      [000] .....     9.240329: virt_bottom_step: step=work n=1
irq order ok
+ cat /sys/devices/platform/virt_bottom/counts
1 1 1 1 1 1 1 0
+ rmmod virt_bottom
rmmod ok
[PASS] kernel06_irq_bottom
```

成功时客户机打印 `[PASS] kernel06_irq_bottom`。`counts` 是 `1 1 1 1 1 1 1 0`：四个回调各跑了一次，硬中断、软中断、进程上下文三个判断都为真，`generic_handle_domain_irq()` 返回 0。事件里要有 `step=hard n=1`、`step=tasklet n=1`、`step=thread n=1`、`step=work n=1`。
