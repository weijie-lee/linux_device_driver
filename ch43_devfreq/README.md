# ch43_devfreq: Devfreq

用 performance governor 登记一个单频点的 devfreq 设备，不替换 CPU 频率驱动。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch43_devfreq
make
sudo insmod virt_devfreq.ko
sudo rmmod virt_devfreq
```
