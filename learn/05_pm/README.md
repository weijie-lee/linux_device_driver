# runtime PM 和系统睡眠

`dev_pm_ops` 上有两对回调。

**系统睡眠** `suspend` / `resume` 把 `ctrl` 清成 0，唤醒时写回。生产环境里它们由 `pm_suspend()` 调用。本实验不让客户机进入 mem 睡眠，`sleep` 属性调用的是驱动结构体里的同一对函数指针。

**runtime PM** 在 probe 里 `pm_runtime_set_active()` 然后 `pm_runtime_enable()`。写 `idle` 调用 `pm_runtime_idle()`。usage 为 0 时内核进入 `runtime_suspend`。`/sys/devices/platform/virt_pm/power/control` 写成 `on` 会 `pm_runtime_forbid()` 并 resume。

```bash
cat /sys/devices/platform/virt_pm/ctrl                 # 3
echo suspend > /sys/devices/platform/virt_pm/sleep
cat /sys/devices/platform/virt_pm/ctrl                 # 0
echo resume > /sys/devices/platform/virt_pm/sleep
echo 1 > /sys/devices/platform/virt_pm/idle
cat /sys/devices/platform/virt_pm/power/runtime_status # suspended
echo on > /sys/devices/platform/virt_pm/power/control
cat /sys/devices/platform/virt_pm/power/runtime_status # active
```

`runtime_marker` 由 runtime 回调更新，用来确认走的是驱动自己的函数，而不只是 PM 核心改了状态字。
