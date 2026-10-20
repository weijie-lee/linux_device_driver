# 多功能设备

```
insmod virt_mfd.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

用 mfd_add_devices 挂出一个名为 virt-mfd-cell 的子 platform 设备。
