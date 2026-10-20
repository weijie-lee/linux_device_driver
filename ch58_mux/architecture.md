# 多路复用器

```
insmod virt_mux.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

登记一个 4 状态的 mux 控制器。依赖 mux-core.ko。
