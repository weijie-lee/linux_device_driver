# ch24_regulator: 稳压器

注册一颗固定 1.8 V 的 regulator，支持 enable/disable。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch24_regulator
make
sudo insmod virt_regulator.ko
sudo rmmod virt_regulator
```
