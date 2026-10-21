# 数据结构

内核容器是侵入式的：节点嵌在业务结构体里，遍历时用 `container_of` 找回去。`hardcode_samples.c` 的 `struct hc_item` 同时嵌了下面的多种节点，`hc_init()` 对每一种做一次插入和删除。

## list_head

双向循环链表。空表的 `next` 和 `prev` 都指向自己。

本仓库的例子是 `ch01_kernel_basics/linked_lists.c`。它在栈上放三个 `mystruct`，用 `list_add` 头插，再用 `list_for_each_entry` 读出 `data`。内核里每个进程挂在 `task_struct.tasks` 上，`for_each_process()` 就是在走这条 `list_head`。

```c
LIST_HEAD(head);
list_add_tail(&item->list, &head);
list_for_each_entry(tmp, &head, list)
        use(tmp->key);
list_del(&item->list);
```

## hlist_head

哈希桶用的单指针头。头上只有 `first`，桶很多时比 `list_head` 省一半指针。

5.15 的 `struct pid` 用 `hlist_head tasks[]` 把同一类 pid（线程、进程组、会话）上的任务串起来，`inodes` 也是一条 hlist。`hardcode_samples.c` 用 `hlist_add_head()` / `hlist_del()` 演示同一个节点。

## llist_head

无锁单链表。硬中断里可以 `llist_add()`，进程上下文再用 `llist_del_all()` 一次摘下整条链。

`kretprobe_instance.llist` 用它把返回探针的实例串起来，块层请求结构里的 `ipi_list` 也是 `llist_node`。样本里对应的调用是 `llist_add()` 和 `llist_del_all()`。

## plist_head

按优先级排序的链表。优先级数字小的排在前面。

`swap_info_struct.list` 按交换区优先级挂进 `swap_active_head`。SMP 上 `task_struct.pushable_tasks` 也是 `plist_node`，用来按优先级把可推送的实时任务串起来。5.15 的 `plist_add()` / `plist_del()` 没有导出给模块，所以 `hardcode_samples.c` 只做内联的 `plist_node_init()` 和 `plist_head_init()`。插入留在内核里的交换区和调度器路径上。

## rb_root

红黑树，按键有序，查找、插入、删除都是对数时间。

5.15 的进程地址空间 `mm_struct.mm_rb` 把每个 `vm_area_struct` 按起始地址放进红黑树（6.1 起这段改成了 maple tree，本仓库不跟）。样本函数 `hc_insert_rb()` 是标准写法：先沿树找到叶子，`rb_link_node()` 接上，再 `rb_insert_color()` 平衡。删除用 `rb_erase()`。

## interval_tree

建在红黑树上的区间树，用来回答「哪个区间盖住了这个地址」。

5.15 的 `address_space.i_mmap` 是 `rb_root_cached`。映射同一个文件的 VMA 用 `interval_tree_insert()` 放进去，缺页时用 `interval_tree_iter_first()` 找重叠区间。样本把 `[0x1000, 0x1fff]` 插进去，再查询 `0x1800`，然后 `interval_tree_remove()`。

## idr 和 ida

`idr` 把一个小整数映射到指针。System V IPC 的 `ipc_ids.ipcs_idr` 用它给 shm、sem、msg 分配 id。样本调用是 `idr_alloc()`、`idr_find()`、`idr_remove()`。

`ida` 只分配整数，不存指针。`struct dma_device` 的 `chan_ida` 给 DMA 通道编号，温区的冷却设备 id 也来自一张 `ida`。需要「编号到对象」时用 `idr`，只需要一个不重复的号时用 `ida`。

## radix_tree

整数键到指针的稀疏树。`idr` 的底层就是它。页缓存在 5.15 已经改用 `xarray`，不要再为页缓存写新的 radix tree。

样本用 `RADIX_TREE()`、`radix_tree_insert()`、`radix_tree_delete()` 存一个指针。

## xarray

`radix_tree` 的后继，带标记和更清楚的错误返回。5.15 的页缓存就是 `address_space.i_pages` 这棵 `xarray`：页偏移是键，`struct page` 是值。`find_get_page()` 走的是这棵树。

