# ch39_scsi: SCSI 主机

登记一个虚拟 SCSI host。扫描到的命令以 DID_NO_CONNECT 完成，不访问磁盘。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch39_scsi
make
sudo insmod virt_scsi.ko
sudo rmmod virt_scsi
```
