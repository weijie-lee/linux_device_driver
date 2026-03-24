# linked_lists - 内核链表使用示例

## 知识点
Linux内核提供了一套标准的双向循环链表实现：
- `struct list_head` 作为链表节点嵌入到自定义结构体
- 提供了完整的添加、删除、遍历接口
- 使用`container_of`从链表节点获取自定义结构体

## 编译运行
```bash
make linked_lists
cd linked_lists
make
sudo insmod linked_lists.ko
dmesg | tail
sudo rmmod linked_lists
```

## 常用操作
```c
// 定义链表头
LIST_HEAD(mylist);

// 添加节点
list_add(&new_node->list, &mylist);

// 遍历链表
list_for_each(pos, &mylist) {
    // 使用container_of获取结构体指针
}

// 删除节点
list_del(entry);
```

## 参考
- 《Linux设备驱动开发详解》相关章节
