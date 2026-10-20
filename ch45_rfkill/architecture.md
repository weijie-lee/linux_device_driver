# RFKill

```
insmod virt_rfkill.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

登记一个 WLAN 类型的虚拟 rfkill 开关。
