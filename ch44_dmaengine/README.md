# ch44_dmaengine: DMA 引擎

向 dmaengine 注册一个支持 memcpy 能力位的虚拟通道。prep 返回 NULL，表示当前无法排队。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch44_dmaengine
make
sudo insmod virt_dmaeng.ko
sudo rmmod virt_dmaeng
```
