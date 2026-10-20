# ch52_alsa: ALSA 声卡

创建一张只有一个 PCM 设备的虚拟声卡。依赖 snd.ko 与 snd-pcm.ko。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch52_alsa
make
sudo insmod virt_alsa.ko
sudo rmmod virt_alsa
```
