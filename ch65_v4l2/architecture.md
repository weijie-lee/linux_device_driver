# V4L2 视频

```
insmod virt_v4l2.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

登记一个只实现 querycap 的视频采集节点。依赖 videodev.ko 与 mc.ko。
