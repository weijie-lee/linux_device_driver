# ch55_uio: UIO

登记一个没有中断、没有内存映射的 UIO 设备，使用户态可以看到 /dev/uioN。依赖 uio.ko。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch55_uio
make
sudo insmod virt_uio.ko
sudo rmmod virt_uio
```
