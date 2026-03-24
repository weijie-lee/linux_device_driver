/*
 * container_of.c — container_of 宏原理演示
 *
 * 【知识点】
 * container_of(ptr, type, member) 是 Linux 内核中最常用的宏之一。
 * 它的作用是：已知结构体某个成员的指针，反推出整个结构体的首地址。
 *
 * 【实现原理】
 * container_of(ptr, type, member) 在内核中展开为：
 *   (type *)( (char *)(ptr) - offsetof(type, member) )
 *
 * 即：成员地址 - 成员在结构体中的偏移量 = 结构体首地址
 *
 * 【典型使用场景】
 * Linux 内核的链表、等待队列、工作队列等通用数据结构，都将"链接节点"
 * 嵌入到业务结构体中（侵入式设计）。遍历时拿到的是节点指针，
 * 通过 container_of 可以还原出业务结构体指针。
 *
 * 示例：
 *   struct my_device {
 *       int id;
 *       struct list_head list;  // 嵌入的链表节点
 *   };
 *   struct list_head *pos;
 *   struct my_device *dev = container_of(pos, struct my_device, list);
 *
 * (C) 2020.03.26 liweijie<ee.liweijie@gmail.com>
 * GPL v2
 */

#include <linux/module.h>
#include <linux/kernel.h>

/*
 * struct person — 演示用的业务结构体
 *
 * 内存布局（64位系统，无 packed 属性）：
 *   偏移 0: age    (4字节, int)
 *   偏移 4: salary (4字节, int)
 *   偏移 8: name   (8字节, char* 指针)
 * 总大小: 16字节
 *
 * 我们将从三个不同成员的地址分别反推结构体首地址，
 * 三次结果应完全相同，均等于 &leo。
 */
struct person {
    int age;
    int salary;
    char *name;
};

static int __init container_of_init(void)
{
    struct person leo;    /* 栈上分配的 person 实例，地址即为演示基准 */
    struct person *leo_ptr;

    /*
     * 分别取三个成员的地址，用于后续 container_of 演示。
     * 注意：age_ptr 是 int*，name_ptr 是 char**（指向指针的指针）。
     */
    int *age_ptr    = &(leo.age);
    int *salary_ptr = &(leo.salary);
    char **name_ptr = &(leo.name);

    /*
     * 演示1：从 age 成员地址反推结构体首地址
     * 计算过程：leo_ptr = age_ptr - offsetof(person, age)
     *          = age_ptr - 0  （age 在偏移 0 处，所以地址不变）
     */
    leo_ptr = container_of(age_ptr, struct person, age);
    printk(KERN_INFO "[container_of] from age:    struct person @ 0x%p\n", leo_ptr);

    /*
     * 演示2：从 salary 成员地址反推结构体首地址
     * 计算过程：leo_ptr = salary_ptr - offsetof(person, salary)
     *          = salary_ptr - 4  （salary 在偏移 4 处）
     */
    leo_ptr = container_of(salary_ptr, struct person, salary);
    printk(KERN_INFO "[container_of] from salary: struct person @ 0x%p\n", leo_ptr);

    /*
     * 演示3：从 name 成员地址反推结构体首地址
     * 计算过程：leo_ptr = name_ptr - offsetof(person, name)
     *          = name_ptr - 8  （name 指针在偏移 8 处）
     * 注意：name_ptr 是 char**，即 &leo.name，不是 leo.name 本身。
     */
    leo_ptr = container_of(name_ptr, struct person, name);
    printk(KERN_INFO "[container_of] from name:   struct person @ 0x%p\n", leo_ptr);

    /*
     * 验证：打印各成员的直接地址与通过指针访问的地址，两者应完全相同。
     * 这证明 container_of 计算正确，leo_ptr 确实指向 leo 结构体。
     */
    printk(KERN_INFO "[container_of] &leo.age:    direct=0x%p, via ptr=0x%p\n",
           &(leo.age),    &(leo_ptr->age));
    printk(KERN_INFO "[container_of] &leo.salary: direct=0x%p, via ptr=0x%p\n",
           &(leo.salary), &(leo_ptr->salary));
    printk(KERN_INFO "[container_of] &leo.name:   direct=0x%p, via ptr=0x%p\n",
           leo.name,      leo_ptr->name);

    return 0;
}

static void __exit container_of_exit(void)
{
    return;
}
module_init(container_of_init);
module_exit(container_of_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie<ee.liweijie@gmail.com>");
