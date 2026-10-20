# ch59_fpga: FPGA manager

登记一个始终处于 OPERATING 状态的 FPGA manager。依赖 fpga-mgr.ko。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch59_fpga
make
sudo insmod virt_fpga.ko
sudo rmmod virt_fpga
```
