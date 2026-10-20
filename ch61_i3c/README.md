# ch61_i3c: I3C 主机

登记一个虚拟 I3C master。bus_init 与 DAA 为空操作。依赖 i3c.ko。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch61_i3c
make
sudo insmod virt_i3c.ko
sudo rmmod virt_i3c
```
