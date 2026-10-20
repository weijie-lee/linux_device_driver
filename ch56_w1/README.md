# ch56_w1: 1-Wire

登记一个虚拟 1-Wire 主机。reset 报告总线上没有从设备。依赖 wire.ko。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch56_w1
make
sudo insmod virt_w1.ko
sudo rmmod virt_w1
```
