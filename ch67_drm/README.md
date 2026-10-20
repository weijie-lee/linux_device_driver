# ch67_drm: DRM

登记一个只打开控制节点的 DRM 设备，不接管显示模式。依赖 drm.ko。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch67_drm
make
sudo insmod virt_drm.ko
sudo rmmod virt_drm
```
