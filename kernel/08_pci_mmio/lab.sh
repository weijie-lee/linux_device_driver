# kernel/08 的客户机实验。edu 在启动参数里，模块加载时就会 probe。

lab_kernel08() {
    begin "kernel08_pci_mmio"
    tr_ready || { finish; return; }
    load "$MODDIR/kernel_08_pci_mmio_virt_mmio.ko" || { finish; return; }

    dev=""
    for d in /sys/bus/pci/drivers/virt_mmio/0000:*; do
        [ -e "$d" ] && dev=$d
    done
    if [ -z "$dev" ]; then
        echo "edu device not bound"
        ok=0
        unload virt_mmio
        finish
        return
    fi
    echo "bound $dev"
    run "cat $dev/ident"
    want "010000ed"

    # 设备还在，驱动函数已经在内存里。摘掉再 rescan，probe 才会进这次跟踪。
    ft_arm virt_mmio_probe local_pci_probe virt_mmio_read_id \
        || { ft_off; unload virt_mmio; finish; return; }
    echo "+ echo 1 > $dev/remove"
    echo 1 > "$dev/remove"
    echo "+ echo 1 > /sys/bus/pci/rescan"
    echo 1 > /sys/bus/pci/rescan
    echo "+ echo 0 > $TR/tracing_on"
    echo 0 > "$TR/tracing_on"
    ft_show 'virt_mmio_probe|local_pci_probe|virt_mmio_read_id'
    hits=$(ft_hits \
        ": local_pci_probe <-=local_pci_probe" \
        ": virt_mmio_probe <-local_pci_probe=driver_probe" \
        ": virt_mmio_read_id <-virt_mmio_probe=read_id")
    ft_off
    expect_order probe "$hits" local_pci_probe driver_probe read_id

    dev=""
    for d in /sys/bus/pci/drivers/virt_mmio/0000:*; do
        [ -e "$d" ] && dev=$d
    done
    if [ -z "$dev" ]; then
        echo "edu device not rebound"
        ok=0
        unload virt_mmio
        finish
        return
    fi
    ft_arm liveness_show liveness_store dev_attr_show dev_attr_store \
        || { ft_off; unload virt_mmio; finish; return; }
    echo "+ echo 0x12345678 > $dev/liveness"
    echo 0x12345678 > "$dev/liveness"
    echo "+ cat $dev/liveness > /tmp/edu_live"
    cat "$dev/liveness" > /tmp/edu_live
    echo "+ echo 0 > $TR/tracing_on"
    echo 0 > "$TR/tracing_on"
    ft_show 'liveness_|dev_attr_'
    hits=$(ft_hits \
        ": dev_attr_store <-=dev_attr_store" \
        ": liveness_store <-dev_attr_store=store" \
        ": dev_attr_show <-=dev_attr_show" \
        ": liveness_show <-dev_attr_show=show")
    ft_off
    expect_order mmio "$hits" dev_attr_store store dev_attr_show show
    run "cat /tmp/edu_live"
    want "edcba987"

    unload virt_mmio
    finish
}
