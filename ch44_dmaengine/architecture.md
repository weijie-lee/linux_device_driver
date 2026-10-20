# DMA 引擎

```
insmod virt_dmaeng.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

向 dmaengine 注册一个支持 memcpy 能力位的虚拟通道。prep 返回 NULL，表示当前无法排队。
