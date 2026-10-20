# ch41_virtio: Virtio 驱动

登记一个不匹配任何真实 virtio 设备 ID 的 virtio 驱动，演示驱动注册路径。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch41_virtio
make
sudo insmod virt_virtio.ko
sudo rmmod virt_virtio
```
