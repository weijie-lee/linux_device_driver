# Virtio 驱动

```
insmod virt_virtio.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

登记一个不匹配任何真实 virtio 设备 ID 的 virtio 驱动，演示驱动注册路径。
