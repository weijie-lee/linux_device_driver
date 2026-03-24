# container_of 宏使用示例

`container_of`是**Linux内核最重要的宏之一**，是理解内核数据结构的关键。

## 什么问题需要 container_of？

在Linux内核中，**链表节点`struct list_head`通常作为成员嵌入到你自定义的结构体中**：

```c
// 内核定义的链表节点
struct list_head {
    struct list_head *next, *prev;
};

// 你自定义的结构体
struct person {
    int age;
    struct list_head node;  // 链表节点嵌入这里
    char *name;
};
```

当你在遍历时，只有`node`成员的指针，**怎么得到整个`struct person`的指针？**

这就是 `container_of` 解决的问题！

## 原理

**公式：`结构体指针 = 成员指针 - 成员在结构体中的偏移量`**

内核中实际定义：
```c
#define container_of(ptr, type, member) ({              \
    const typeof(((type *)0)->member) *__mptr = (ptr);    \
    (type *)((char *)__mptr - offsetof(type,member));   \
})
```

**分步理解：**

| 步骤 | 作用 |
|------|------|
| `typeof(((type *)0)->member)` | 获取成员变量的类型 |
| `const typeof(...) *__mptr = (ptr)` | 保存成员指针 |
| `offsetof(type, member)` | 计算成员相对于结构体起始地址的偏移量（字节） |
| `(char *)__mptr - offset` | 成员指针减去偏移 = 结构体起始地址 |
| `(type *)` | 转换回正确的指针类型 |

## 本示例代码分析

```c
struct person {
    int age;        // 偏移 0
    int salary;     // 偏移 4（int 占4字节）
    char *name;     // 偏移 8（64位指针占8字节）
};

struct person leo;
int *age_ptr = &(leo.age);
leo_ptr = container_of(age_ptr, struct person, age);
// = age_ptr (0) - 0 = &leo ✅

int *salary_ptr = &(leo.salary);
leo_ptr = container_of(salary_ptr, struct person, salary);
// = (&leo+4) - 4 = &leo ✅

char **name_ptr = &(leo.name);
leo_ptr = container_of(name_ptr, struct person, name);
// = (&leo+8) - 8 = &leo ✅
```

**不管你拿哪个成员的指针，都能正确算出整个结构体指针！**

## 编译运行
```bash
# 在顶级目录
make container_of

# 或者进入当前目录编译
cd container_of
make

# 加载模块
sudo insmod container_of.ko

# 查看输出
dmesg | grep "addr of person"

# 你会看到：三次输出的结构体地址都相同！证明计算正确
# addr of person: 0xffffc9000057c000
# addr of person: 0xffffc9000057c000
# addr of person: 0xffffc9000057c000

# 卸载模块
sudo rmmod container_of

# 清理编译产物
make clean
```

## 内核中实际使用场景

### 1. 链表遍历
```c
list_for_each(pos, &my_list) {
    struct my_struct *entry = container_of(pos, struct my_struct, node);
    // entry 就是你要的自定义结构体指针
}
```

### 2. 字符设备驱动
```c
struct my_dev *dev = container_of(inode->i_cdev, struct my_dev, cdev);
```

**只要看到"结构体成员嵌入"，就需要 container_of！**

## 参考
- 《Linux设备驱动开发详解》第6章
