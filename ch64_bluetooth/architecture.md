# 蓝牙 HCI

```
insmod virt_bt.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

登记一个 HCI_VIRTUAL 控制器。发送路径释放 skb。依赖 bluetooth.ko。
