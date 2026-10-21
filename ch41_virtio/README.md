# virtio 驱动

## 代码知识点

virtio 驱动是 `struct virtio_driver`。`id_table` 给出设备号，`register_virtio_driver()` 挂到 virtio 总线。匹配的 virtio 设备出现时才 `probe`。传输（PCI、MMIO）由 virtio 核心处理，驱动只谈 virtqueue。

本表匹配设备号 `0xffff`。QEMU 命令行没有加这种 virtio 设备时，`probe` 不运行。实验确认驱动已注册。

## 代码架构

```
virtio_device_id { 0xffff, VIRTIO_DEV_ANY_ID }
register_virtio_driver
  有匹配设备才 probe
/sys/bus/virtio/drivers/virt_virtio
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch41
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch41_virtio_virt_virtio.ko
insmod ok
+ lsmod | grep virt_virtio
virt_virtio            16384  0 
+ find /sys /dev /proc -name '*virt_virtio*' 2>/dev/null | head -n 30
/sys/bus/virtio/drivers/virt_virtio
/sys/module/virt_virtio
/sys/module/virt_virtio/drivers/virtio:virt_virtio
+ rmmod virt_virtio
rmmod ok
[PASS] ch41_virtio
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch41_virtio`
- 能找到 `virt_virtio` 驱动。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch41
```
