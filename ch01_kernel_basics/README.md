# kfifo - 内核 kfifo 使用示例

## 知识点
Linux内核提供了kfifo数据结构，用于实现环形缓冲区：
- 支持静态创建和动态分配
- 并发安全，适合生产者-消费者模型
- 提供了方便的入队出队API

## 示例内容
- `kfifo_demo_static.c`: 静态创建kfifo演示

## 编译运行
```bash
make kfifo
cd kfifo
make
sudo insmod kfifo.ko
dmesg | tail
sudo rmmod kfifo
```

## 参考
- 《Linux设备驱动开发详解》相关章节
