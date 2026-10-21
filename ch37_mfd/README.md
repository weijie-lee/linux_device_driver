# MFD

## 代码知识点

MFD 是一个父设备拆成多个平台子设备。父驱动在 `probe` 里填 `mfd_cell` 数组，`devm_mfd_add_devices()` 为每个 cell 生成一个 platform 设备。子驱动按 cell 的名字匹配。

本父设备添加名为 `virt-mfd-cell` 的单元。没有第二块子驱动去 probe 它。实验看的是 cell 已经挂到 platform 总线上。

## 代码架构

```
mfd_cell name = virt-mfd-cell
devm_mfd_add_devices
  → platform 设备 virt-mfd-cell
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch37
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch37_mfd_virt_mfd.ko
insmod ok
+ lsmod | grep virt_mfd
virt_mfd               16384  0 
+ find /sys /dev /proc -name '*virt-mfd*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_mfd/virt-mfd-cell.1.auto
/sys/bus/platform/devices/virt-mfd-cell.1.auto
+ rmmod virt_mfd
rmmod ok
[PASS] ch37_mfd
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch37_mfd`
- 能找到 `virt-mfd` 或它的子设备。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch37
```
