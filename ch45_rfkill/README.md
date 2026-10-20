# ch45_rfkill: RFKill

登记一个 WLAN 类型的虚拟 rfkill 开关。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch45_rfkill
make
sudo insmod virt_rfkill.ko
sudo rmmod virt_rfkill
```
