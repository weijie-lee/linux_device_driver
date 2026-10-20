# ch26_nvmem: NVMEM

用 256 字节 RAM 模拟 EEPROM，挂到 nvmem 子系统。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch26_nvmem
make
sudo insmod virt_nvmem.ko
sudo rmmod virt_nvmem
```
