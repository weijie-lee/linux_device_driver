# MAC 和 PHY

## 代码知识点

以太网驱动通常分成 MAC 和 PHY。MAC 负责 `net_device` 和收发，PHY 负责链路速率和双工，中间用 MDIO。本模块把两步收成一块虚拟网卡 `veth0_mac`：随机本地 MAC、`IFF_NOARP`、`NETIF_F_LOOPBACK`，并注册 NAPI。

`ethtool_ops` 提供驱动名和链路信息。打开接口时 `netif_napi_add` 已经在注册前做完。

## 代码架构

```
alloc_etherdev
  name = veth0_mac
  netdev_ops + ethtool_ops
  netif_napi_add
register_netdev
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch14
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch14_net_mac_phy_eth_mac.ko
insmod ok
+ ip link
1: lo: <LOOPBACK> mtu 65536 qdisc noop qlen 1000
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
4: veth0_mac: <BROADCAST,MULTICAST,NOARP> mtu 1500 qdisc noop qlen 1000
    link/ether c6:a2:56:ab:b4:9a brd ff:ff:ff:ff:ff:ff
+ rmmod eth_mac
rmmod ok
[PASS] ch14_eth_mac
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch14_eth_mac`
- `ip link` 里有 `veth0_mac`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch14
```
