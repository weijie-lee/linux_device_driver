# ALSA

## 代码知识点

ALSA 声卡是 `snd_card`。`snd_card_new()` 创建，`snd_pcm_new()` 加一块 PCM，`snd_pcm_set_ops()` 填 open、hw_params、prepare、trigger。`snd_card_register()` 之后 `/proc/asound/cards` 能看到它。

硬件参数固定 48000 Hz、双声道、S16_LE。`trigger` 不推真实 DAC。

## 代码架构

```
snd_card_new(..., "virt")
snd_pcm_new("virt PCM")
snd_pcm_set_ops(playback 和 capture)
snd_card_register
/proc/asound/cards
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch52
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch52_alsa_virt_alsa.ko
insmod ok
+ lsmod | grep virt_alsa
virt_alsa              16384  0 
snd_pcm               147456  1 virt_alsa
snd                   106496  3 virt_alsa,snd_pcm,snd_timer
+ find /sys /dev /proc -name '*virt*' 2>/dev/null | head -n 30
/sys/kernel/tracing/events/xhci-hcd/xhci_free_virt_device
/sys/kernel/tracing/events/xhci-hcd/xhci_alloc_virt_device
/sys/kernel/tracing/events/xhci-hcd/xhci_setup_addressable_virt_device
/sys/kernel/debug/virtio-ports
/sys/kernel/debug/tracing/events/xhci-hcd/xhci_free_virt_device
/sys/kernel/debug/tracing/events/xhci-hcd/xhci_alloc_virt_device
/sys/kernel/debug/tracing/events/xhci-hcd/xhci_setup_addressable_virt_device
/sys/class/virtio-ports
/sys/devices/platform/virt_alsa
/sys/devices/virtual
/sys/devices/virtual/block/loop1/queue/virt_boundary_mask
/sys/devices/virtual/block/loop6/queue/virt_boundary_mask
/sys/devices/virtual/block/loop4/queue/virt_boundary_mask
/sys/devices/virtual/block/loop2/queue/virt_boundary_mask
/sys/devices/virtual/block/loop0/queue/virt_boundary_mask
/sys/devices/virtual/block/loop7/queue/virt_boundary_mask
/sys/devices/virtual/block/loop5/queue/virt_boundary_mask
/sys/devices/virtual/block/loop3/queue/virt_boundary_mask
/sys/bus/platform/devices/virt_alsa
/sys/bus/platform/drivers/virtio-mmio
/sys/bus/platform/drivers/virt_alsa
/sys/bus/platform/drivers/virt_alsa/virt_alsa
/sys/bus/pci/drivers/virtio-pci
/sys/bus/virtio
/sys/bus/virtio/drivers/virtio_console
/sys/bus/virtio/drivers/virtio_balloon
/sys/bus/virtio/drivers/virtio_iommu
/sys/bus/virtio/drivers/virtio_rproc_serial
/sys/module/virtio_mmio
/sys/module/virt_alsa
+ cat /proc/asound/cards
 0 [virt           ]: virt_alsa - virt
                      virtual sound card
+ rmmod virt_alsa
rmmod ok
[PASS] ch52_alsa
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch52_alsa`
- `/proc/asound/cards` 里有 `virt`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch52
```
