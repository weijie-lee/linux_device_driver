# FPGA manager

```
insmod virt_fpga.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

登记一个始终处于 OPERATING 状态的 FPGA manager。依赖 fpga-mgr.ko。
