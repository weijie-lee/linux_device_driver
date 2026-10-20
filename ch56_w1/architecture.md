# 1-Wire

```
insmod virt_w1.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

登记一个虚拟 1-Wire 主机。reset 报告总线上没有从设备。依赖 wire.ko。
