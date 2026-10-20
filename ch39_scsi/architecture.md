# SCSI 主机

```
insmod virt_scsi.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

登记一个虚拟 SCSI host。扫描到的命令以 DID_NO_CONNECT 完成，不访问磁盘。
