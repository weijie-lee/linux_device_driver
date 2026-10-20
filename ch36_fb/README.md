# ch36_fb: 帧缓冲

分配一块 320x240 RGB565 的系统内存，注册为 framebuffer。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch36_fb
make
sudo insmod virt_fb.ko
sudo rmmod virt_fb
```
