# ch50_mdio: MDIO 总线

登记一条虚拟 MDIO 总线。phy_mask 屏蔽全部地址，避免扫描不存在的 PHY。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch50_mdio
make
sudo insmod virt_mdio.ko
sudo rmmod virt_mdio
```
