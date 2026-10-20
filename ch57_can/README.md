# ch57_can: CAN

用 alloc_candev 登记一个虚拟 CAN 控制器。依赖 can.ko 与 can-dev.ko。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch57_can
make
sudo insmod virt_can.ko
sudo rmmod virt_can
```
