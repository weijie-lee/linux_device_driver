# ch53_hid: HID

构造一个带键盘报告描述符的虚拟 HID 设备并交给 hid 核心解析。依赖 hid.ko。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch53_hid
make
sudo insmod virt_hid.ko
sudo rmmod virt_hid
```
