# 硬件自旋锁

```
insmod virt_hwspin.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

登记 1 把 id 从 32 起的虚拟 hwspinlock。trylock 总是成功。
