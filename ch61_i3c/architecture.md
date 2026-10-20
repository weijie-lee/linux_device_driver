# I3C 主机

```
insmod virt_i3c.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

登记一个虚拟 I3C master。bus_init 与 DAA 为空操作。依赖 i3c.ko。
