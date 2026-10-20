# ch32_phy: 通用 PHY

用 generic PHY 框架登记一条虚拟 PHY（init/power_on/power_off）。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch32_phy
make
sudo insmod virt_phy.ko
sudo rmmod virt_phy
```
