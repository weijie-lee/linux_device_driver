# 电源供应

```
insmod virt_psy.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

注册虚拟电池 virt-battery，报告容量、电压和在位状态。
