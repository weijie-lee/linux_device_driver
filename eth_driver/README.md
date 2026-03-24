# eth_driver — 虚拟以太网 MAC + PHY 驱动示例

本模块实现一个完整的 Linux 以太网网络设备驱动，注册虚拟网卡 `veth0_mac`，内置软件模拟的 PHY 状态机和 NAPI 接收框架。发送的数据帧通过 loopback 立即回注到接收路径，无需任何网络硬件即可验证完整的 MAC 驱动栈。

## 知识点

- ✅ `alloc_etherdev()` / `register_netdev()` — 网络设备的分配与注册
- ✅ `struct net_device_ops` — 实现 open/stop/start_xmit/get_stats64/tx_timeout
- ✅ `sk_buff` 生命周期 — alloc、clone、enqueue、netif_receive_skb、kfree
- ✅ **NAPI** — `napi_add` / `napi_schedule` / `napi_complete_done` 轮询接收框架
- ✅ 虚拟 PHY 状态机 — 用内核定时器模拟链路协商（link up/down）
- ✅ `ethtool_ops` — 向 ethtool 报告链路速率和双工模式
- ✅ `u64_stats_sync` — 无锁 per-CPU 统计计数器
- ✅ `netif_carrier_on/off` — 通知内核链路状态变化

## 架构图

```
用户空间 (ping / ip link)
        │
        ▼
  Linux 网络栈 (TCP/IP)
        │
        ▼
  virt_mac_start_xmit()         ← TX 路径
  ├── 更新 TX 统计
  ├── skb_copy() → rx_queue     ← loopback：TX 帧入 RX 队列
  └── napi_schedule()
        │
        ▼
  virt_mac_poll()               ← NAPI RX 路径
  ├── skb_dequeue(rx_queue)
  ├── eth_type_trans()          ← 识别以太网帧类型
  └── netif_receive_skb()       ← 上交网络栈

  phy_timer_fn()                ← 虚拟 PHY（定时器）
  └── netif_carrier_on()        ← 500ms 后模拟链路 UP
```

## 代码结构

### 核心数据结构

```c
struct virt_mac_priv {
    struct net_device  *dev;
    struct napi_struct  napi;       // NAPI 接收实例
    struct timer_list   phy_timer;  // 虚拟 PHY 定时器
    bool                link_up;    // PHY 链路状态
    int                 speed;      // 链路速率（Mbps）
    spinlock_t          rx_lock;
    struct sk_buff_head rx_queue;   // loopback RX 队列
    struct virt_stats   stats;      // u64 统计计数器
};
```

### 关键函数说明

| 函数 | 作用 |
|------|------|
| `virt_mac_open()` | 启动发送队列、使能 NAPI、启动 PHY 定时器 |
| `virt_mac_stop()` | 停止队列、禁用 NAPI、删除 PHY 定时器 |
| `virt_mac_start_xmit()` | TX 核心：clone skb 入 RX 队列，触发 NAPI |
| `virt_mac_poll()` | NAPI 轮询：从 rx_queue 取帧，调用 `netif_receive_skb` |
| `phy_timer_fn()` | 模拟 PHY 链路协商，500ms 后触发 carrier on |
| `virt_mac_get_stats64()` | 使用 `u64_stats_sync` 安全读取统计数据 |

## 编译

```bash
cd eth_driver
make
```

## 加载与验证

```bash
# 1. 加载模块
sudo insmod eth_mac.ko

# 2. 查看注册的网络设备
dmesg | grep virt_mac
# 预期: virt_mac: registered network device 'veth0_mac' (MAC xx:xx:xx:xx:xx:xx)

ip link show veth0_mac
# 预期: veth0_mac: <BROADCAST,MULTICAST,LOOPBACK> mtu 1500 ...
```

## 验证实验

### 实验一：启动接口并观察 PHY 链路 UP

