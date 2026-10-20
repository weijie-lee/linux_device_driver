# ch37_mfd: 多功能设备

用 mfd_add_devices 挂出一个名为 virt-mfd-cell 的子 platform 设备。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch37_mfd
make
sudo insmod virt_mfd.ko
sudo rmmod virt_mfd
```
