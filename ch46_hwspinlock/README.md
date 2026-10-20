# ch46_hwspinlock: 硬件自旋锁

登记 1 把 id 从 32 起的虚拟 hwspinlock。trylock 总是成功。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch46_hwspinlock
make
sudo insmod virt_hwspin.ko
sudo rmmod virt_hwspin
```
