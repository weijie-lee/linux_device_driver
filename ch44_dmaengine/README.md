# dmaengine

## 代码知识点

dmaengine 是通用 DMA 通道。控制器实现 `dma_device`，通道是 `virt_dma_chan`。`dma_async_device_register()` 之后，别的驱动 `dma_request_chan()`。提交描述符、`issue_pending` 开始搬移。

本引擎的 `issue_pending` 不碰硬件。它和 `ch12` 的 kmalloc、`learn/06` 的 DMA API 不是同一层。

## 代码架构

```
dma_device + virt_dma_chan
dma_async_device_register
issue_pending
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch44
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch44_dmaengine_virt_dmaeng.ko
insmod ok
+ lsmod | grep virt_dmaeng
virt_dmaeng            16384  0 
+ find /sys /dev /proc -name '*virt_dmaeng*' 2>/dev/null | head -n 30
/sys/kernel/debug/dmaengine/virt_dmaeng
/sys/devices/platform/virt_dmaeng
/sys/bus/platform/devices/virt_dmaeng
/sys/bus/platform/drivers/virt_dmaeng
/sys/bus/platform/drivers/virt_dmaeng/virt_dmaeng
/sys/module/virt_dmaeng
/sys/module/virt_dmaeng/drivers/platform:virt_dmaeng
+ rmmod virt_dmaeng
rmmod ok
[PASS] ch44_dmaengine
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch44_dmaengine`
- 能找到 `virt_dmaeng`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch44
```
