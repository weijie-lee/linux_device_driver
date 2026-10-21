# FPGA manager

## 代码知识点

FPGA manager 负责加载比特流。`fpga_manager_ops` 有 `write_init`、`write`、`write_complete`。`devm_fpga_mgr_register()` 把它放进 `/sys/class/fpga_manager/`。

本管理器接受写入并报告完成，不配置真实 FPGA。

## 代码架构

```
fpga_manager_ops
  write_init / write / write_complete
devm_fpga_mgr_register("virt-fpga")
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch59
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch59_fpga_virt_fpga.ko
insmod ok
+ lsmod | grep virt_fpga
virt_fpga              16384  0 
fpga_mgr               24576  1 virt_fpga
+ find /sys /dev /proc -name '*virt_fpga*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_fpga
/sys/bus/platform/devices/virt_fpga
/sys/bus/platform/drivers/virt_fpga
/sys/bus/platform/drivers/virt_fpga/virt_fpga
/sys/module/virt_fpga
/sys/module/virt_fpga/drivers/platform:virt_fpga
/sys/module/fpga_mgr/holders/virt_fpga
+ cat /sys/class/fpga_manager/*/name
virt-fpga
+ rmmod virt_fpga
rmmod ok
[PASS] ch59_fpga
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch59_fpga`
- 能找到 `virt-fpga`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch59
```
