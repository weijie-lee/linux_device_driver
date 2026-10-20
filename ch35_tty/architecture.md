# TTY / 串口

```
insmod virt_tty.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

用 tty 层登记一个名为 ttyVIRT 的虚拟串口。write 丢弃数据并返回成功，对应 Linux tty/serial 驱动模型。
