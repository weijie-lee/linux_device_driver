# DRM

```
insmod virt_drm.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

登记一个只打开控制节点的 DRM 设备，不接管显示模式。依赖 drm.ko。
