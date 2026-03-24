# container_of 宏使用示例

## 知识点
`container_of`是Linux内核中非常重要的一个宏，用于**从结构体成员指针获取整个结构体指针**。

在Linux内核开发中，经常会遇到：
- 链表节点嵌入在自定义结构体中
- 只有链表节点指针，需要找到所在的自定义结构体
- `container_of`帮你计算出整个结构体的起始地址

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
dmesg | tail

# 卸载模块
sudo rmmod container_of

# 清理编译产物
make clean
```

## 代码说明
- 定义`struct person`，包含三个成员
- 分别获取每个成员的指针
- 使用`container_of`从成员指针反推结构体指针
- 打印地址验证结果正确

## 公式理解
```c
container_of(ptr, type, member)
```
- `ptr`: 成员指针
- `type`: 整个结构体类型
- `member`: 成员在结构体中的名称
- 返回值: 整个结构体的指针

## 参考
- 《Linux设备驱动开发详解》第6章
