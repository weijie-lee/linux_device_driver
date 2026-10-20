# 时钟框架

```
insmod virt_clk.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

向 common clock framework 注册一路 24 MHz 固定时钟。
