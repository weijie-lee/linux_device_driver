# 帧缓冲

```
insmod virt_fb.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

分配一块 320x240 RGB565 的系统内存，注册为 framebuffer。
