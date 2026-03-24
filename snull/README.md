# snull - 简单网络设备驱动示例

## 知识点
这是来自LDD3（《Linux Device Drivers Development》）的经典示例，实现了一个简单的虚拟网络设备：
- 创建两个虚拟网络接口 `sn0` 和 `sn1`
- 从一个接口发送的数据包会被 loopback 到另一个接口
- 可以用来测试网络栈和驱动开发

## 编译运行
```bash
make snull
cd snull
make
sudo insmod snull.ko
# 查看加载信息
dmesg | tail
# 配置IP地址
sudo ip addr add 192.168.10.1/24 dev sn0
sudo ip addr add 192.168.10.2/24 dev sn1
sudo ip link set sn0 up
sudo ip link set sn1 up
# ping 测试互访
ping 192.168.10.2 from sn0
```

## 关键点
- 网络设备驱动的基本框架
- NAPI 支持
- 数据包收发处理
- 硬件地址（MAC）设置

## 参考
- 《Linux Device Drivers Development》第17章
- 《Linux设备驱动开发详解》相关章节
