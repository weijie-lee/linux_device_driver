# ch29_thermal: 热区

注册名为 virt_thermal 的热区，读数固定 30°C，不启动轮询。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch29_thermal
make
sudo insmod virt_thermal.ko
sudo rmmod virt_thermal
```
