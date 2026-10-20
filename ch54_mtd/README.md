# ch54_mtd: MTD

用 64 KiB RAM 模拟 MTD_RAM 设备并注册到 MTD 子系统。依赖 mtd.ko。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch54_mtd
make
sudo insmod virt_mtd.ko
sudo rmmod virt_mtd
```
