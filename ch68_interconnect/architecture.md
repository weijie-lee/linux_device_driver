# 互联带宽

```
insmod virt_icc.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

向 interconnect 框架登记一个提供者和一个节点。
