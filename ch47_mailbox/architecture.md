# Mailbox

```
insmod virt_mbox.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

登记一个单通道 mailbox 控制器，send_data 立即成功。
