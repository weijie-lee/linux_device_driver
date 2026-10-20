# ch33_pps: PPS 秒脉冲

向 PPS 子系统登记一个可捕获 assert 事件的虚拟源。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch33_pps
make
sudo insmod virt_pps.ko
sudo rmmod virt_pps
```
