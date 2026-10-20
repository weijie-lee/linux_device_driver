# ch22_pinctrl: 引脚控制器

登记 4 个引脚和一个引脚组，覆盖 pinctrl 子系统的控制器注册路径。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch22_pinctrl
make
sudo insmod virt_pinctrl.ko
sudo rmmod virt_pinctrl
```
