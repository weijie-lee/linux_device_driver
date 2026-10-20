# ch58_mux: 多路复用器

登记一个 4 状态的 mux 控制器。依赖 mux-core.ko。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch58_mux
make
sudo insmod virt_mux.ko
sudo rmmod virt_mux
```
