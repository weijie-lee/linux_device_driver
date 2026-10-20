# ch21_gpio: GPIO 控制器

注册 8 线虚拟 gpio_chip。方向和电平保存在驱动私有数据中，base 由 gpiolib 动态分配。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch21_gpio
make
sudo insmod virt_gpio.ko
sudo rmmod virt_gpio
```
