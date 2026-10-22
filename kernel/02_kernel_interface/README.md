# 驱动和内核的接口

`kernel/01_driver_framework` 看的是驱动自己注册的 `match` / `probe`。这一步看内核从哪一个函数走进这些回调，以及字符设备的 `file_operations` 是从哪一个函数被调用的。

模块在 `insmod` 时只向内核登记接口，不绑定设备：

- `bus_register`、`driver_register`：总线和驱动先就位
- `alloc_chrdev_region`、`cdev_add`、`class_create`、`device_create`：字符设备 `/dev/virt_kif` 先出现

设备留到后面再注册。实验先打开 ftrace，再写 `/sys/bus/virtkif/bind`，同一次调用里就能看见内核函数和驱动函数的先后。

## 代码知识点

驱动和内核之间有两个方向。

**驱动调用内核。** 上面列的注册函数都是这个方向。`device_register()` 内部就是 `device_initialize()` 加 `device_add()`。ftrace 里能看到 `device_add <-device_register`，调用者是驱动的 `virt_kif_bind_store`。

**内核调用驱动。** 内核拿着一张函数表。绑定走 `bus_type` 和 `device_driver`，读写字符设备走 `file_operations`。5.15 里这两张表都不是由驱动自己顺着调用栈找进去的，中间还有内核函数。

### 写 sysfs 之后，绑定是怎么发生的

`/sys/bus/virtkif/bind` 是总线的属性。写它的路径在 5.15 里是：

```
new_sync_write
  kernfs_fop_write_iter
    sysfs_kf_write
      bus_attr_store
        virt_kif_bind_store
          device_register
            device_add
              dev_uevent → virt_kif_uevent          KOBJ_ADD，这时还没有 probe
              bus_probe_device
                device_initial_probe
                  __device_attach_driver
                    virt_kif_match
                    driver_probe_device
                      __driver_probe_device
                        really_probe
                          virt_kif_bus_probe
                            virt_kif_driver_probe
                          dev_uevent → virt_kif_uevent    KOBJ_BIND
```

`really_probe()` 在 `bus->probe` 非空时只调用总线的 probe。跟踪里 `virt_kif_bus_probe <-really_probe`，`virt_kif_driver_probe <-virt_kif_bus_probe`：驱动的 probe 是总线转进去的，不是 `really_probe` 直接调用的。这和 `kernel/01` 里 `platform_probe()` 的规则相同，这里用 ftrace 看到了调用者。

`virt_kif_match` 和 `driver_probe_device` 的调用者都是 `__device_attach_driver`。匹配返回 1 之后，同一次附着才进入 probe。

这些函数里，`really_probe`、`bus_probe_device`、`kernfs_fop_write_iter`、`sysfs_kf_write`、`bus_attr_store` 在 `System.map` 里是本地符号（`t`）。这台客户机打开了函数跟踪，所以过滤表里写得进这些名字。回调都标了 `noinline`，否则编译器会把它们折进调用者，过滤表里就没有单独一项。

设备在 `insmod` 时不注册，是为了让跟踪先打开。如果在 `module_init` 里就 `device_register`，probe 发生在用户态还来不及写 `set_ftrace_filter` 之前。

第二次写 `bind` 返回 `-EBUSY`。静态的 `struct device` 注册过一次就不能再注册。

### 打开字符设备之后，read 是怎么进来的

`cdev_add` 把 `file_operations` 交给字符设备层。打开 `/dev/virt_kif` 时，VFS 先调用字符设备默认的 `open`，也就是 `chrdev_open`。它找到 `cdev`，换上驱动的 `file_operations`，再调用驱动的 `open`。跟踪里是 `chrdev_open <-do_dentry_open`，然后 `virt_kif_open <-chrdev_open`。

5.15 的 `vfs_read()` 在 `f_op->read` 非空时直接调用它。跟踪里是 `vfs_read <-ksys_read`，然后 `virt_kif_read <-vfs_read`。`read` 返回 `kif`。

函数跟踪记录的是函数进入的时刻。如果用 `data=$(head ...)` 把结果读回 shell，shell 自己对管道的 `vfs_read` 会先进入并堵住，排在 `chrdev_open` 前面。实验把 `head` 的输出写到文件，窗口里只留下 `head` 这一次打开和读取。

## 代码架构

```
module_init
  bus_register
  driver_register                         总线上还没有设备，probe 不跑
  alloc_chrdev_region → cdev_add
  class_create → device_create           /dev/virt_kif

echo 1 > /sys/bus/virtkif/bind           ftrace 已经打开
  kernfs → sysfs → bus_attr_store
  device_add → really_probe → 两层 probe

head -c 4 /dev/virt_kif
  chrdev_open → file_operations.open
  vfs_read → file_operations.read

module_exit
  device_destroy → cdev_del → class_destroy
  driver_unregister                        bus_remove → driver_remove
  device_unregister                        release
  bus_unregister
```

