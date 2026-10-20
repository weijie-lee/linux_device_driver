# ch25_reset: 复位控制器

注册 4 路虚拟 reset line，支持 assert、deassert 和 reset。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch25_reset
make
sudo insmod virt_reset.ko
sudo rmmod virt_reset
```
