# 内存块设备

## 代码知识点

块设备按扇区访问。5.15 用 `blk_mq_alloc_disk()` 分配 `gendisk`，`set_capacity()` 设定扇区数，`add_disk()` 公布出去。请求在 `blk_mq_ops.queue_rq` 里完成，驱动把 bio 的页拷进自己的 `vmalloc` 后备存储。

节点名是 `vmem_disk0`。它不是硬盘，卸载前不要把它当根文件系统。

## 代码架构

```
register_blkdev
vmalloc 后备存储
blk_mq_alloc_disk
set_capacity
add_disk → /dev/vmem_disk0
queue_rq：bio ↔ 后备存储
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch17
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch17_block_vmem_disk.ko
insmod ok
+ ls /dev/vmem_disk0 /sys/block
/dev/vmem_disk0

/sys/block:
loop0
loop1
loop2
loop3
loop4
loop5
loop6
loop7
vmem_disk0
vmem_disk1
+ rmmod vmem_disk
rmmod ok
[PASS] ch17_block
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch17_block`
- `/dev/vmem_disk0` 或 `/sys/block` 里能看到 `vmem_disk`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch17
```
