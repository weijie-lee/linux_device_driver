# CAN

```
insmod virt_can.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

用 alloc_candev 登记一个虚拟 CAN 控制器。依赖 can.ko 与 can-dev.ko。
