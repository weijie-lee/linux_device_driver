# edu 的 MSI

同一块 `1234:11e8`。状态寄存器 `0x20` 的 bit7 让设备在阶乘算完后发中断。往 `0x08` 写 5，设备把 `5!` 写回 `0x08`，并通过 MSI 通知。处理函数读 `0x24`，把 `0x1` 写进 `0x64` 应答。MSI 也要做这一步，否则状态位清不掉。

不要和 `kernel/08` 的 `virt_mmio` 同时加载，两个驱动都匹配这一块卡。

## 代码知识点

`pci_alloc_irq_vectors(..., PCI_IRQ_MSI)` 拿到一个向量。QEMU 的 edu 默认走 MSI，不走 INTx。`/proc/interrupts` 里这一行的类型是 `PCI-MSI`，名字是 `virt_msi`。

函数跟踪：

```
virt_msi_compute <-run_store
handle_irq_event <-handle_edge_irq
virt_msi_irq <-__handle_irq_event_percpu
```

`handle_irq_event` 也会被别的中断碰到。判定只要求 `virt_msi_compute` 之后还能看到 `handle_irq_event` 和 `virt_msi_irq`。

算完后 sysfs 是 `fact=120 irqs=1`。`/proc/interrupts` 里这行的 CPU 计数之和比运行前大。

## 代码架构

```
module_init
  pci_alloc_irq_vectors(PCI_IRQ_MSI)
  request_irq(..., virt_msi_irq, "virt_msi")

echo 5 > run
  virt_msi_compute
    iowrite32(0x80, mmio + 0x20)        算完后发中断
    iowrite32(5, mmio + 0x08)
    等到状态位 COMPUTING 清掉
  MSI → handle_edge_irq → handle_irq_event → virt_msi_irq
    iowrite32(0x1, mmio + 0x64)         应答
  0x08 读回 120
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter kernel09
```

下面这份回显来自镜像里的一次实际运行。中断号每次不同。

```
======== kernel09_msi_irq ========
+ insmod /lib/modules/driver_test/kernel_09_msi_irq_virt_msi.ko
insmod ok
bound /sys/bus/pci/drivers/virt_msi/0000:00:03.0
interrupts before: 0
 25:          0          0   PCI-MSI 49152-edge      virt_msi
              sh-1       [001] .....     7.905676: virt_msi_compute <-run_store
          <idle>-0       [001] d.h..     7.905980: handle_irq_event <-handle_edge_irq
          <idle>-0       [001] d.h..     7.905988: virt_msi_irq <-__handle_irq_event_percpu
msi order ok
fact=120 irqs=1
interrupts after: 1
proc interrupts increased
+ rmmod virt_msi
rmmod ok
[PASS] kernel09_msi_irq
```

成功时客户机打印 `[PASS] kernel09_msi_irq`。`fact=120`，`irqs=1`，`/proc/interrupts` 的计数增加。
