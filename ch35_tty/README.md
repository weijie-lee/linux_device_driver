# ch35_tty: TTY / 串口

用 tty 层登记一个名为 ttyVIRT 的虚拟串口。write 丢弃数据并返回成功，对应 Linux tty/serial 驱动模型。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch35_tty
make
sudo insmod virt_tty.ko
sudo rmmod virt_tty
```
