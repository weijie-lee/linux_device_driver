# Remoteproc

```
insmod virt_rproc.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

登记一个不会自动启动的虚拟 remote processor，start/stop 为空操作。
