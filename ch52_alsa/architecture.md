# ALSA 声卡

```
insmod virt_alsa.ko
  -> platform_device + platform_driver
  -> probe 注册到子系统
rmmod
  -> remove 注销
```

创建一张只有一个 PCM 设备的虚拟声卡。依赖 snd.ko 与 snd-pcm.ko。
