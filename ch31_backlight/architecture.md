# 背光

```
insmod virt_backlight.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

注册 backlight class 设备，亮度范围 0–100。
