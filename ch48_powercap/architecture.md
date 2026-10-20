# Powercap

```
insmod virt_powercap.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

向 powercap 框架登记控制类型 virt_pcap。
