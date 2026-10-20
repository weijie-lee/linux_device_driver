# 热区

```
insmod virt_thermal.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

注册名为 virt_thermal 的热区，读数固定 30°C，不启动轮询。
