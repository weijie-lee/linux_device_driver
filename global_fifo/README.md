# global_fifo - 支持poll/epoll/fasync异步通知的FIFO驱动

这是一个**进阶字符设备驱动示例**，展示了Linux内核中**阻塞/非阻塞IO**和**异步事件通知**的完整实现。

## 知识点

| 机制 | 用途 | 特点 |
|------|------|------|
| **阻塞读写** | 资源不可用时进程进入睡眠等待 | 简单，适合单一等待 |
| **非阻塞读写** | 资源不可用时立即返回-EAGAIN | 用户空间可以自己处理等待 |
| **poll/select** | 同时监听多个文件描述符是否就绪 | 传统方法，数量有限 |
| **epoll** | 更高效的多路复用，适合大量连接 | 事件驱动，性能更高 |
| **fasync** | 设备就绪时内核发信号通知应用 | 真正异步，应用不用轮询 |

## 数据结构

```c
struct global_mem_dev {
	struct cdev cdev;
	unsigned char mem[GLOBALMEM_SIZE];
	int current_len;           // FIFO中当前数据长度
	struct mutex mutex;
	wait_queue_head_t r_wait;  // 读等待队列：FIFO空时读进程睡这里
	wait_queue_head_t w_wait;  // 写等待队列：FIFO满时写进程睡这里
	struct fasync_struct *async_queue;  // 异步通知链表
};
```

## 核心代码讲解

### 阻塞读
```c
while (dev->current_len == 0) {  // FIFO空，没数据可读
	if (filp->f_flags & O_NONBLOCK) {
		// 非阻塞：直接返回-EAGAIN
		ret = -EAGAIN;
		goto out;
	}
	// 阻塞：释放锁，进程进入睡眠，等待唤醒
	mutex_unlock(&dev->mutex);
	schedule();  // 让出CPU
	mutex_lock(&dev->mutex);
}
// 被唤醒后，有数据了，继续读...
```

### poll 方法
```c
static unsigned int globalfifo_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	poll_wait(filp, &dev->r_wait, wait);  // 将进程添加到poll等待表
	poll_wait(filp, &dev->w_wait, wait);

	if (dev->current_len != 0)
		mask |= POLLIN | POLLRDNORM;  // 现在可读

	if (dev->current_len != GLOBALMEM_SIZE)
		mask |= POLLOUT | POLLWRNORM; // 现在可写

	return mask;
}
```

### 异步通知（fasync）
```c
// 写入完成后，如果有数据可读，通知应用
if (dev->async_queue)
	kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
```
- 应用注册`SIGIO`信号处理函数
- 设备就绪时，内核主动发信号给应用
- 应用不用轮询，有数据来了自然会被通知

## 本目录文件说明

| 文件 | 功能 |
|------|------|
| `globalfifo.c` | 基础FIFO，完整支持阻塞/非阻塞 + poll |
| `globalfifo_poll.c` | poll接口完整实现 |
| `globalfifo_epoll.c` | epoll支持（驱动层面接口和poll一致） |
| `globalfifo_fasync.c` | 添加fasync异步通知支持 |

## 编译运行
```bash
make global_fifo
cd global_fifo
make
sudo insmod globalfifo.ko
```

## 测试阻塞读

测试程序 `reader.c`:
```c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd = open("/dev/global_mem_0", O_RDWR);
    char buf[100];
    printf("waiting for data...\n");
    int n = read(fd, buf, sizeof(buf));  // 阻塞在这里等数据
    printf("got %d bytes: %.*s\n", n, n, buf);
    close(fd);
    return 0;
}
```

```bash
# 终端1
./reader
waiting for data...   # 这里阻塞了

# 终端2
echo "Hello, async IO!" > /dev/global_mem_0

# 终端1立刻输出：
got 15 bytes: Hello, async IO!
```

## 核心概念总结

| 概念 | 作用 |
|------|------|
| `wait_queue_head_t` | 等待队列，睡眠/唤醒机制基础 |
| `DECLARE_WAITQUEUE` | 定义一个等待队列项 |
| `add_wait_queue` | 将进程添加到等待队列 |
| `schedule()` | 让出CPU，进程进入睡眠 |
| `wake_up_interruptible` | 唤醒等待队列上的进程 |
| poll/select | **应用主动轮询**内核，问"谁就绪了" |
| fasync | **内核主动通知**应用，"我就绪了" |

## 参考
- 《Linux设备驱动开发详解》，阻塞非阻塞IO与异步通知章节
