# MTD

```
insmod virt_mtd.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

用 64 KiB RAM 模拟 MTD_RAM 设备并注册到 MTD 子系统。依赖 mtd.ko。
