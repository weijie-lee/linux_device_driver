# SPMI 总线

```
insmod virt_spmi.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

登记一个虚拟 SPMI 控制器，读写命令返回 0。依赖 spmi.ko。
