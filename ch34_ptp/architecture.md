# PTP 时钟

```
insmod virt_ptp.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

登记一个读系统实时钟的虚拟 PTP hardware clock。
