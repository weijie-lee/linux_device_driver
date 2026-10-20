# ch28_leds: LED 类设备

注册 LED class 设备 virt:status，亮度由驱动保存。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch28_leds
make
sudo insmod virt_led.ko
sudo rmmod virt_led
```
