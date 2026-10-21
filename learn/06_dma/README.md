# 一致性内存和流式 DMA

`ch12_dma` 的代码只演示 `kmalloc` / `vmalloc`。DMA 映射在这里。

设备是一个平台设备，probe 里设置 64 位 `dma_mask`，然后做三次映射并立刻解除：

| API | 内存从哪来 | 同步 |
| --- | --- | --- |
| `dma_alloc_coherent` | DMA 层分配，CPU 和设备同时可见 | 不需要 `dma_sync_*` |
| `dma_map_single` | `kmalloc` | 交给设备前 `dma_sync_single_for_device`，CPU 再读之前 `dma_sync_single_for_cpu`，最后 `dma_unmap_single` |
| `dma_map_sg` | `alloc_page` 放进 `scatterlist` | `dma_unmap_sg` |

成功后 `/sys/devices/platform/virt_dma/report` 为 `coherent:ok stream:ok sg:ok`。映射本身不留到 remove，避免卸载时还握着总线地址。
