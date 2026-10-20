# ch64_bluetooth: 蓝牙 HCI

登记一个 HCI_VIRTUAL 控制器。发送路径释放 skb。依赖 bluetooth.ko。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch64_bluetooth
make
sudo insmod virt_bt.ko
sudo rmmod virt_bt
```
