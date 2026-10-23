# kernel/07 的客户机实验。

lab_kernel07() {
    begin "kernel07_lock_context"
    tr_ready || { finish; return; }
    load "$MODDIR/kernel_07_lock_context_virt_lock.ko" || { finish; return; }
    node /dev/virt_lock

    run "cat /sys/class/misc/virt_lock/context"
    want "task=0 spin=0 mutex=0"
    want "hard_in_irq=1"

    run "cat /sys/class/misc/virt_lock/spread"
    want "cpu0=1 cpu1=1 sum=2 atomic=2"

    # mutex 里 msleep 会进 schedule_timeout。function_graph 只展开这一个函数。
    fg_arm virt_lock_mutex_sleep 5 || { fg_off; unload virt_lock; finish; return; }
    echo "+ echo 1 > /sys/class/misc/virt_lock/mutex_sleep"
    echo 1 > /sys/class/misc/virt_lock/mutex_sleep
    echo "+ echo 0 > $TR/tracing_on"
    echo 0 > "$TR/tracing_on"
    run "grep -E 'virt_lock_mutex_sleep|msleep|schedule_timeout' $TR/trace"
    want "virt_lock_mutex_sleep"
    want "schedule_timeout"
    fg_off

    # 先装好 function_graph，再把软锁阈值降到 1 秒（实际判定是 2 秒）。
    # 阈值放在装跟踪之前的话，切换 tracer 本身就可能被判成 soft lockup。
    fg_arm virt_lock_spin 3 || { fg_off; unload virt_lock; finish; return; }
    echo "+ echo 1 > /proc/sys/kernel/watchdog_thresh"
    echo 1 > /proc/sys/kernel/watchdog_thresh
    echo "+ echo 1 > /sys/class/misc/virt_lock/lockup"
    echo 1 > /sys/class/misc/virt_lock/lockup
    echo "+ echo 0 > $TR/tracing_on"
    echo 0 > "$TR/tracing_on"
    run "grep virt_lock_spin $TR/trace"
    want "virt_lock_spin"
    if printf '%s\n' "$LAST" | grep -q 'schedule'; then
        echo "spin path called schedule"
        ok=0
    fi
    fg_off
    echo "+ echo 10 > /proc/sys/kernel/watchdog_thresh"
    echo 10 > /proc/sys/kernel/watchdog_thresh
    run "dmesg | grep -A 25 'BUG: soft lockup'"
    want "BUG: soft lockup"
    want "virt_lock_spin"

    unload virt_lock
    finish
}
