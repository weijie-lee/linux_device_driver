# ch23_clk: 时钟框架

向 common clock framework 注册一路 24 MHz 固定时钟。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch23_clk
make
sudo insmod virt_clk.ko
sudo rmmod virt_clk
```
