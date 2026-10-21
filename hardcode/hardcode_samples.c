// SPDX-License-Identifier: GPL-2.0
/*
 * hardcode_samples.c — 5.15 上可加载的数据结构短例子
 *
 * 每种结构在 init 里走一遍创建、插入、取出、销毁。说明见同目录的
 * data_structures.md。这里不注册设备，也不碰客户机里已有的磁盘和网卡。
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/llist.h>
#include <linux/plist.h>
#include <linux/rbtree.h>
#include <linux/interval_tree.h>
#include <linux/idr.h>
#include <linux/radix-tree.h>
#include <linux/xarray.h>
#include <linux/hashtable.h>
#include <linux/rhashtable.h>
#include <linux/kfifo.h>
#include <linux/circ_buf.h>
#include <linux/skbuff.h>
#include <linux/scatterlist.h>
#include <linux/bio.h>
#include <linux/kref.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/hrtimer.h>
#include <linux/notifier.h>
#include <linux/bitmap.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/seqlock.h>
#include <linux/rcupdate.h>

struct hc_item {
	int key;
	struct list_head list;
	struct hlist_node hnode;
	struct llist_node lnode;
	struct plist_node pnode;
	struct rb_node rb;
	struct interval_tree_node it;
	struct rhash_head rhead;
};

static LIST_HEAD(hc_list);
static HLIST_HEAD(hc_hlist);
static LLIST_HEAD(hc_llist);
static DEFINE_HASHTABLE(hc_hash, 3);
static DEFINE_IDR(hc_idr);
static DEFINE_XARRAY(hc_xa);
static RADIX_TREE(hc_radix, GFP_KERNEL);
static DEFINE_KFIFO(hc_fifo, unsigned char, 16);
static DECLARE_COMPLETION(hc_done);
static DEFINE_MUTEX(hc_mutex);
static DEFINE_SPINLOCK(hc_lock);
static seqlock_t hc_seq = __SEQLOCK_UNLOCKED(hc_seq);
static int __rcu *hc_rcu_ptr;
static int hc_rcu_storage = 7;
static ATOMIC_NOTIFIER_HEAD(hc_notifier);
static struct rb_root hc_rb = RB_ROOT;
static struct rb_root_cached hc_itree = RB_ROOT_CACHED;
static struct rhashtable hc_rht;

static const struct rhashtable_params hc_rht_params = {
	.key_len = sizeof(int),
	.key_offset = offsetof(struct hc_item, key),
	.head_offset = offsetof(struct hc_item, rhead),
};

static void hc_work_fn(struct work_struct *work)
{
	complete(&hc_done);
}
static DECLARE_WORK(hc_work, hc_work_fn);

static void hc_timer_fn(struct timer_list *t)
{
}
static DEFINE_TIMER(hc_timer, hc_timer_fn);

static enum hrtimer_restart hc_hrtimer_fn(struct hrtimer *t)
{
	return HRTIMER_NORESTART;
}
static struct hrtimer hc_hrtimer;

static int hc_notify(struct notifier_block *nb, unsigned long val, void *data)
{
	return NOTIFY_OK;
}
static struct notifier_block hc_nb = {
	.notifier_call = hc_notify,
};

static void hc_release(struct kref *kref)
{
}

static int hc_insert_rb(struct hc_item *item)
{
	struct rb_node **link = &hc_rb.rb_node, *parent = NULL;

	while (*link) {
		struct hc_item *this = rb_entry(*link, struct hc_item, rb);

		parent = *link;
		if (item->key < this->key)
			link = &(*link)->rb_left;
		else if (item->key > this->key)
			link = &(*link)->rb_right;
		else
			return -EEXIST;
	}
	rb_link_node(&item->rb, parent, link);
	rb_insert_color(&item->rb, &hc_rb);
	return 0;
}

static int __init hc_init(void)
{
	struct hc_item *item, *tmp;
	struct llist_node *ln;
	unsigned char byte = 0x5a, out = 0;
	char circ_storage[8];
	struct circ_buf circ = { .buf = circ_storage, .head = 0, .tail = 0 };
	DECLARE_BITMAP(bits, 64);
	struct scatterlist sg;
	struct sk_buff *skb;
	struct bio *bio;
	struct kref ref;
	int id, *rcu_seen;
	unsigned int seq;
	int ret;

	item = kzalloc(sizeof(*item), GFP_KERNEL);
	if (!item)
		return -ENOMEM;
	item->key = 1;

	list_add_tail(&item->list, &hc_list);
	list_for_each_entry(tmp, &hc_list, list)
		pr_info("hardcode list key=%d\n", tmp->key);
	list_del(&item->list);

	hlist_add_head(&item->hnode, &hc_hlist);
	hlist_del(&item->hnode);

	llist_add(&item->lnode, &hc_llist);
	ln = llist_del_all(&hc_llist);
	if (ln)
		pr_info("hardcode llist popped\n");

	/*
	 * plist_add/plist_del 在 5.15 上没有导出给模块。节点初始化是内联的，
	 * 真正的插入发生在内核内建的 futex 等待队列里，见 data_structures.md。
	 */
	plist_node_init(&item->pnode, 10);
	{
		struct plist_head phead;

		plist_head_init(&phead);
		(void)phead;
	}

	ret = hc_insert_rb(item);
	if (ret)
		goto out_item;
	rb_erase(&item->rb, &hc_rb);

	item->it.start = 0x1000;
	item->it.last = 0x1fff;
	interval_tree_insert(&item->it, &hc_itree);
	if (!interval_tree_iter_first(&hc_itree, 0x1800, 0x1800)) {
		ret = -ENOENT;
		interval_tree_remove(&item->it, &hc_itree);
		goto out_item;
	}
	interval_tree_remove(&item->it, &hc_itree);

	id = idr_alloc(&hc_idr, item, 1, 0, GFP_KERNEL);
	if (id < 0) {
		ret = id;
		goto out_item;
	}
	if (idr_find(&hc_idr, id) != item) {
		ret = -ENOENT;
		idr_remove(&hc_idr, id);
		goto out_item;
	}
	idr_remove(&hc_idr, id);

	ret = radix_tree_insert(&hc_radix, 3, item);
	if (ret)
		goto out_item;
	radix_tree_delete(&hc_radix, 3);

	if (xa_is_err(xa_store(&hc_xa, 4, item, GFP_KERNEL))) {
		ret = -ENOMEM;
		goto out_item;
	}
	xa_erase(&hc_xa, 4);

	INIT_HLIST_NODE(&item->hnode);
	hash_add(hc_hash, &item->hnode, item->key);
	hash_del(&item->hnode);

	ret = rhashtable_init(&hc_rht, &hc_rht_params);
	if (ret)
		goto out_item;
	ret = rhashtable_insert_fast(&hc_rht, &item->rhead, hc_rht_params);
	if (!ret)
		rhashtable_remove_fast(&hc_rht, &item->rhead, hc_rht_params);
	rhashtable_destroy(&hc_rht);
	if (ret)
		goto out_item;

	INIT_KFIFO(hc_fifo);
	if (kfifo_in(&hc_fifo, &byte, 1) != 1 || kfifo_out(&hc_fifo, &out, 1) != 1) {
		ret = -EIO;
		goto out_item;
	}

	if (CIRC_SPACE(circ.head, circ.tail, 8) > 0)
		circ.buf[circ.head++ & 7] = 'A';

	skb = alloc_skb(64, GFP_KERNEL);
	if (!skb) {
		ret = -ENOMEM;
		goto out_item;
	}
	skb_put(skb, 4);
	kfree_skb(skb);

	sg_init_one(&sg, &byte, sizeof(byte));

	bio = bio_alloc(GFP_KERNEL, 1);
	if (!bio) {
		ret = -ENOMEM;
		goto out_item;
	}
	bio_put(bio);

	kref_init(&ref);
	kref_get(&ref);
	kref_put(&ref, hc_release);
	kref_put(&ref, hc_release);

	schedule_work(&hc_work);
	wait_for_completion(&hc_done);

	timer_setup(&hc_timer, hc_timer_fn, 0);
	mod_timer(&hc_timer, jiffies + HZ);
	del_timer_sync(&hc_timer);

	hrtimer_init(&hc_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hc_hrtimer.function = hc_hrtimer_fn;
	hrtimer_start(&hc_hrtimer, ms_to_ktime(50), HRTIMER_MODE_REL);
	hrtimer_cancel(&hc_hrtimer);

	ret = atomic_notifier_chain_register(&hc_notifier, &hc_nb);
	if (ret)
		goto out_item;
	atomic_notifier_call_chain(&hc_notifier, 0, NULL);
	atomic_notifier_chain_unregister(&hc_notifier, &hc_nb);

	bitmap_zero(bits, 64);
	set_bit(3, bits);
	if (!test_bit(3, bits)) {
		ret = -EINVAL;
		goto out_item;
	}

	mutex_lock(&hc_mutex);
	mutex_unlock(&hc_mutex);
	spin_lock(&hc_lock);
	spin_unlock(&hc_lock);

	write_seqlock(&hc_seq);
	write_sequnlock(&hc_seq);
	do {
		seq = read_seqbegin(&hc_seq);
	} while (read_seqretry(&hc_seq, seq));

	rcu_assign_pointer(hc_rcu_ptr, &hc_rcu_storage);
	rcu_read_lock();
	rcu_seen = rcu_dereference(hc_rcu_ptr);
	if (!rcu_seen || *rcu_seen != 7)
		ret = -EINVAL;
	rcu_read_unlock();
	rcu_assign_pointer(hc_rcu_ptr, NULL);
	synchronize_rcu();
	if (ret)
		goto out_item;

	kfree(item);
	pr_info("hardcode samples ok\n");
	return 0;

out_item:
	kfree(item);
	return ret;
}

static void __exit hc_exit(void)
{
	idr_destroy(&hc_idr);
}

module_init(hc_init);
module_exit(hc_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Short 5.15 samples for common kernel data structures");
