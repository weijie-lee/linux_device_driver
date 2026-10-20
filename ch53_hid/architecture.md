# HID

```
insmod virt_hid.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

构造一个带键盘报告描述符的虚拟 HID 设备并交给 hid 核心解析。依赖 hid.ko。
