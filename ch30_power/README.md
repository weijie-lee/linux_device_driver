# ch30_power: 电源供应

注册虚拟电池 virt-battery，报告容量、电压和在位状态。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch30_power
make
sudo insmod virt_psy.ko
sudo rmmod virt_psy
```