```bash
# 启动接口
sudo ip link set veth0_mac up

# 等待约 500ms，观察 PHY 链路 UP 事件
dmesg | grep "PHY link"
# 预期: PHY link UP: 1000 Mbps full-duplex

# 验证 carrier 状态
cat /sys/class/net/veth0_mac/carrier
# 预期: 1

# 查看 ethtool 报告的链路信息
sudo ethtool veth0_mac
# 预期: Speed: 1000Mb/s, Duplex: Full, Link detected: yes
```

### 实验二：配置 IP 并 ping 自身（loopback 验证）

```bash
# 配置 IP 地址
sudo ip addr add 192.168.99.1/24 dev veth0_mac

# ping 自身（每个 ICMP 包经过完整 TX→loopback→RX 路径）
ping -c 4 192.168.99.1 -I veth0_mac
# 预期: 4 packets transmitted, 4 received, 0% packet loss
```

### 实验三：观察数据包统计

```bash
# 查看 TX/RX 统计（每次 ping 应同时增加 TX 和 RX 计数）
ip -s link show veth0_mac
# 预期:
#   RX: bytes  packets  errors  dropped
#        xxxx        4       0        0
#   TX: bytes  packets  errors  dropped
#        xxxx        4       0        0

# 或使用 ethtool 查看统计
sudo ethtool -S veth0_mac
```

### 实验四：抓包验证帧结构

```bash
# 在另一个终端启动 tcpdump
sudo tcpdump -i veth0_mac -n -e

# 发送数据
ping -c 2 192.168.99.1 -I veth0_mac

# 预期 tcpdump 输出（每个 ICMP 包出现两次：TX 和 RX loopback）:
# xx:xx:xx > xx:xx:xx, ethertype IPv4, length 98: 192.168.99.1 > 192.168.99.1: ICMP echo request
# xx:xx:xx > xx:xx:xx, ethertype IPv4, length 98: 192.168.99.1 > 192.168.99.1: ICMP echo reply
```

### 实验五：模拟链路 DOWN

```bash
# 关闭接口（触发 PHY 定时器停止，carrier off）
sudo ip link set veth0_mac down

dmesg | tail -3
# 预期: virt_mac: interface stopped

cat /sys/class/net/veth0_mac/carrier
# 预期: 0
```

### 实验六：卸载模块

```bash
sudo rmmod eth_mac
dmesg | tail -2
# 预期: virt_mac: module unloaded

ip link show veth0_mac 2>&1
# 预期: Device "veth0_mac" does not exist
```

## NAPI 工作原理

```
中断/定时器触发
      │
      ▼
napi_schedule()        ← 将 NAPI 实例加入轮询队列
      │
      ▼
virt_mac_poll()        ← 内核调用，budget=64
  ├── 处理最多 budget 个帧
  ├── 若 work_done < budget → napi_complete_done() 退出轮询模式
  └── 若 work_done == budget → 保持轮询，下次继续
```

NAPI 的核心优势：高流量时关闭中断，改用轮询，避免"中断风暴"导致 CPU 被打满。

## MAC 驱动核心要点

| 要点 | 说明 |
|------|------|
| **alloc_etherdev** | 分配 net_device + 私有数据，`netdev_priv()` 获取私有指针 |
| **sk_buff 所有权** | `start_xmit` 必须消费 skb（`dev_kfree_skb` 或上交网络栈），不能泄漏 |
| **NETDEV_TX_OK** | `start_xmit` 返回此值表示帧已被驱动接管；`NETDEV_TX_BUSY` 表示队列满 |
| **netif_carrier** | 必须正确维护 carrier 状态，否则内核不会向此接口路由数据包 |
| **u64_stats_sync** | 64 位统计在 32 位系统上非原子，必须用此机制保护 |
| **eth_type_trans** | 从以太网头部解析 `skb->protocol`，必须在 `netif_receive_skb` 之前调用 |

## 参考

- 《Linux设备驱动开发详解》第17章（网络设备驱动）
- `Documentation/networking/netdevices.rst`
- `Documentation/networking/napi.rst`
- `drivers/net/dummy.c`（内核自带虚拟网卡，最简实现参考）
- `drivers/net/loopback.c`（内核 loopback 驱动参考）
