# 引脚控制器

```
insmod virt_pinctrl.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

登记 4 个引脚和一个引脚组，覆盖 pinctrl 子系统的控制器注册路径。
