# ch65_v4l2: V4L2 视频

登记一个只实现 querycap 的视频采集节点。依赖 videodev.ko 与 mc.ko。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch65_v4l2
make
sudo insmod virt_v4l2.ko
sudo rmmod virt_v4l2
```
