# 驱动调用内核

`kernel/02_kernel_interface` 看的是内核走进驱动：`really_probe` 调用总线 probe，`vfs_read` 调用 `file_operations.read`。这一步看反方向。驱动拿到用户态指针之后，自己调用内核把字节搬过地址空间边界。

模块用 `misc_register()` 登记 `/dev/virt_call`。缓冲只有 64 字节。一次 `write` 放进内核，下一次打开再 `read` 出来。

## 代码知识点

交互有两段，都发生在同一次系统调用里。

**内核调用驱动。** `vfs_write()` 在 `f_op->write` 非空时直接调用它。跟踪里是 `virt_call_write <-vfs_write`。`vfs_read()` 同样直接调用 `virt_call_read`。

**驱动调用内核。** 用户态指针不能在内核里直接解引用。5.15 的 x86 上，`copy_from_user()` 和 `copy_to_user()` 是内联函数，函数跟踪里单独的符号是 `_copy_from_user` 和 `_copy_to_user`。驱动的 read/write 标了 `noinline`，所以调用者落在驱动函数上，而不是被折进别的函数。

写 4 个字节 `ping` 时，进入顺序是：

```
vfs_write
  virt_call_write
    _copy_from_user
```

读回这 4 个字节时：

```
vfs_read
  virt_call_read
    _copy_to_user
```

函数跟踪记的是函数进入的时刻，所以过滤之后的顺序就是上面这两列。`_copy_from_user` 的调用者是 `virt_call_write`，`_copy_to_user` 的调用者是 `virt_call_read`。

`copy_from_user()` 可能睡眠，所以缓冲用 mutex 保护，不放在自旋锁里。超过 64 字节的写入只保留前 64 字节，返回值是实际拷进内核的长度。这个长度一路回到 `write` 系统调用的返回值。

第二次打开设备时偏移从 0 开始，所以读到的是整段缓冲。实验把 `head` 的输出写到文件，避免 shell 自己的 `vfs_read` 混进跟踪窗口。

## 代码架构

```
module_init
  misc_register                         /dev/virt_call

printf ping > /dev/virt_call            ftrace 已经打开
  vfs_write → virt_call_write
    _copy_from_user                     用户态字节进入 call_buf

head -c 4 /dev/virt_call
  vfs_read → virt_call_read
    _copy_to_user                       call_buf 回到用户态

module_exit
  misc_deregister
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter kernel03
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。方括号里的时间戳、CPU 号、进程号每次都不同，对照函数名和 `<-` 后面的调用者即可。

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] kernel03_calls`
- `write-hits` 整行是 `vfs_write virt_call_write _copy_from_user`
- 跟踪原文里能看到 `_copy_from_user <-virt_call_write`
- `read-hits` 整行是 `vfs_read virt_call_read _copy_to_user`
- 跟踪原文里能看到 `_copy_to_user <-virt_call_read`
- `/tmp/call_out` 的内容是 `ping`
- `dmesg` 里有 `virt_call: stored 4` 和 `virt_call: returned 4`

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter kernel03
```
