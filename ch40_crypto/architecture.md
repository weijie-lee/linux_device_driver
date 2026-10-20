# 加密算法

```
insmod virt_crypto.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

向 crypto API 注册 virt-null：按块把输入复制到输出，供 skcipher 路径使用。
