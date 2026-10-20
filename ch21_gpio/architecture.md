# GPIO 控制器

```
insmod virt_gpio.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

注册 8 线虚拟 gpio_chip。方向和电平保存在驱动私有数据中，base 由 gpiolib 动态分配。
