# 设计模式

下面几种写法在驱动里反复出现。每一节都指到本仓库里已经能编译、并且在 5.15 客户机里加载过的代码。

## 嵌入节点和 container_of

通用结构嵌进业务对象，回调里再倒推回来。链表、`cdev`、`work_struct`、`timer_list` 都是这个形状。

`ch01_kernel_basics/container_of.c` 用一个 `person` 演示偏移计算：从 `age`、`salary`、`name` 三个成员的地址都能回到 `&leo`。

真正的驱动用法在 `ch02_char_basic/globalmem.c` 的 `global_mem_open()`：

```c
struct global_mem_dev {
        struct cdev cdev;
        unsigned char mem[GLOBALMEM_SIZE];
        struct mutex mutex;
};

dev = container_of(inode->i_cdev, struct global_mem_dev, cdev);
filp->private_data = dev;
```

`inode->i_cdev` 只知道 `cdev`。`cdev` 是 `global_mem_dev` 的第一个成员时地址碰巧相同，成员不在开头时就靠偏移。之后的 read/write 只用 `filp->private_data`，不再查找。

`ch14_net_mac_phy/eth_mac.c` 的 `virt_mac_poll()` 对 NAPI 做同样的事：`container_of(napi, struct virt_mac_priv, napi)`。

## ops 表

子系统核心拿着一张函数表，在固定的时机调用。驱动不继承一个类，只填这张表。

字符设备是 `file_operations`。`globalmem.c` 的 `global_mem_fops` 填了 `open`、`read`、`write`、`release`，`cdev_init()` 把它绑到 `cdev` 上。用户 `read()` 最后进的是 `global_mem_read()`。

网卡是 `net_device_ops`。`eth_mac.c` 的 `virt_mac_ops` 填了 `ndo_open`、`ndo_start_xmit`、`ndo_get_stats64`。协议栈发一个包时调用 `ndo_start_xmit`，驱动自己不暴露这些符号给用户。

总线驱动也是同一张形状。`ch61_i3c/virt_i3c.c` 的 `i3c_master_controller_ops` 必须提供 `bus_init`、`priv_xfers`、`send_ccc_cmd`、`do_daa`、`i2c_xfers`，否则 `i3c_master_register()` 直接返回 `-EINVAL`。看门狗是 `watchdog_ops`（`ch09`），块设备是 `blk_mq_ops.queue_rq`（`ch17`）。

填表时按 5.15 的结构体来，不要照着更新的内核补参数。`ch11` 的 `pwm_ops.get_state` 在 5.15 返回 `void`，写成 `int` 会在编译期对不上。

## probe 和 remove

总线匹配到 `id_table` 或名字之后才调用 `probe`。资源在 `probe` 里申请，失败就按申请的反序释放。`remove` 做和成功的 `probe` 相反的事。

`ch21_gpio/virt_gpio.c` 到 `ch68` 的多数模块用同一个骨架，因为 QEMU 客户机没有对应的设备树节点：

1. `platform_device_alloc()` / `platform_device_add()` 造一个同名设备。
2. `platform_driver_register()`。
3. 看 `g_pdev->dev.driver` 是否非空。`platform_driver_register()` 在 `probe` 失败时仍然返回 0，设备上没有驱动就说明 `probe` 没绑上，模块 init 改返回 `-ENODEV`。

`ch61` 的 `probe` 调用 `i3c_master_register()`。5.15 要求 `bus_init()` 里调用 `i3c_master_set_info()`，否则核心打印 `master_set_info() was not called` 并返回 `-EINVAL`，上面的检查就会让 `insmod` 失败。

PCI 和 USB 不自己造设备。`ch19_pci_bus` 的 `pci_device_id`、`ch20_usb_bus` 的 `USB_DEVICE()` 使用客户机里不存在的 vendor/product，所以 `probe` 不会绑到 QEMU 的真实磁盘或控制器上。

## devm

把资源和 `device` 绑在一起。`probe` 失败或驱动解绑时，核心按注册的反序释放。

`ch54_mtd/virt_mtd.c` 用 `devm_kzalloc()` 分配控制器和后备缓冲区，`remove` 不必再 `kfree`。

释放顺序有意义时用 `devm_add_action_or_reset()`。`ch43_devfreq/virt_devfreq.c` 先 `dev_pm_opp_add()`，再登记 `virt_drop_opp`，最后 `devm_devfreq_add_device()`。devm 按后进先出释放，所以先拆掉 devfreq，再删 OPP。若在 `remove` 里先删 OPP，devfreq 还拿着这张表。

`devm_*` 不能用在没有 `device` 的地方。纯软件的 `ch40` crypto 算法就没有平台设备，内存用普通的 `kmalloc` / `kfree`。

## 上半部和下半部

硬中断或发送函数里只做不能推迟的事：记录、入队、通知。会睡眠或会跑很久的工作放到下半部。

`ch14_net_mac_phy/eth_mac.c` 是这个划分的具体样子。

上半部是 `virt_mac_start_xmit()`。它在持有 `rx_lock` 时 `skb_queue_tail()`，然后 `napi_schedule()`，随即返回 `NETDEV_TX_OK`。它不在发送路径里遍历协议栈。

下半部是 `virt_mac_poll()`。NAPI 在软中断里调用它，按 `budget` 从队列里 `skb_dequeue()`，交给 `netif_receive_skb()`。队列空了就 `napi_complete_done()`。

同一份代码里的 PHY 定时器是另一种下半部：`timer_list` 到期后在软中断里看链路，不在打开设备的函数里空转等待。

中断里如果必须睡眠（读一条慢的 I2C，或者分配大量内存），下半部用 `work_struct`，见 `data_structures.md` 里 devfreq 的 `delayed_work`。

## 通知链

一个事件，多个互不引用的订阅者。发布者只调用 `atomic_notifier_call_chain()` 或 `blocking_notifier_call_chain()`，不知道对面是谁。

和 ops 表的差别：ops 表是「这个设备的那一个驱动」。通知链是「这件事发生了，谁关心谁注册」。

内核里的两个实例：

- `register_reboot_notifier()`：重启前各子系统收到回调，用来停硬件。
- `register_netdevice_notifier()`：网卡 up/down 时，路由、防火墙等各自的 `notifier_block` 被调用。

`hardcode_samples.c` 建了一条只在模块内部使用的 `ATOMIC_NOTIFIER_HEAD`，回调 `hc_notify()` 返回 `NOTIFY_OK`。这条链上的函数不能睡眠。

## 状态机

用一个明确的状态记录设备走到了哪一步，不要堆一串互相矛盾的布尔值。

`ch14` 的虚拟 PHY 用定时器在链路 up 和 down 之间切换，`ethtool` 读到的是这一步的结果，而不是发送函数里临时猜的。

`ch09_watchdog/watchdog_demo.c` 的状态就是看门狗框架规定的那几步：`start` 启动 `hrtimer`，写入数据是 `ping`（重新计时），`stop` 取消定时器。超时回调只在「已经 start、并且一个周期内没有 ping」时发生。用户空间看到的 `/dev/watchdog` 语义和内核里的 `watchdog_ops` 是同一台状态机。

`ch61` 的 I3C 主控制器把状态交给核心：`bus_init` 成功之后核心才会发 RSTDAA、关事件、做 DAA。驱动若跳过 `i3c_master_set_info()`，核心拒绝进入下一步，而不是带着空的 `master->this` 继续发 CCC。
