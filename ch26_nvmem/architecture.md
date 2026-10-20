# NVMEM

```
insmod virt_nvmem.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

用 256 字节 RAM 模拟 EEPROM，挂到 nvmem 子系统。
