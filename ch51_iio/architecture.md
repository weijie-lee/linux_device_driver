# IIO 传感器

```
insmod virt_iio.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

登记一个直接模式的 IIO 电压通道，raw 读数固定为 1234。依赖 industrialio.ko。
