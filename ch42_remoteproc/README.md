# remoteproc

## 代码知识点

remoteproc 管一块远程处理器的生命周期。`rproc_alloc()` 配上 `rproc_ops` 的 `start` 和 `stop`，`rproc_add()` 注册。sysfs 在 `/sys/class/remoteproc/remoteprocN/`，`state` 可以读，写入 `start` 会调到 `ops.start`。

本处理器没有固件。`start` 只改状态。

## 代码架构

```
rproc_alloc(name virt-rproc)
  ops.start / ops.stop
rproc_add
/sys/class/remoteproc/
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch42
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch42_remoteproc_virt_rproc.ko
insmod ok
+ lsmod | grep virt_rproc
virt_rproc             16384  0 
+ find /sys /dev /proc -name '*virt_rproc*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_rproc
/sys/bus/platform/devices/virt_rproc
/sys/bus/platform/drivers/virt_rproc
/sys/bus/platform/drivers/virt_rproc/virt_rproc
/sys/module/virt_rproc
/sys/module/virt_rproc/drivers/platform:virt_rproc
+ cat /sys/class/remoteproc/remoteproc*/name
virt-rproc
+ rmmod virt_rproc
rmmod ok
[PASS] ch42_remoteproc
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch42_remoteproc`
- 能找到 `virt-rproc`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch42
```
