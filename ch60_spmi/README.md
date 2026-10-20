# ch60_spmi: SPMI 总线

登记一个虚拟 SPMI 控制器，读写命令返回 0。依赖 spmi.ko。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch60_spmi
make
sudo insmod virt_spmi.ko
sudo rmmod virt_spmi
```
