# ch42_remoteproc: Remoteproc

登记一个不会自动启动的虚拟 remote processor，start/stop 为空操作。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch42_remoteproc
make
sudo insmod virt_rproc.ko
sudo rmmod virt_rproc
```