样本用 `xa_store()` 和 `xa_erase()`。`xa_store()` 失败时返回值要用 `xa_is_err()` 判断，不要把它当成普通指针。

## hashtable

`DEFINE_HASHTABLE(name, bits)` 展开成 `1 << bits` 个 `hlist_head`。键是 `unsigned long`，用 `hash_add()` / `hash_for_each_possible()` / `hash_del()`。

pid 哈希是手写的 `hlist` 数组，宏只是把同一件事收成三行。样本在 `hc_init()` 里 `hash_add()` 然后 `hash_del()`。`hlist_del()` 会把节点涂成毒值，再次入桶前要 `INIT_HLIST_NODE()`。

## rhashtable

可扩容的哈希表，桶满了会自己长大。`ipc_ids.key_ht` 用它按键查找 System V IPC 对象，对象里嵌 `rhash_head`。

样本的 `hc_rht_params` 给出键的长度和两个偏移，然后 `rhashtable_init()`、`rhashtable_insert_fast()`、`rhashtable_remove_fast()`、`rhashtable_destroy()`。插入失败也要 `destroy`，否则会留下内部的桶数组。

## kfifo

记录或字节的环形缓冲，容量必须是 2 的幂。读写下标用原子操作维护。

本仓库的完整例子是 `ch01_kernel_basics/kfifo_demo_static.c`：`kfifo_in()` 写入 `"hello"`，`kfifo_out()` 再读出来，并接到 `/proc/bytestream-fifo`。样本里只用 16 字节的 `DEFINE_KFIFO` 走一次入队和出队，并且检查返回的字节数。

## circ_buf

更裸的环形缓冲。`head` 是写位置，`tail` 是读位置，容量也必须是 2 的幂，并且始终留一个空位区分满和空。

5.15 串口核心把每个端口的发送缓冲放在 `uart_state.xmit`（`struct circ_buf`）里。样本用 `CIRC_SPACE()` 判断还有没有空位，再写入一个字节。它不管并发。两个上下文同时改 `head` 和 `tail` 时要自己加锁，或者改用 `kfifo`。

## sk_buff

网络包。数据放在 `skb` 指向的缓冲区里，`skb_put()` 从尾部划出一段给驱动填。

`ch14_net_mac_phy/eth_mac.c` 的 `virt_mac_start_xmit()` 是完整路径：`skb_copy()` 复制待发的包，`skb_queue_tail()` 放进 `priv->rx_queue`，再 `napi_schedule()`。样本只做 `alloc_skb()`、`skb_put()`、`kfree_skb()`，不注册网卡。

## scatterlist

描述一段或若干段不连续的 DMA 缓冲区。`sg_dma_address()` 和 `sg_dma_len()` 要等 `dma_map_sg()` 成功之后才有意义。

`ch12_dma/DMA-API.txt` 写了 `dma_map_sg()` 的用法。样本只调用 `sg_init_one()` 把一个字节描述成一张表，不调用 `dma_map_sg()`，因为没有真实的 `device`。

## bio

块层的一次 I/O。一个 `bio` 含有若干 `bio_vec`，每个 vec 是一页里的一段。

`ch17_block/vmem_disk.c` 的 `vmem_disk_queue_rq()` 用 `__rq_for_each_bio()` 取出请求里的每个 `bio`，再 `bio_for_each_segment()` 拷贝数据。样本只 `bio_alloc()` 再 `bio_put()`，不提交给任何磁盘。

## kref 和 refcount_t

对象还有人用就不能释放。`kref_put()` 减到 0 时调用你提供的 `release`。`struct device` 自己就嵌着 `struct kref kref`，`put_device()` 走到 0 才真正释放。

样本对一个栈上的 `kref` 做 `kref_init()`、`kref_get()`、两次 `kref_put()`。第二次才进入 `hc_release()`。不要把 `kfree` 直接当成 `release`：签名是 `void (*)(struct kref *)`，要从 `kref` 成员反推出外层对象。

## wait_queue_head_t

条件不成立时让当前进程睡下，另一个上下文改完条件再唤醒。

