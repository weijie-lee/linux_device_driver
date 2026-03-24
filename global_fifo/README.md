# global_fifo - 支持poll/epoll/fasync的FIFO驱动

## 知识点
在基础globalmem之上，增加了异步通知相关功能：
- `globalfifo.c` - 基础FIFO
- `globalfifo_poll.c` - 支持poll系统调用
- `globalfifo_epoll.c` - 支持epoll
- `globalfifo_fasync.c` - 支持异步通知（fasync）

这些机制用于实现**用户空间和内核空间的事件通知**，让应用程序不用轮询就能知道设备是否可读写。

## 编译运行
```bash
make global_fifo
cd global_fifo
make
```

## 知识点对比
| 机制 | 用途 |
|------|------|
| poll/select | 同时监听多个文件描述符，传统方法 |
| epoll | 更高效的多路复用，适合大量连接 |
| fasync | 异步通知，设备就绪时发送信号给应用 |

## 参考
- 《Linux设备驱动开发详解》相关章节
