# NFC

```
insmod virt_nfc.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

登记一个支持 NFC-DEP 的虚拟 NFC 控制器。依赖 nfc.ko。
