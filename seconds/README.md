# seconds - 简单的秒计数设备驱动

## 知识点
这是一个简单的内核定时器演示驱动：
- 内核定时器使用方法
- 定时触发计数
- 读取当前计数值

## 编译运行
```bash
make seconds
cd seconds
make
sudo insmod seconds.ko
cat /dev/seconds
sudo rmmod seconds
```

## 参考
- 《Linux设备驱动开发详解》相关章节
