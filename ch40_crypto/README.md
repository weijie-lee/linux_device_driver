# ch40_crypto: 加密算法

向 crypto API 注册 virt-null：按块把输入复制到输出，供 skcipher 路径使用。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch40_crypto
make
sudo insmod virt_crypto.ko
sudo rmmod virt_crypto
```
