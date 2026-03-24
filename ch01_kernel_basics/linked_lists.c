/*
 * linked_lists.c — Linux 内核侵入式双向链表使用演示
 *
 * 【知识点】
 * Linux 内核使用"侵入式"链表设计：不是让业务结构体继承链表节点，
 * 而是将 struct list_head 作为成员嵌入到业务结构体中。
 *
 * 这种设计的优势：
 *   1. 通用性：同一套链表操作函数可用于任意结构体
 *   2. 零开销：无需动态分配链表节点，节省内存
 *   3. 多链表：同一结构体可同时挂在多条链表上（嵌入多个 list_head）
 *
 * 【核心 API 速查】
 *   LIST_HEAD(name)              — 静态定义并初始化链表头
 *   INIT_LIST_HEAD(ptr)          — 动态初始化链表头（自环）
 *   LIST_HEAD_INIT(name)         — 初始化器，用于结构体字面量初始化
 *   list_add(new, head)          — 在 head 之后插入（头插法，LIFO）
 *   list_add_tail(new, head)     — 在 head 之前插入（尾插法，FIFO）
 *   list_del(entry)              — 从链表中删除节点
 *   list_for_each(pos, head)     — 遍历链表（pos 是 list_head*）
 *   list_entry(ptr, type, member)— 等价于 container_of，获取业务结构体指针
 *   list_for_each_entry(pos, head, member) — 直接遍历业务结构体（推荐）
 *
 * 【内存布局示意】
 *   mylinkedlist (head)
 *       ↓ next
 *   node_3.mylist ⇔ node_2.mylist ⇔ node_1.mylist ⇔ (head)
 *   （list_add 是头插法，插入顺序 1→2→3，遍历顺序 3→2→1）
 *
 * (C) 2020.03.26 liweijie<ee.liweijie@gmail.com>
 * GPL v2
 */

#include <linux/module.h>
#include <linux/list.h>

static int __init mod_init(void)
{
	/*
	 * struct mystruct — 演示用的业务结构体
	 *
	 * 关键设计：struct list_head mylist 是嵌入的链表节点。
	 * 遍历链表时通过 list_entry()/container_of() 从 mylist 地址
	 * 反推出整个 mystruct 的地址，从而访问 data 字段。
	 */
	struct mystruct {
		int data;
		struct list_head mylist;	/* 嵌入的链表节点，不存储业务数据 */
	};

	struct mystruct node_1;
	struct mystruct node_2;
	/*
	 * node_3 使用结构体字面量初始化：
	 *   LIST_HEAD_INIT(node_3.mylist) 展开为
	 *   { .prev = &node_3.mylist, .next = &node_3.mylist }
	 * 即初始化为自环（空链表节点的标准状态）。
	 */
	struct mystruct node_3 = {
		.data = 97,
		.mylist = LIST_HEAD_INIT(node_3.mylist),
	};

	struct list_head *position    = NULL;	/* 遍历游标（list_head*） */
	struct mystruct  *datastructptr = NULL;	/* 业务结构体指针 */

	/*
	 * LIST_HEAD(mylinkedlist) 在栈上定义并初始化一个链表头。
	 * 展开为：
	 *   struct list_head mylinkedlist = { &mylinkedlist, &mylinkedlist }
	 * 即 next 和 prev 都指向自身，表示空链表。
	 */
	LIST_HEAD(mylinkedlist);
	
	node_1.data = 99;
	INIT_LIST_HEAD(&(node_1.mylist));	/* 动态初始化：node_1.mylist 自环 */

	node_2.data = 98;
	INIT_LIST_HEAD(&(node_2.mylist));	/* 动态初始化：node_2.mylist 自环 */

	/*
	 * list_add() 是头插法（在 head 的 next 位置插入）。
	 * 插入顺序：node_1 → node_2 → node_3
	 * 链表结构：head ⇔ node_3 ⇔ node_2 ⇔ node_1 ⇔ head
	 * 因此遍历顺序为：node_3(97) → node_2(98) → node_1(99)
	 *
	 * 若需要 FIFO 顺序，应使用 list_add_tail()。
	 */
	list_add(&node_1.mylist, &mylinkedlist);
	list_add(&node_2.mylist, &mylinkedlist);
	list_add(&node_3.mylist, &mylinkedlist);

	/*
	 * list_for_each(position, head) 遍历链表。
	 * position 是 struct list_head* 类型的游标，指向当前节点的 mylist 字段。
	 * 需要用 list_entry() 从 position 还原出业务结构体指针。
	 *
	 * list_entry(ptr, type, member) 等价于 container_of(ptr, type, member)。
	 */
	list_for_each(position, &mylinkedlist) {
		datastructptr = list_entry(position, struct mystruct, mylist);
		printk(KERN_INFO "[linked_list] data: %d\n", datastructptr->data);
	}
	/* 预期输出：97 → 98 → 99（头插法，逆序） */

	/*
	 * list_del() 从链表中摘除 node_1，但不释放内存。
	 * 摘除后 node_1.mylist 的 prev/next 被设为 LIST_POISON1/LIST_POISON2，
	 * 防止悬空指针被误用（访问会触发 oops，便于调试）。
	 */
	list_del(&node_1.mylist);
	printk(KERN_INFO "[linked_list] after deleting node_1 (data=99):\n");

	/*
	 * list_for_each_entry() 是更简洁的遍历宏，直接将游标设为业务结构体指针，
	 * 内部自动调用 list_entry()，无需手动转换。
	 * 这是内核代码中最常见的链表遍历写法。
	 */
	list_for_each_entry(datastructptr, &mylinkedlist, mylist) {
		printk(KERN_INFO "[linked_list] data: %d\n", datastructptr->data);
	}
	/* 预期输出：97 → 98（node_1 已删除） */

	return 0;
}
static void __exit mod_exit(void)
{
	return;
}

module_init(mod_init);
module_exit(mod_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie<ee.liweijie@gmail.com>");
