# edu 的 MMIO

QEMU 命令行上有 `-device edu`，PCI ID `1234:11e8`。BAR0 是 1MB 的 MMIO。识别寄存器 `0x00` 读出来是 `0x010000ed`。往 `0x04` 写一个数，读回来是按位取反：写入 `0x12345678`，读到 `edcba987`。

`System.map` 里有 `ioread32`，这台内核的 `set_ftrace_filter` 不接受它，和 `kernel/03` 里的 `_copy_from_user` 一样。跟踪停在驱动自己的 `noinline` 函数上。数值对得上，说明读和写确实进了这块 BAR。

## 代码知识点

模块加载时设备已经在总线上，`pci_register_driver()` 立刻 probe。要跟踪 probe，先加载模块（函数进入过滤表），再把设备 `remove` 掉，打开跟踪，然后 `rescan`。第二次 probe 的顺序是：

```
local_pci_probe <-pci_device_probe
virt_mmio_probe <-local_pci_probe
virt_mmio_read_id <-virt_mmio_probe
```

写活性和读活性走 sysfs：

```
dev_attr_store <-sysfs_kf_write
liveness_store <-dev_attr_store
dev_attr_show <-sysfs_kf_seq_show
liveness_show <-dev_attr_show
```

`liveness_store` 里是 `iowrite32()`，`liveness_show` 里是 `ioread32()`。这两个名字不进函数跟踪。

这张卡和 `ch19` 的 `1234:5678` / `1234:5679` 不是同一个 ID，两边的驱动不会抢设备。`kernel/09` 也匹配 `1234:11e8`，不要和本模块同时加载。

## 代码架构

```
-device edu                         1234:11e8，BAR0

module_init
  pci_register_driver
    local_pci_probe → virt_mmio_probe
      pci_iomap(BAR0)
      virt_mmio_read_id               ioread32(mmio + 0x00) == 0x010000ed

echo 0x12345678 > liveness
  sysfs_kf_write → dev_attr_store → liveness_store
    iowrite32(0x12345678, mmio + 0x04)

cat liveness
  sysfs_kf_seq_show → dev_attr_show → liveness_show
    ioread32(mmio + 0x04) == 0xedcba987
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter kernel08
```

下面这份回显来自镜像里的一次实际运行。PCI 地址每次可能不同。

```
======== kernel08_pci_mmio ========
+ insmod /lib/modules/driver_test/kernel_08_pci_mmio_virt_mmio.ko
insmod ok
bound /sys/bus/pci/drivers/virt_mmio/0000:00:03.0
+ cat /sys/bus/pci/drivers/virt_mmio/0000:00:03.0/ident
010000ed
              sh-1       [001] .....     8.067633: local_pci_probe <-pci_device_probe
              sh-1       [001] .....     8.067898: virt_mmio_probe <-local_pci_probe
              sh-1       [001] .....     8.070581: virt_mmio_read_id <-virt_mmio_probe
probe-hits: local_pci_probe driver_probe read_id
probe order ok
              sh-1       [001] .....     8.740521: dev_attr_store <-sysfs_kf_write
              sh-1       [001] .....     8.740547: liveness_store <-dev_attr_store
             cat-164     [000] .....     8.745472: dev_attr_show <-sysfs_kf_seq_show
             cat-164     [000] .....     8.745490: liveness_show <-dev_attr_show
mmio-hits: dev_attr_store store dev_attr_show show
mmio order ok
edcba987
+ rmmod virt_mmio
rmmod ok
[PASS] kernel08_pci_mmio
```

成功时客户机打印 `[PASS] kernel08_pci_mmio`。`ident` 是 `010000ed`，`liveness` 在写入 `0x12345678` 之后是 `edcba987`。
