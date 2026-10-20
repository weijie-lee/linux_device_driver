# OPP 工作点

```
insmod virt_opp.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

给 platform 设备添加一个 1 GHz / 900 mV 的动态 OPP。CPU 频率驱动保持不动。
