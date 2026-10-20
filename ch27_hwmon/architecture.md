# 硬件监控

```
insmod virt_hwmon.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

通过 hwmon 上报一个固定 25°C 的温度通道。
