# kmalloc 和 vmalloc

## 代码知识点

`kmalloc` 来自直接映射的物理页，`__pa()` / `__va()` 可以互相换算。`vmalloc` 是虚拟地址连续、物理页可以不连续，对它用 `__pa()` 没有「一整块物理地址」的含义。模块在 `init` 里各分配一次，打印地址，然后立刻释放。

真正的 `dma_alloc_coherent`、`dma_map_single` 和 scatterlist 在 `learn/06_dma`。

## 代码架构

```
init
  kmalloc → __pa → __va → kfree
  vmalloc → vfree
没有 dma_map，没有设备节点
```

DMA 映射要看 [learn/06_dma](../learn/06_dma)。

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch12
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch12_dma_dma_demo.ko
insmod ok
+ dmesg | grep kmalloc
[    4.729170] kmalloc virtual addr:0x(____ptrval____), pa:0x13291c0
[    4.729353] kmalloc virtual addr:0x(____ptrval____), pa:0x13291c0
+ rmmod dma_demo
rmmod ok
[PASS] ch12_dma
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch12_dma`
- dmesg 里有 `kmalloc virtual addr`。这一章不调用 DMA API。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch12
```
