# PPS 秒脉冲

```
insmod virt_pps.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

向 PPS 子系统登记一个可捕获 assert 事件的虚拟源。
