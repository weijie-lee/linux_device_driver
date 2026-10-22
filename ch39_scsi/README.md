# SCSI 主机

## 代码知识点

SCSI 主机是 `Scsi_Host`。`scsi_host_alloc()` 分配，`scsi_add_host()` 公布，`scsi_scan_host()` 扫描目标。命令在 `queuecommand` 里完成，驱动填 `scsi_cmnd` 的结果并调用 `scsi_done`。

本主机没有磁盘。`queuecommand` 把命令完成掉，用来看主机注册，而不是挂载文件系统。

## 代码架构

```
scsi_host_alloc
scsi_add_host
scsi_scan_host
queuecommand → scsi_done
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch39
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch39_scsi_virt_scsi.ko
insmod ok
+ lsmod | grep virt_scsi
virt_scsi              16384  0 
+ find /sys /dev /proc -name '*virt_scsi*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_scsi
/sys/bus/platform/devices/virt_scsi
/sys/bus/platform/drivers/virt_scsi
/sys/bus/platform/drivers/virt_scsi/virt_scsi
/sys/module/virt_scsi
/sys/module/virt_scsi/drivers/platform:virt_scsi
+ rmmod virt_scsi
rmmod ok
[PASS] ch39_scsi
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch39_scsi`
- 能找到 `virt_scsi`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch39
```
