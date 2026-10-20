# ch63_nfc: NFC

登记一个支持 NFC-DEP 的虚拟 NFC 控制器。依赖 nfc.ko。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch63_nfc
make
sudo insmod virt_nfc.ko
sudo rmmod virt_nfc
```
