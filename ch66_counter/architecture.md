# Counter

```
insmod virt_counter.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

登记一个单信号、单计数器的 counter 设备，读数恒为 0。依赖 counter.ko。
