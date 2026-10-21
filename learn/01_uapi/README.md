# 用户接口：read/write、ioctl、poll、mmap、sysfs、debugfs

`ch02` 只有 read/write。一块设备通常还要决定控制命令、等待数据和共享内存走哪条接口。本模块把这些都放在 `/dev/virt_uapi` 上，缓冲区是一页内存。

| 接口 | 什么时候用 | 本模块 |
| --- | --- | --- |
| read/write | 字节流，调用方式和文件一样 | 读写这一页，长度记在 `len` |
| ioctl | 一次调用里带命令和结构体，不是字节流 | `VIRT_UAPI_GET_LEN`、`VIRT_UAPI_CLEAR` |
| poll | 调用者要和别的 fd 一起等 | 有数据返回 `POLLIN`，`CLEAR` 之后不再返回 |
| mmap | 大块数据不想每次拷贝 | `remap_pfn_range` 映射同一页，用户写入后 `read` 能看见 |
| sysfs | 一个属性一个文件，脚本好用 | `/sys/class/misc/virt_uapi/bytes` |
| debugfs | 只给开发时看，不承诺稳定 | `/sys/kernel/debug/virt_uapi/flags` |

命令号在 `virt_uapi_cmd.h`，内核模块和 `uapi_test.c` 共用。用户程序要静态链接，QEMU 的 initramfs 里没有动态装载器。

```bash
cd learn/01_uapi
make
# 在 5.15 客户机里：
insmod virt_uapi.ko
./uapi_test
```

`copy_to_user` / `copy_from_user` 可能睡眠，所以不在自旋锁里调用。这里用 mutex。`poll` 先 `poll_wait` 再看 `len`，避免数据和等待队列之间的丢失唤醒。
