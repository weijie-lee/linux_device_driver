# LED 类设备

```
insmod virt_led.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

注册 LED class 设备 virt:status，亮度由驱动保存。
