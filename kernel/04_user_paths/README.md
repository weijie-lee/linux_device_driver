# 用户态进驱动的几条路径

`kernel/03_driver_calls` 看的是驱动调用 `copy_from_user`。这一章看反方向的入口：同一份数据从 ioctl、sysfs、procfs 进驱动，能力检查由驱动自己做。

模块用 `misc_register()` 登记 `/dev/virt_upath`。`compat_ioctl` 指向 `compat_ptr_ioctl`，32 位程序和 64 位程序共用同一个 `unlocked_ioctl`。

## 代码知识点

**ioctl。** 64 位 `ioctl` 系统调用进 `__x64_sys_ioctl`。这台内核把 `vfs_ioctl()` 内联进了系统调用入口，函数跟踪里看不到 `vfs_ioctl` 这个名字，调用者直接是 `__x64_sys_ioctl`：

```
virt_upath_ioctl <-__x64_sys_ioctl
capable <-virt_upath_ioctl
```

`UPATH_SET` 用 `get_user()` 取一个 `__u32`，`UPATH_GET` 用 `copy_to_user()` 交还 `struct upath_info`。结构体在 i386 和 x86_64 上都是 16 字节，`_IOR` 编进去的长度相同。

**32 位 ioctl。** 客户机打开了 `CONFIG_IA32_EMULATION`。32 位程序走 `__do_compat_sys_ioctl`，再进 `file_operations.compat_ioctl`。`compat_ptr_ioctl()` 把 32 位指针扩成内核指针后调用同一个 `unlocked_ioctl`：

```
compat_ptr_ioctl <-__do_compat_sys_ioctl
virt_upath_ioctl <-compat_ptr_ioctl
```

**capable。** 打开设备时 VFS 只查文件权限，`/dev/virt_upath` 是 `crw-------`。`UPATH_SET` 另外要求 `CAP_SYS_ADMIN`。程序先以 root 打开，再 `setresuid(65534)`，用已经打开的 fd 写。第二次写失败是驱动里的 `capable()` 返回 0，`errno` 是 `EPERM`。降权之后 `/proc/self/status` 里 `CapEff` 是 0。

**sysfs。** `device_attribute.show` 不由 `sysfs_kf_read` 直接调用。5.15 的 kernfs 用 seq_file 读属性：

```
sysfs_kf_seq_show <-kernfs_seq_show
dev_attr_show <-sysfs_kf_seq_show
upath_value_show <-dev_attr_show
```

**procfs。** `proc_create_single()` 的 show 由 `seq_read_iter()` 调用。busybox 的 `cat` 走 splice，所以 `proc_reg_read_iter` 的调用者是 `generic_file_splice_read`：

```
proc_reg_read_iter <-generic_file_splice_read
seq_read_iter <-proc_reg_read_iter
virt_upath_proc_show <-seq_read_iter
```

**sparse。** `kernel/04_user_paths/sparse/bad_user.c` 不进 `make all`，也不在客户机里加载。它把用户态指针交给 `copy_from_user` 时少了 `__user`，又把带 `__user` 的指针直接解引用。gcc 不报，sparse 报两条地址空间警告。正确写法在 `virt_upath.c`。

## 代码架构

```
module_init
  proc_create_single("virt_upath")
  misc_register                         /dev/virt_upath
                                        /sys/class/misc/virt_upath/value

ioctl UPATH_SET / UPATH_GET
  __x64_sys_ioctl → virt_upath_ioctl
    capable(CAP_SYS_ADMIN)              只有 SET 检查
32 位 ioctl
  __do_compat_sys_ioctl → compat_ptr_ioctl → virt_upath_ioctl

cat /sys/class/misc/virt_upath/value
  kernfs_seq_show → sysfs_kf_seq_show → dev_attr_show → upath_value_show

cat /proc/virt_upath
  generic_file_splice_read → proc_reg_read_iter → seq_read_iter → virt_upath_proc_show
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter kernel04
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、CPU 号、进程号每次都不同，对照函数名和 `<-` 后面的调用者即可。

```
======== kernel04_user_paths ========
+ insmod /lib/modules/driver_test/kernel_04_user_paths_virt_upath.ko
insmod ok
+ test -c /dev/virt_upath -o -b /dev/virt_upath
node ok
+ ls -l /dev/virt_upath
crw-------    1 0        0          10, 123 Oct 23 10:01 /dev/virt_upath
+ /bin/upath_ioctl
abi: sizeof(long)=8 sizeof(struct upath_info)=16 UPATH_GET=0x80107501
  [PASS] open /dev/virt_upath as root
  [PASS] UPATH_SET 7 as root
  [PASS] UPATH_GET returns 7
  value=7 calls=2
  Uid:	65534	65534	65534	65534
  CapEff:	0000000000000000
  UPATH_SET 9 as uid 65534: ret=-1 errno=Operation not permitted
  [PASS] UPATH_SET without CAP_SYS_ADMIN is EPERM
  [PASS] UPATH_GET still 7
