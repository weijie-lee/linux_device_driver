# 外部连接器

```
insmod virt_extcon.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

登记 extcon 设备并报告 USB 电缆已连接。
