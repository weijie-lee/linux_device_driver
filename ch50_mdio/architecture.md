# MDIO 总线

```
insmod virt_mdio.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

登记一条虚拟 MDIO 总线。phy_mask 屏蔽全部地址，避免扫描不存在的 PHY。
