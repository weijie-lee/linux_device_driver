# ch51_iio: IIO 传感器

登记一个直接模式的 IIO 电压通道，raw 读数固定为 1234。依赖 industrialio.ko。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch51_iio
make
sudo insmod virt_iio.ko
sudo rmmod virt_iio
```
