# kernel/09 的客户机实验。不要和 virt_mmio 同时加载。

k09_irqs() {
    grep virt_msi /proc/interrupts | awk '{
        s = 0
        for (i = 2; i <= NF; i++)
            if ($i ~ /^[0-9]+$/)
                s += $i
        print s
    }'
}

lab_kernel09() {
    begin "kernel09_msi_irq"
    tr_ready || { finish; return; }
    load "$MODDIR/kernel_09_msi_irq_virt_msi.ko" || { finish; return; }

    dev=""
    for d in /sys/bus/pci/drivers/virt_msi/0000:*; do
        [ -e "$d" ] && dev=$d
    done
    if [ -z "$dev" ]; then
        echo "edu device not bound"
        ok=0
        unload virt_msi
        finish
        return
    fi
    echo "bound $dev"
    before=$(k09_irqs)
    echo "interrupts before: ${before:-missing}"
    run "grep virt_msi /proc/interrupts"
    want "virt_msi"

    ft_arm virt_msi_irq virt_msi_compute handle_irq_event \
        || { ft_off; unload virt_msi; finish; return; }
    echo "+ echo 5 > $dev/run"
    echo 5 > "$dev/run"
    echo "+ echo 0 > $TR/tracing_on"
    echo 0 > "$TR/tracing_on"
    ft_show 'virt_msi_|handle_irq_event'
    hits=$(ft_hits \
        ": virt_msi_compute <-=compute" \
        ": handle_irq_event <-=handle_irq_event" \
        ": virt_msi_irq <-=handler")
    ft_off
    expect_order msi "$hits" compute handle_irq_event handler
    run "cat $dev/result"
    want "fact=120"
    want "irqs=1"
    after=$(k09_irqs)
    echo "interrupts after: $after"
    if [ -n "$before" ] && [ -n "$after" ] && [ "$after" -gt "$before" ]; then
        echo "proc interrupts increased"
    else
        echo "proc interrupts did not increase"
        ok=0
    fi

    unload virt_msi
    finish
}
