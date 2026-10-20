# UIO

```
insmod virt_uio.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

登记一个没有中断、没有内存映射的 UIO 设备，使用户态可以看到 /dev/uioN。依赖 uio.ko。
