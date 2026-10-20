# 稳压器

```
insmod virt_regulator.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

注册一颗固定 1.8 V 的 regulator，支持 enable/disable。
