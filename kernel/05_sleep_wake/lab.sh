# kernel/05 的客户机实验。build_rootfs.sh 拷成 /lab.d/kernel05.sh，由 qemu_test/lab.sh 引入。

k05_sleepers() {
    cat /sys/class/misc/virt_sleep/sleepers
}

# 失败路径上也要把 D 状态的读者叫醒，否则 rmmod 会因为设备还开着而失败。
k05_drain() {
    n=0
    while [ "$(k05_sleepers 2>/dev/null)" != 0 ] && [ "$n" -lt 8 ]; do
        printf wake > /dev/virt_sleep
        n=$((n + 1))
    done
    wait 2>/dev/null || true
}

# 后台起一个读者，等它真的睡进 wait_event。读者的 pid 留在 rpid。
k05_reader() {
    echo "+ head -c 4 /dev/virt_sleep > $1 &"
    head -c 4 /dev/virt_sleep > "$1" &
    rpid=$!
    if wait_for 5 '[ "$(k05_sleepers)" = 1 ]'; then
        echo "reader $rpid is sleeping"
        return 0
    fi
    echo "reader never slept"
    ok=0
    return 1
}

lab_kernel05() {
    begin "kernel05_sleep_wake"
    tr_ready || { finish; return; }
    load "$MODDIR/kernel_05_sleep_wake_virt_sleep.ko" || { finish; return; }
    node /dev/virt_sleep
    run "cat /sys/class/misc/virt_sleep/sleepers"
    want "0"

    # 1. 函数跟踪：读者进 schedule，写者进 __wake_up
    ft_arm virt_sleep_read virt_sleep_write schedule __wake_up || { ft_off; k05_drain; unload virt_sleep; finish; return; }
    k05_reader /tmp/sleep_out
    run "cat /sys/class/misc/virt_sleep/sleepers"
    want "1"
    run "grep State /proc/$rpid/status"
    want "D (disk sleep)"
    run "cat /proc/$rpid/stack"
    want "virt_sleep_read"
    echo "+ printf ping > /dev/virt_sleep"
    printf ping > /dev/virt_sleep
    wait "$rpid"
    echo "+ echo 0 > $TR/tracing_on"
    echo 0 > "$TR/tracing_on"
    ft_show 'virt_sleep_(read|write) <-|schedule <-virt_sleep_read|__wake_up <-virt_sleep_write'
    hits=$(ft_hits ": virt_sleep_read <-vfs_read=read" \
        ": schedule <-virt_sleep_read=schedule" \
        ": virt_sleep_write <-vfs_write=write" \
        ": __wake_up <-virt_sleep_write=wake_up")
    ft_off
    expect_order sleep "$hits" read schedule write wake_up
    run "cat /tmp/sleep_out"
    want "ping"
    run "cat /sys/class/misc/virt_sleep/sleepers"
    want "0"

    # 2. function_graph：wake_up 往下走到调度器的 try_to_wake_up
    k05_reader /tmp/sleep_out2
    fg_arm virt_sleep_write 8 || { fg_off; k05_drain; unload virt_sleep; finish; return; }
    echo "+ printf pong > /dev/virt_sleep"
    printf pong > /dev/virt_sleep
    wait "$rpid"
    echo "+ echo 0 > $TR/tracing_on"
    echo 0 > "$TR/tracing_on"
    run "grep -E 'virt_sleep_write|__wake_up|autoremove_wake_function|default_wake_function|try_to_wake_up' $TR/trace"
    want "virt_sleep_write"
    want "__wake_up_common"
    want "autoremove_wake_function"
    want "default_wake_function"
    want "try_to_wake_up"
    fg_off
    run "cat /tmp/sleep_out2"
    want "pong"

    # 3. 调度事件：被叫醒的是哪个任务，它什么时候真正上 CPU
    k05_reader /tmp/sleep_out3
    ev_arm sched:sched_waking sched:sched_switch || { ev_off; k05_drain; unload virt_sleep; finish; return; }
    echo "+ echo 'pid == $rpid' > $TR/events/sched/sched_waking/filter"
    echo "pid == $rpid" > "$TR/events/sched/sched_waking/filter"
    echo "+ echo 'next_pid == $rpid' > $TR/events/sched/sched_switch/filter"
    echo "next_pid == $rpid" > "$TR/events/sched/sched_switch/filter"
    echo "+ printf abcd > /dev/virt_sleep"
    printf abcd > /dev/virt_sleep
    wait "$rpid"
    echo "+ echo 0 > $TR/tracing_on"
    echo 0 > "$TR/tracing_on"
    ft_show 'sched_waking|sched_switch'
    hits=$(ft_hits "sched_waking: comm=head pid=$rpid =waking" \
        "next_comm=head next_pid=$rpid =switch_in")
    echo 0 > "$TR/events/sched/sched_waking/filter"
    echo 0 > "$TR/events/sched/sched_switch/filter"
    ev_off
    expect_order sched "$hits" waking switch_in
    run "cat /tmp/sleep_out3"
    want "abcd"

    # 4. hung task：读者一直睡在 D 状态，检测器把它的内核栈打出来
    k05_reader /tmp/sleep_out4
    echo "+ echo 2 > /proc/sys/kernel/hung_task_timeout_secs"
    echo 2 > /proc/sys/kernel/hung_task_timeout_secs
    if wait_for 20 'dmesg | grep -q "task head:$rpid blocked for more than"'; then
        echo "hung task reported"
    else
        echo "hung task not reported"
        ok=0
    fi
    echo "+ echo 120 > /proc/sys/kernel/hung_task_timeout_secs"
    echo 120 > /proc/sys/kernel/hung_task_timeout_secs
    run "dmesg | grep -A 14 'task head:$rpid blocked'"
    want "blocked for more than"
    want "state:D"
    want "virt_sleep_read"
    echo "+ printf done > /dev/virt_sleep"
    printf done > /dev/virt_sleep
    wait "$rpid"
    run "cat /tmp/sleep_out4"
    want "done"
    run "cat /sys/class/misc/virt_sleep/wakeups"
    want "4"

    k05_drain
    unload virt_sleep
    finish
}
