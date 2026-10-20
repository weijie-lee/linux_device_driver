# GNSS

```
insmod virt_gnss.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

登记一个 NMEA 类型的虚拟 GNSS 接收机。依赖 gnss.ko。
