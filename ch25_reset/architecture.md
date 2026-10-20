# 复位控制器

```
insmod virt_reset.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

注册 4 路虚拟 reset line，支持 assert、deassert 和 reset。
