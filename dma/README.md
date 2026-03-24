# dma - DMA（直接内存访问）演示

## 知识点
DMA相关API使用演示：
- DMA映射
- DMA地址转换
- 直接内存访问示例

## 编译运行
```bash
make dma
cd dma
make
sudo insmod dma_demo.ko
dmesg | tail
sudo rmmod dma_demo
```

## 参考
- 《Linux设备驱动开发详解》相关章节
