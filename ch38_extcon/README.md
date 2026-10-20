# ch38_extcon: 外部连接器

登记 extcon 设备并报告 USB 电缆已连接。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch38_extcon
make
sudo insmod virt_extcon.ko
sudo rmmod virt_extcon
```
