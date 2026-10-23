# kernel/04 的客户机实验。build_rootfs.sh 拷成 /lab.d/kernel04.sh，由 qemu_test/lab.sh 引入。

lab_kernel04() {
    begin "kernel04_user_paths"
    tr_ready || { finish; return; }
    load "$MODDIR/kernel_04_user_paths_virt_upath.ko" || { finish; return; }
    node /dev/virt_upath
    run "ls -l /dev/virt_upath"
    want "crw-------"

    # 64 位 ioctl。这台内核把 vfs_ioctl 内联进了 __x64_sys_ioctl，
    # 跟踪里能看见的调用者是系统调用入口，不是 vfs_ioctl。
    ft_arm virt_upath_ioctl capable || { ft_off; unload virt_upath; finish; return; }
    run "/bin/upath_ioctl"
    want "FAIL: 0"
    want "sizeof(long)=8"
    want "errno=Operation not permitted"
    echo "+ echo 0 > $TR/tracing_on"
    echo 0 > "$TR/tracing_on"
    ft_show 'upath_ioctl-.*(virt_upath_ioctl|capable <-virt_upath)'
    hits=$(ft_hits ": virt_upath_ioctl <-__x64_sys_ioctl=driver_ioctl" \
        ": capable <-virt_upath_ioctl=capable")
    ft_off
    expect_order ioctl "$hits" driver_ioctl capable driver_ioctl capable
    run "dmesg | grep virt_upath:"
    want "value set to 7 by uid 0"

    # 32 位程序：同一个 ioctl 号，入口换成 compat_ptr_ioctl
    ft_arm virt_upath_ioctl compat_ptr_ioctl || { ft_off; unload virt_upath; finish; return; }
    run "/bin/upath_ioctl32"
    want "FAIL: 0"
    want "sizeof(long)=4"
    echo "+ echo 0 > $TR/tracing_on"
    echo 0 > "$TR/tracing_on"
    ft_show 'upath_ioctl32-.*(virt_upath_ioctl|compat_ptr_ioctl)'
    hits=$(ft_hits ": compat_ptr_ioctl <-=compat_ptr_ioctl" \
        ": virt_upath_ioctl <-compat_ptr_ioctl=driver_ioctl")
    ft_off
    expect_order compat "$hits" compat_ptr_ioctl driver_ioctl

    # sysfs：读属性走 seq_file，再进 device_attribute.show
    echo "+ echo 11 > /sys/class/misc/virt_upath/value"
    echo 11 > /sys/class/misc/virt_upath/value
    ft_arm upath_value_show dev_attr_show sysfs_kf_seq_show || { ft_off; unload virt_upath; finish; return; }
    echo "+ cat /sys/class/misc/virt_upath/value > /tmp/upath_sysfs"
    cat /sys/class/misc/virt_upath/value > /tmp/upath_sysfs
    echo "+ echo 0 > $TR/tracing_on"
    echo 0 > "$TR/tracing_on"
    ft_show 'upath_value_show|dev_attr_show|sysfs_kf_seq_show'
    hits=$(ft_hits ": sysfs_kf_seq_show <-=sysfs_kf_seq_show" \
        ": dev_attr_show <-sysfs_kf_seq_show=dev_attr_show" \
        ": upath_value_show <-dev_attr_show=driver_show")
    ft_off
    expect_order sysfs "$hits" sysfs_kf_seq_show dev_attr_show driver_show
    run "cat /tmp/upath_sysfs"
    want "11"

    # procfs：proc_create_single 的 show 由 seq_read_iter 调用
    ft_arm virt_upath_proc_show seq_read_iter proc_reg_read_iter || { ft_off; unload virt_upath; finish; return; }
    echo "+ cat /proc/virt_upath > /tmp/upath_proc"
    cat /proc/virt_upath > /tmp/upath_proc
    echo "+ echo 0 > $TR/tracing_on"
    echo 0 > "$TR/tracing_on"
    ft_show 'cat-.*(virt_upath_proc_show|seq_read_iter|proc_reg_read_iter)'
    hits=$(ft_hits ": proc_reg_read_iter <-=proc_reg_read_iter" \
        ": seq_read_iter <-proc_reg_read_iter=seq_read_iter" \
        ": virt_upath_proc_show <-seq_read_iter=driver_show")
    ft_off
    expect_order proc "$hits" proc_reg_read_iter seq_read_iter driver_show
    run "cat /tmp/upath_proc"
    want "value=11"

    unload virt_upath
    finish
}
