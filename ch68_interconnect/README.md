# ch68_interconnect: 互联带宽

向 interconnect 框架登记一个提供者和一个节点。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch68_interconnect
make
sudo insmod virt_icc.ko
sudo rmmod virt_icc
```
