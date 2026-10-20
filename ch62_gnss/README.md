# ch62_gnss: GNSS

登记一个 NMEA 类型的虚拟 GNSS 接收机。依赖 gnss.ko。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch62_gnss
make
sudo insmod virt_gnss.ko
sudo rmmod virt_gnss
```
