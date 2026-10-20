# ch47_mailbox: Mailbox

登记一个单通道 mailbox 控制器，send_data 立即成功。

本目录是不依赖真实硬件的虚拟驱动。模块自己创建 platform 设备并完成子系统注册，可在 QEMU（Linux 5.15）里 insmod / rmmod。

```bash
cd ch47_mailbox
make
sudo insmod virt_mbox.ko
sudo rmmod virt_mbox
```
