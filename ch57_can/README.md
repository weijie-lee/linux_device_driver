# CAN

## 代码知识点

CAN 控制器在 `alloc_candev()` 的 `net_device` 上填 `can_priv`：位定时常数、时钟、`do_set_bittiming`。`register_candev()` 注册。接口要先设比特率再 `ip link set up`。

本控制器时钟 8 MHz。`start_xmit` 丢掉 skb 并计一次发送。没有收发器。

## 代码架构

```
alloc_candev
can_priv.clock.freq = 8000000
can_bittiming_const
register_candev
ip link 看到 can0
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch57
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch57_can_virt_can.ko
insmod ok
+ lsmod | grep virt_can
virt_can               16384  0 
can_dev                40960  1 virt_can
+ find /sys /dev /proc -name '*can*' 2>/dev/null | head -n 30
/sys/kernel/mm/transparent_hugepage/khugepaged/scan_sleep_millisecs
/sys/kernel/mm/transparent_hugepage/khugepaged/pages_to_scan
/sys/kernel/mm/transparent_hugepage/khugepaged/full_scans
/sys/kernel/mm/ksm/pages_to_scan
/sys/kernel/mm/ksm/full_scans
/sys/kernel/btf/can_dev
/sys/kernel/tracing/events/xhci-hcd/xhci_dbg_cancel_urb
/sys/kernel/tracing/events/jbd2/jbd2_shrink_scan_enter
/sys/kernel/tracing/events/jbd2/jbd2_shrink_scan_exit
/sys/kernel/tracing/events/ext4/ext4_es_shrink_scan_enter
/sys/kernel/tracing/events/ext4/ext4_es_shrink_scan_exit
/sys/kernel/tracing/events/ext4/ext4_fc_replay_scan
/sys/kernel/tracing/events/huge_memory/mm_khugepaged_scan_pmd
/sys/kernel/tracing/events/vmscan
/sys/kernel/tracing/events/vmscan/mm_vmscan_kswapd_sleep
/sys/kernel/tracing/events/vmscan/mm_vmscan_kswapd_wake
/sys/kernel/tracing/events/vmscan/mm_vmscan_wakeup_kswapd
/sys/kernel/tracing/events/vmscan/mm_vmscan_direct_reclaim_begin
/sys/kernel/tracing/events/vmscan/mm_vmscan_memcg_reclaim_begin
/sys/kernel/tracing/events/vmscan/mm_vmscan_memcg_softlimit_reclaim_begin
/sys/kernel/tracing/events/vmscan/mm_vmscan_direct_reclaim_end
/sys/kernel/tracing/events/vmscan/mm_vmscan_memcg_reclaim_end
/sys/kernel/tracing/events/vmscan/mm_vmscan_memcg_softlimit_reclaim_end
/sys/kernel/tracing/events/vmscan/mm_vmscan_lru_isolate
/sys/kernel/tracing/events/vmscan/mm_vmscan_writepage
/sys/kernel/tracing/events/vmscan/mm_vmscan_lru_shrink_inactive
/sys/kernel/tracing/events/vmscan/mm_vmscan_lru_shrink_active
/sys/kernel/tracing/events/vmscan/mm_vmscan_node_reclaim_begin
/sys/kernel/tracing/events/vmscan/mm_vmscan_node_reclaim_end
/sys/kernel/tracing/events/alarmtimer/alarmtimer_cancel
+ ip link
1: lo: <LOOPBACK> mtu 65536 qdisc noop qlen 1000
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
5: can0: <NOARP> mtu 16 qdisc noop qlen 10
    link/[280] 
+ rmmod virt_can
rmmod ok
[PASS] ch57_can
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch57_can`
- `ip link` 里有 `can`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch57
```