PASS: 5  FAIL: 0
     upath_ioctl-163     [001] .....     7.847816: virt_upath_ioctl <-__x64_sys_ioctl
     upath_ioctl-163     [001] .....     7.848109: capable <-virt_upath_ioctl
     upath_ioctl-163     [001] .....     7.848616: virt_upath_ioctl <-__x64_sys_ioctl
     upath_ioctl-163     [001] .....     7.854443: virt_upath_ioctl <-__x64_sys_ioctl
     upath_ioctl-163     [001] .....     7.854449: capable <-virt_upath_ioctl
     upath_ioctl-163     [001] .....     7.855024: virt_upath_ioctl <-__x64_sys_ioctl
ioctl-hits: driver_ioctl capable driver_ioctl driver_ioctl capable driver_ioctl
ioctl order ok
[    7.848284] virt_upath: value set to 7 by uid 0
+ /bin/upath_ioctl32
abi: sizeof(long)=4 sizeof(struct upath_info)=16 UPATH_GET=0x80107501
  UPATH_SET 9 as uid 65534: ret=-1 errno=Operation not permitted
PASS: 5  FAIL: 0
   upath_ioctl32-182     [001] .....     8.340014: compat_ptr_ioctl <-__do_compat_sys_ioctl
   upath_ioctl32-182     [001] .....     8.340038: virt_upath_ioctl <-compat_ptr_ioctl
compat-hits: compat_ptr_ioctl driver_ioctl compat_ptr_ioctl driver_ioctl compat_ptr_ioctl driver_ioctl compat_ptr_ioctl driver_ioctl
compat order ok
             cat-191     [000] .....     8.903797: sysfs_kf_seq_show <-kernfs_seq_show
             cat-191     [000] .....     8.903870: dev_attr_show <-sysfs_kf_seq_show
             cat-191     [000] .....     8.903884: upath_value_show <-dev_attr_show
sysfs-hits: sysfs_kf_seq_show dev_attr_show driver_show
sysfs order ok
11
             cat-199     [001] .....     9.459489: proc_reg_read_iter <-generic_file_splice_read
             cat-199     [001] .....     9.459516: seq_read_iter <-proc_reg_read_iter
             cat-199     [001] .....     9.459542: virt_upath_proc_show <-seq_read_iter
proc-hits: proc_reg_read_iter seq_read_iter driver_show proc_reg_read_iter seq_read_iter proc_reg_read_iter seq_read_iter
proc order ok
value=11 calls=6
+ rmmod virt_upath
rmmod ok
[PASS] kernel04_user_paths
```

成功时客户机打印 `[PASS] kernel04_user_paths`。64 位程序结尾有 `FAIL: 0` 和 `sizeof(long)=8`，32 位程序有 `sizeof(long)=4`。`ioctl-hits` 里 `driver_ioctl` 和 `capable` 按这个顺序出现两次：root 的 `UPATH_SET` 过 `capable()`，uid 65534 的 `UPATH_SET` 再过一次。两次 `UPATH_GET` 不调用 `capable()`。

sparse 在容器里跑，不进客户机：

```bash
docker run --rm -v "$PWD":/linux_device_driver -w /linux_device_driver ldd-lab \
    make -C kernel/04_user_paths sparse
```

这次运行打出两条警告：

```
sparse/bad_user.c:22:40: warning: incorrect type in argument 2 (different address spaces)
sparse/bad_user.c:22:40:    expected void const [noderef] __user *from
sparse/bad_user.c:22:40:    got unsigned int [usertype] *plain
sparse/bad_user.c:26:17: warning: dereference of noderef expression
```
