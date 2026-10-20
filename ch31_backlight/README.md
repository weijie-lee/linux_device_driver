# ch31_backlight: 背光

注册 backlight class 设备，亮度范围 0–100。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch31_backlight
make
sudo insmod virt_backlight.ko
sudo rmmod virt_backlight
```