`/sys/bus/virtkif/trace` 是模块自己记下的回调顺序，用来和 ftrace 对照。ftrace 多出来的是内核那一侧的函数名和调用者。

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter kernel02
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、CPU 号、进程号每次都不同，对照函数名和 `<-` 后面的调用者即可。

```
+ insmod /lib/modules/driver_test/kernel_02_kernel_interface_virt_kif.ko
insmod ok
+ test -c /dev/virt_kif -o -b /dev/virt_kif
node ok
+ echo 0 > /sys/kernel/tracing/tracing_on
+ echo nop > /sys/kernel/tracing/current_tracer
+ echo function > /sys/kernel/tracing/current_tracer
+ echo kernfs_fop_write_iter > /sys/kernel/tracing/set_ftrace_filter
+ echo sysfs_kf_write >> /sys/kernel/tracing/set_ftrace_filter
+ echo bus_attr_store >> /sys/kernel/tracing/set_ftrace_filter
+ echo virt_kif_bind_store >> /sys/kernel/tracing/set_ftrace_filter
+ echo device_add >> /sys/kernel/tracing/set_ftrace_filter
+ echo virt_kif_uevent >> /sys/kernel/tracing/set_ftrace_filter
+ echo bus_probe_device >> /sys/kernel/tracing/set_ftrace_filter
+ echo device_initial_probe >> /sys/kernel/tracing/set_ftrace_filter
+ echo virt_kif_match >> /sys/kernel/tracing/set_ftrace_filter
+ echo driver_probe_device >> /sys/kernel/tracing/set_ftrace_filter
+ echo really_probe >> /sys/kernel/tracing/set_ftrace_filter
+ echo virt_kif_bus_probe >> /sys/kernel/tracing/set_ftrace_filter
+ echo virt_kif_driver_probe >> /sys/kernel/tracing/set_ftrace_filter
+ echo > /sys/kernel/tracing/trace
+ echo 1 > /sys/kernel/tracing/tracing_on
+ echo 1 > /sys/bus/virtkif/bind
bind ok
+ echo 0 > /sys/kernel/tracing/tracing_on
+ head -n 30 /sys/kernel/tracing/trace
# tracer: function
#
# entries-in-buffer/entries-written: 14/14   #P:2
#
#                                _-----=> irqs-off
#                               / _----=> need-resched
#                              | / _---=> hardirq/softirq
#                              || / _--=> preempt-depth
#                              ||| / _-=> migrate-disable
#                              |||| /     delay
#           TASK-PID     CPU#  |||||  TIMESTAMP  FUNCTION
#              | |         |   |||||     |         |
              sh-1       [001] .....     9.190779: kernfs_fop_write_iter <-new_sync_write
              sh-1       [001] .....     9.191103: sysfs_kf_write <-kernfs_fop_write_iter
              sh-1       [001] .....     9.191134: bus_attr_store <-sysfs_kf_write
              sh-1       [001] .....     9.191147: virt_kif_bind_store <-bus_attr_store
              sh-1       [001] .....     9.191274: device_add <-device_register
              sh-1       [001] .....     9.192155: virt_kif_uevent <-dev_uevent
              sh-1       [001] .....     9.192666: bus_probe_device <-device_add
              sh-1       [001] .....     9.192678: device_initial_probe <-bus_probe_device
              sh-1       [001] .....     9.192740: virt_kif_match <-__device_attach_driver
              sh-1       [001] .....     9.192790: driver_probe_device <-__device_attach_driver
              sh-1       [001] .....     9.192876: really_probe <-__driver_probe_device
              sh-1       [001] .....     9.193076: virt_kif_bus_probe <-really_probe
              sh-1       [001] .....     9.193111: virt_kif_driver_probe <-virt_kif_bus_probe
              sh-1       [001] .....     9.193293: virt_kif_uevent <-dev_uevent
+ echo 0 > /sys/kernel/tracing/tracing_on
+ echo nop > /sys/kernel/tracing/current_tracer
bind-hits: kernfs_fop_write_iter sysfs_kf_write bus_attr_store virt_kif_bind_store device_add virt_kif_uevent bus_probe_device device_initial_probe virt_kif_match driver_probe_device really_probe virt_kif_bus_probe virt_kif_driver_probe virt_kif_uevent
bind order ok
+ cat /sys/bus/virtkif/trace
bus_register > uevent virtkif-sensor > match virtkif-sensor 1 > bus_probe virtkif-sensor > driver_probe virtkif-sensor > uevent virtkif-sensor
callback trace ok
+ cat /sys/bus/virtkif/devices/virtkif-sensor/bound
1
+ test -e /sys/bus/virtkif/devices/virtkif-sensor/driver
sensor bound
+ echo 1 > /sys/bus/virtkif/bind
bind-refused
+ echo 0 > /sys/kernel/tracing/tracing_on
+ echo nop > /sys/kernel/tracing/current_tracer
+ echo function > /sys/kernel/tracing/current_tracer
+ echo chrdev_open > /sys/kernel/tracing/set_ftrace_filter
+ echo virt_kif_open >> /sys/kernel/tracing/set_ftrace_filter
+ echo vfs_read >> /sys/kernel/tracing/set_ftrace_filter
+ echo virt_kif_read >> /sys/kernel/tracing/set_ftrace_filter
+ echo > /sys/kernel/tracing/trace
+ echo 1 > /sys/kernel/tracing/tracing_on
+ head -c 4 /dev/virt_kif > /tmp/kif_out
+ echo 0 > /sys/kernel/tracing/tracing_on
+ head -n 20 /sys/kernel/tracing/trace
# tracer: function
#
# entries-in-buffer/entries-written: 4/4   #P:2
#
#                                _-----=> irqs-off
#                               / _----=> need-resched
#                              | / _---=> hardirq/softirq
#                              || / _--=> preempt-depth
#                              ||| / _-=> migrate-disable
#                              |||| /     delay
#           TASK-PID     CPU#  |||||  TIMESTAMP  FUNCTION
#              | |         |   |||||     |         |
            head-164     [000] .....     9.910907: chrdev_open <-do_dentry_open
            head-164     [000] .....     9.911001: virt_kif_open <-chrdev_open
            head-164     [000] .....     9.911276: vfs_read <-ksys_read
            head-164     [000] .....     9.911300: virt_kif_read <-vfs_read
+ echo 0 > /sys/kernel/tracing/tracing_on
+ echo nop > /sys/kernel/tracing/current_tracer
vfs-hits: chrdev_open virt_kif_open vfs_read virt_kif_read
vfs order ok
+ cat /tmp/kif_out
kif
read ok
+ rmmod virt_kif
rmmod ok
+ dmesg | grep virtkif:
[    1.913471] virtkif: bus_register
[    9.192373] virtkif: uevent virtkif-sensor
[    9.192767] virtkif: match virtkif-sensor 1
[    9.193089] virtkif: bus_probe virtkif-sensor
[    9.193121] virtkif: driver_probe virtkif-sensor
[    9.193295] virtkif: uevent virtkif-sensor
[    9.911027] virtkif: cdev_open
[    9.911341] virtkif: cdev_read
[   10.006538] virtkif: bus_remove virtkif-sensor
[   10.006607] virtkif: driver_remove virtkif-sensor
[   10.006709] virtkif: uevent virtkif-sensor
[   10.006981] virtkif: uevent virtkif-sensor
[   10.007005] virtkif: release virtkif-sensor
[   10.007142] virtkif: bus_unregister
[PASS] kernel02_interface
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] kernel02_interface`
- `bind-hits` 整行是 `kernfs_fop_write_iter sysfs_kf_write bus_attr_store virt_kif_bind_store device_add virt_kif_uevent bus_probe_device device_initial_probe virt_kif_match driver_probe_device really_probe virt_kif_bus_probe virt_kif_driver_probe virt_kif_uevent`
- 跟踪原文里能看到 `virt_kif_bus_probe <-really_probe` 和 `virt_kif_driver_probe <-virt_kif_bus_probe`
- `/sys/bus/virtkif/trace` 整行是 `bus_register > uevent virtkif-sensor > match virtkif-sensor 1 > bus_probe virtkif-sensor > driver_probe virtkif-sensor > uevent virtkif-sensor`
- `virtkif-sensor` 的 `bound` 是 1，并且存在 `driver` 链接；第二次写 `bind` 得到 `bind-refused`
- `vfs-hits` 整行是 `chrdev_open virt_kif_open vfs_read virt_kif_read`，跟踪原文里能看到 `virt_kif_open <-chrdev_open` 和 `virt_kif_read <-vfs_read`
- `/tmp/kif_out` 的内容是 `kif`
- `rmmod` 之后 `dmesg` 里有 `bus_remove virtkif-sensor`、`driver_remove virtkif-sensor`、`release virtkif-sensor`、`bus_unregister`

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter kernel02
```
