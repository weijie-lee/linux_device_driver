# 通用 PHY

```
insmod virt_phy.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

用 generic PHY 框架登记一条虚拟 PHY（init/power_on/power_off）。
