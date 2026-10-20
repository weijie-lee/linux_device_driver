# Devfreq

```
insmod virt_devfreq.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

用 performance governor 登记一个单频点的 devfreq 设备，不替换 CPU 频率驱动。