`ch03_char_advanced/globalfifo.c` 有两条队列：`r_wait` 在 FIFO 空时挡住读，`w_wait` 在 FIFO 满时挡住写。读路径是 `add_wait_queue()`、检查条件、`schedule()`，写路径在有空间后 `wake_up()`。

## completion

一次性的完成事件。适合「提交一个工作，等它做完」。

样本里 `hc_work_fn()` 调用 `complete()`，`hc_init()` 在 `schedule_work()` 之后 `wait_for_completion()`。内核里 `kthread_stop()` 也是等线程自己 `complete()` 它的退出完成量。

## timer_list

以 jiffies 为单位的定时器。回调在软中断里运行，不能睡眠。

`ch04_timer/second.c` 用 `timer_setup()` 绑定 `second_timer_handler`，到期后再 `mod_timer()` 把自己重新挂上，形成周期。`ch14` 的 PHY 轮询也是一个 `timer_list`。样本把定时器挂到 1 秒后，马上 `del_timer_sync()`，所以回调不会跑。卸载前必须 `del_timer_sync()`，否则回调可能打进已经卸载的代码。

## hrtimer

纳秒级定时器。回调返回 `HRTIMER_NORESTART` 或 `HRTIMER_RESTART`。

`ch09_watchdog/watchdog_demo.c` 用 `hrtimer` 模拟看门狗：打开设备时 `hrtimer_start()`，用户写入 `/dev/watchdog` 时重新计时，到期回调里打印超时。样本同样 `hrtimer_init()`、`hrtimer_start()`、`hrtimer_cancel()`。

## work_struct

把工作留到进程上下文。中断处理函数不能睡眠，需要睡眠时就 `schedule_work()`。

5.15 的 devfreq 监视就是这样：`devfreq_monitor()` 是 `delayed_work` 的函数，由 `devfreq_monitor_start()` 排进 `devfreq_wq`。样本用普通 `work_struct`，函数里只 `complete()`，init 里等待它结束，避免模块卸载时工作还在队列上。

## notifier_block

一条回调链。注册者提供 `notifier_call`，事件发生时按优先级依次调用。

内核重启走 `register_reboot_notifier()`。网络设备状态变化走 `register_netdevice_notifier()`，回调里看到的值是 `NETDEV_UP`、`NETDEV_DOWN` 这一类。样本自己建了一条 `ATOMIC_NOTIFIER_HEAD`，注册、`atomic_notifier_call_chain()`、再注销。原子链的回调不能睡眠。需要睡眠就用 `BLOCKING_NOTIFIER_HEAD`。

## 位图和 atomic_t

`cpumask` 就是一张位图，每一位是一个 CPU。`set_bit()` / `test_bit()` 对单个位是原子的。`atomic_t` 用来做计数和标志，不要用普通 `int` 加加减减再指望别的 CPU 能看见。

样本 `DECLARE_BITMAP(bits, 64)`，`set_bit(3)` 再 `test_bit(3)`。

## 锁、seqlock、RCU

这三种保护的是不同的读写真相。

`mutex` 可以睡眠。`ch02_char_basic/globalmem.c` 用 `mutex` 保护 `mem[]`，读和写都在 `mutex_lock()` 里 `copy_to_user` / `copy_from_user`。

`spinlock` 不能睡眠。`ch14` 的 `virt_mac_start_xmit()` 用 `spin_lock_irqsave()` 保护 `rx_queue`，因为 NAPI 和发送路径都会碰它。

`seqlock` 适合读多写少、读者可以重试的一小段数据。写者 `write_seqlock()`，读者 `read_seqbegin()` / `read_seqretry()`。`ch14` 的计数用的是同一思想的专用版 `u64_stats_sync`：`u64_stats_update_begin()` 写，`u64_stats_fetch_begin()` / `u64_stats_fetch_retry()` 读。

RCU 保护的是指针的发布。写者 `rcu_assign_pointer()` 换上新指针，等 `synchronize_rcu()` 之后才能释放旧对象。读者 `rcu_read_lock()` 里 `rcu_dereference()`。样本把 `hc_rcu_ptr` 指到 `hc_rcu_storage`，读出 7，再换成 `NULL` 并 `synchronize_rcu()`。文件表、路由缓存这一类「读远多于写」的表用的就是这个模式，不是又一种链表。
