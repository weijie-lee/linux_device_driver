# 一致性内存和流式 DMA

`ch12_dma` 的代码只演示 `kmalloc` / `vmalloc`。DMA 映射在这里。

设备是一个平台设备，probe 里设置 64 位 `dma_mask`，然后做三次映射并立刻解除：

| API | 内存从哪来 | 同步 |
| --- | --- | --- |
| `dma_alloc_coherent` | DMA 层分配，CPU 和设备同时可见 | 不需要 `dma_sync_*` |
| `dma_map_single` | `kmalloc` | 交给设备前 `dma_sync_single_for_device`，CPU 再读之前 `dma_sync_single_for_cpu`，最后 `dma_unmap_single` |
| `dma_map_sg` | `alloc_page` 放进 `scatterlist` | `dma_unmap_sg` |

成功后 `/sys/devices/platform/virt_dma/report` 为 `coherent:ok stream:ok sg:ok`。映射本身不留到 remove，避免卸载时还握着总线地址。

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter learn06
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/learn_06_dma_virt_dma.ko
insmod ok
+ cat /sys/devices/platform/virt_dma/report
coherent:ok stream:ok sg:ok
+ rmmod virt_dma
rmmod ok
[PASS] learn06_dma
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] learn06_dma`
- `report` 同时包含 `coherent:ok`、`stream:ok`、`sg:ok`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter learn06
```
