# ch34_ptp: PTP 时钟

登记一个读系统实时钟的虚拟 PTP hardware clock。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch34_ptp
make
sudo insmod virt_ptp.ko
sudo rmmod virt_ptp
```
