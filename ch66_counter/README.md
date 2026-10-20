# ch66_counter: Counter

登记一个单信号、单计数器的 counter 设备，读数恒为 0。依赖 counter.ko。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch66_counter
make
sudo insmod virt_counter.ko
sudo rmmod virt_counter
```
