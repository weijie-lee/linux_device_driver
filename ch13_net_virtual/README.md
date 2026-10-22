# 虚拟网卡 snull

## 代码知识点

网络设备是 `struct net_device`。`alloc_netdev()` 分配，`register_netdev()` 注册。发包走 `ndo_start_xmit`。本驱动做两块网卡 `sn0` 和 `sn1`，把从一块收到的 skb 交换到另一块，用来看协议栈而不需要网线。

`netif_start_queue` / `netif_stop_queue` 控制发送队列。私有数据跟在 `net_device` 后面，用 `netdev_priv()` 取出。

## 代码架构

```
alloc_netdev("sn%d") × 2
register_netdev
  ndo_open / ndo_stop / ndo_start_xmit
sn0 的包交给 sn1，反过来一样
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch13
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch13_net_virtual_snull.ko
insmod ok
+ ip link
1: lo: <LOOPBACK> mtu 65536 qdisc noop qlen 1000
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
2: sn0: <BROADCAST,MULTICAST,NOARP> mtu 1500 qdisc noop qlen 1000
    link/ether 00:00:00:00:00:00 brd ff:ff:ff:ff:ff:ff
3: sn1: <BROADCAST,MULTICAST,NOARP> mtu 1500 qdisc noop qlen 1000
    link/ether 00:00:00:00:00:00 brd ff:ff:ff:ff:ff:ff
+ rmmod snull
rmmod ok
[PASS] ch13_snull
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch13_snull`
- `ip link` 里同时有 `sn0` 和 `sn1`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch13
```
