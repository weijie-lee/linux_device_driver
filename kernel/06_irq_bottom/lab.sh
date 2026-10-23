# kernel/06 的客户机实验。

lab_kernel06() {
    begin "kernel06_irq_bottom"
    tr_ready || { finish; return; }
    load "$MODDIR/kernel_06_irq_bottom_virt_bottom.ko" || { finish; return; }

    echo "+ echo 1 > $TR/events/virt_bottom/virt_bottom_step/enable"
    if ! echo 1 > "$TR/events/virt_bottom/virt_bottom_step/enable"; then
        echo "virt_bottom_step event missing"
        ok=0
    fi
    ft_arm virt_bottom_raise virt_bottom_hard handle_irq_event \
        virt_bottom_tasklet_fn virt_bottom_thread process_one_work \
        virt_bottom_work_fn || { ft_off; unload virt_bottom; finish; return; }
    echo "+ echo 1 > /sys/devices/platform/virt_bottom/kick"
    echo 1 > /sys/devices/platform/virt_bottom/kick
    if wait_for 3 'grep -q "^1 1 1 1 " /sys/devices/platform/virt_bottom/counts'; then
        echo "bottom halves finished"
    else
        echo "bottom halves did not finish"
        ok=0
    fi
    echo "+ echo 0 > $TR/tracing_on"
    echo 0 > "$TR/tracing_on"
    ft_show 'virt_bottom_|handle_irq_event|process_one_work'
    hits=$(ft_hits \
        ": virt_bottom_raise <-=raise" \
        ": handle_irq_event <-=handle_irq_event" \
        ": virt_bottom_hard <-=hard" \
        ": virt_bottom_tasklet_fn <-=tasklet" \
        ": virt_bottom_thread <-=thread" \
        ": process_one_work <-=process_one_work" \
        ": virt_bottom_work_fn <-process_one_work=work")
    run "grep virt_bottom_step $TR/trace"
    want "step=hard n=1"
    want "step=tasklet n=1"
    want "step=thread n=1"
    want "step=work n=1"
    ft_off
    expect_order irq "$hits" raise handle_irq_event hard tasklet thread process_one_work work
    echo 0 > "$TR/events/virt_bottom/virt_bottom_step/enable"
    run "cat /sys/devices/platform/virt_bottom/counts"
    want "1 1 1 1 1 1 1 0"

    unload virt_bottom
    finish
}
