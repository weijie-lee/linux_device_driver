# ch49_opp: OPP 工作点

给 platform 设备添加一个 1 GHz / 900 mV 的动态 OPP。CPU 频率驱动保持不动。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch49_opp
make
sudo insmod virt_opp.ko
sudo rmmod virt_opp
```
