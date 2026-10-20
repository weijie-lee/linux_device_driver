# ch48_powercap: Powercap

向 powercap 框架登记控制类型 virt_pcap。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch48_powercap
make
sudo insmod virt_powercap.ko
sudo rmmod virt_powercap
```
