/*
 * snull.c — 简单虚拟网络驱动（Simple Network Utility）
 *
 * Copyright (C) 2020.3.24 liweijie<ee.liweijie@gmail.com>
 * 派生自《Linux Device Drivers》（Rubini & Corbet）
 *
 * 【驱动架构概述】
 * 本驱动创建两个虚拟以太网接口 sn0/sn1，它们可以互相 ping。
 *
 * 数据包流转路径：
 *   sn0 发送包 → snull_tx() → snull_hw_tx()
 *                              │
 *                    修改 IP 地址第三字节（子网切换）
 *                    修改目标 MAC 的 LSB（地址切换）
 *                    重算 IP 校验和
 *                              │
 *                    模拟 RX 中断 → sn1 的 snull_rx()
 *                              │
 *                    netif_rx() → 内核网络层处理
 *
 * 【关键技术点】
 * 1. net_device 注册：alloc_netdev + register_netdev
 * 2. 发送路径：实现 ndo_start_xmit，内部模拟硬件发送和中断
 * 3. 接收路径：分配 skb，调用 netif_rx() 上送内核网络层
 * 4. 包缓冲池：手动管理 snull_packet 链表，模拟硬件 DMA 缓冲区
 * 5. 自旋锁：中断上下文中使用 spin_lock_irqsave/irqrestore
 *
 * 【整改记录】
 *   - 删除 snull_header/tx/hw_tx/rx 中约 20 处冗余调试 printk
 *   - 修复 init 中错误处理始终返回 0 的问题
 *   - 替换裸 printk 为 pr_* 完整宏
 */

#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/etherdevice.h>
#include <linux/tcp.h>

#define SNULL_RX_INTR 0x0001
#define SNULL_TX_INTR 0x0002

inint pool_size = 8;			/* 每个设备的包缓冲池大小，可通过 modprobe pool_size=N 调整 */
module_param(pool_size, int, 0);
void snull_module_exit(void);

/*
 * snull_interrupt — 函数指针，指向当前使用的中断处理函数。
 * 本驱动使用 snull_regular_interrupt（模拟普通硬件中断）。
 */
static void (*snull_interrupt)(int, void *, struct pt_regs *);

struct net_device *snull_devs[2];	/* 两个虚拟网络设备：sn0 和 sn1 */

/*
 * struct snull_packet — 模拟硬件 DMA 缓冲区的包结构体
 *
 * 真实网卡驱动中，硬件有固定的 DMA 缓冲区用于存放包数据。
 * 本驱动用 kmalloc 分配的内存模拟这些缓冲区。
 *
 *   next    — 单链表指针，用于组成包缓冲池（ppool）和接收队列（rx_queue）
 *   dev     — 该包属于哪个网络设备（用于 snull_release_buffer 中返回缓冲区）
 *   datalen — 包数据的实际长度（字节）
 *   data    — 包数据缓冲区，最大 ETH_DATA_LEN（1500）字节
 */
struct snull_packet {
	struct snull_packet *next;
	struct net_device *dev;
	int datalen;
	u8 data[ETH_DATA_LEN];
};

/*
 * struct snull_priv — 虚拟网络设备的私有数据结构体
 *
 * 通过 netdev_priv(dev) 获取，存储在 net_device 结构体后面的私有区域。
 *
 *   dev          — 指向对应的 net_device（反向引用）
 *   skb          — 当前正在发送的 skb，等待 TX 完成中断后释放
 *   ppool        — 包缓冲池链表头，预分配 pool_size 个 snull_packet
 *   rx_queue     — 模拟硬件接收队列，中断处理函数从此取包上送
 *   lock         — 自旋锁，保护 ppool/rx_queue/status 等字段
 *   status       — 模拟硬件状态寄存器：RX_INTR | TX_INTR
 *   rx_int_enabled — 是否启用模拟 RX 中断
 *   tx_packetlen — 当前发送包的长度（用于统计）
 *   tx_packetdata — 当前发送包的数据指针
 *   stats        — 网络设备统计信息（rx/tx 包数、字节数、丢包数等）
 *   napi         — NAPI 轮询接收结构体（保留为扩展预留）
 */
struct snull_priv {
	struct net_device *dev;
	struct sk_buff *skb;
	struct snull_packet *ppool;
	struct snull_packet *rx_queue;
	spinlock_t lock;
	int status;
	int rx_int_enabled;
	int tx_packetlen;
	u8 *tx_packetdata;
	struct net_device_stats stats;
	struct napi_struct napi;
};

static struct snull_packet *snull_get_tx_buffer(struct net_device *dev)
{
	struct snull_priv *priv = netdev_priv(dev);
	unsigned long flags;
	struct snull_packet *pkt;

	spin_lock_irqsave(&priv->lock, flags);
	pkt = priv->ppool;
	priv->ppool = pkt->next;
	if (priv->ppool == NULL) {
		pr_info("snull: packet pool empty, stopping tx queue on %s\n",
			dev->name);
		netif_stop_queue(dev);
	}
	spin_unlock_irqrestore(&priv->lock, flags);
	return pkt;
}

static void snull_release_buffer(struct snull_packet *pkt)
{
	unsigned long flags;
	struct snull_priv *priv = netdev_priv(pkt->dev);

	spin_lock_irqsave(&priv->lock, flags);
	pkt->next = priv->ppool;
	priv->ppool = pkt;
	spin_unlock_irqrestore(&priv->lock, flags);

	if (netif_queue_stopped(pkt->dev) && pkt->next == NULL)
		netif_wake_queue(pkt->dev);
}

static void snull_setup_pool(struct net_device *dev)
{
	struct snull_priv *priv = netdev_priv(dev);
	int i;
	struct snull_packet *pkt;

	priv->ppool = NULL;
	for (i = 0; i < pool_size; i++) {
		pkt = kmalloc(sizeof(struct snull_packet), GFP_KERNEL);
		if (!pkt) {
			pr_notice("snull: ran out of memory allocating packet pool\n");
			return;
		}
		pkt->dev = dev;
		pkt->next = priv->ppool;
		priv->ppool = pkt;
	}
}

static void snull_teardown_pool(struct net_device *dev)
{
	struct snull_priv *priv = netdev_priv(dev);
	struct snull_packet *pkt;

	while ((pkt = priv->ppool)) {
		priv->ppool = pkt->next;
		kfree(pkt);
	}
}

static void snull_enqueue_buf(struct net_device *dev, struct snull_packet *pkt)
{
	unsigned long flags;
	struct snull_priv *priv = netdev_priv(dev);

	spin_lock_irqsave(&priv->lock, flags);
	pkt->next = priv->rx_queue;
	priv->rx_queue = pkt;
	spin_unlock_irqrestore(&priv->lock, flags);
}

static struct snull_packet *snull_dequeue_buf(struct net_device *dev)
{
	struct snull_priv *priv = netdev_priv(dev);
	struct snull_packet *pkt;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);
	pkt = priv->rx_queue;
	if (pkt)
		priv->rx_queue = pkt->next;
	spin_unlock_irqrestore(&priv->lock, flags);
	return pkt;
}

static void snull_rx_ints(struct net_device *dev, int enable)
{
	struct snull_priv *priv = netdev_priv(dev);
	priv->rx_int_enabled = enable;
}

/*
 * snull_open — 启动网络设备（ip link set sn0 up 时调用）
 *
 * 分配虚拟 MAC 地址：sn0 使用 \0SNUL0，sn1 使用 \0SNUL1。
 * MAC 第一字节为 0 表示单播地址（最低位为 0）。
 * netif_start_queue() 通知内核可以开始向此设备发送包。
 */
static int snull_open(struct net_device *dev)
{
	/* 分配虚拟 MAC 地址：sn0 为 \0SNUL0，sn1 为 \0SNUL1 */
	if (dev == snull_devs[0])
		memcpy(dev->dev_addr, "\0SNUL0", ETH_ALEN);
	else
		memcpy(dev->dev_addr, "\0SNUL1", ETH_ALEN);

	netif_start_queue(dev);	/* 开启 TX 队列，内核可以发包 */
	pr_info("snull: %s opened\n", dev->name);
	return 0;
}

static int snull_release(struct net_device *dev)
{
	netif_stop_queue(dev);
	pr_info("snull: %s closed\n", dev->name);
	return 0;
}

/*
 * snull_rx — 将接收到的包上送网络层
 *
 * 【接收路径详解】
 * 1. dev_alloc_skb() 分配 skb，额外 +2 字节用于 IP 头对齐
 * 2. skb_reserve(skb, 2) 将数据指针向后移 2 字节
 *    原因：以太网头 14 字节 + 2 = 16，使 IP 头对齐到 16 字节边界
 * 3. skb_put() 将 skb 的 tail 指针向后移，返回数据写入地址
 * 4. eth_type_trans() 解析以太网头，设置协议类型（ETH_P_IP 等）
 * 5. CHECKSUM_UNNECESSARY 告知内核无需验证校验和
 * 6. netif_rx() 将 skb 交给内核网络层处理
 */
static void snull_rx(struct net_device *dev, struct snull_packet *pkt)
{
	struct sk_buff *skb;
	struct snull_priv *priv = netdev_priv(dev);

	/*
	 * Allocate +2 bytes so that skb_reserve(skb, 2) can align the IP
	 * header to a 16-byte boundary (Ethernet header is 14 bytes, so
	 * adding 2 bytes of padding puts the IP header at offset 16).
	 */
	skb = dev_alloc_skb(pkt->datalen + 2);
	if (!skb) {
		if (printk_ratelimit())
			pr_notice("snull: %s low on memory, packet dropped\n",
				  dev->name);
		priv->stats.rx_dropped++;
		return;
	}

	skb_reserve(skb, 2);
	memcpy(skb_put(skb, pkt->datalen), pkt->data, pkt->datalen);

	skb->dev = dev;
	skb->protocol = eth_type_trans(skb, dev);
	skb->ip_summed = CHECKSUM_UNNECESSARY;

	priv->stats.rx_packets++;
	priv->stats.rx_bytes += pkt->datalen;
	netif_rx(skb);
}

/*
 * snull_regular_interrupt — 模拟硬件中断处理函数
 *
 * 真实网卡驱动中，此函数由硬件中断触发。
 * 本驱动中由 snull_hw_tx() 直接调用，模拟硬件中断。
 *
 * 处理两种中断事件：
 *   SNULL_RX_INTR — 有包到达：从 rx_queue 取包并调用 snull_rx() 上送
 *   SNULL_TX_INTR — 发送完成：更新统计并释放 skb
 *
 * 注意：snull_release_buffer() 在自旋锁外调用，避免锁序问题。
 */
static void snull_regular_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	int statusword;
	struct snull_priv *priv;
	struct snull_packet *pkt = NULL;
	struct net_device *dev = (struct net_device *)dev_id;

	if (!dev)
		return;

	priv = netdev_priv(dev);
	spin_lock(&priv->lock);

	statusword = priv->status;
	priv->status = 0;

	if (statusword & SNULL_RX_INTR) {
		pkt = priv->rx_queue;
		if (pkt) {
			priv->rx_queue = pkt->next;
			snull_rx(dev, pkt);
		}
	}

	if (statusword & SNULL_TX_INTR) {
		priv->stats.tx_packets++;
		priv->stats.tx_bytes += priv->tx_packetlen;
		dev_kfree_skb(priv->skb);
	}

	spin_unlock(&priv->lock);

	/* Release the buffer outside the spinlock to avoid lock ordering issues */
	if (pkt)
		snull_release_buffer(pkt);
}

/*
 * snull_hw_tx — 模拟硬件发送（核心逻辑）
 *
 * 【地址转换逻辑】
 * sn0 的 IP 地址第三字节为 0，sn1 的为 1（例如 192.168.0.x vs 192.168.1.x）。
 * 将源和目的 IP 的第三字节异或 1，实现子网切换：
 *   sn0 发包 → 修改 IP → 看起来是 sn1 网段的包 → sn1 接收
 *
 * 【MAC 地址切换】在 snull_header() 中实现：
 * 将目标 MAC 的最后一个字节异或 0x01，实现 sn0↔sn1 的 MAC 映射。
 *
 * 【中断模拟】
 * 将包入队到对端设备的 rx_queue，然后直接调用 snull_interrupt()。
 * 真实网卡驱动中，这里应该是 DMA 完成后硬件自动触发中断。
 */
static void snull_hw_tx(char *buf, int len, struct net_device *dev)
{
	struct iphdr *ih;
	struct net_device *dest;
	struct snull_priv *priv;
	u32 *saddr, *daddr;
	struct snull_packet *tx_buffer;

	if (len < sizeof(struct ethhdr) + sizeof(struct iphdr)) {
		pr_warn("snull: packet too short (%d octets)\n", len);
		return;
	}

	/*
	 * Locate the IP header (immediately after the 14-byte Ethernet header).
	 * Swap the third octet of src/dst IP so the packet crosses the virtual
	 * subnet boundary between sn0 (192.168.0.x) and sn1 (192.168.1.x).
	 */
	ih = (struct iphdr *)(buf + sizeof(struct ethhdr));
	saddr = &ih->saddr;
	daddr = &ih->daddr;

	((u8 *)saddr)[2] ^= 1;
	((u8 *)daddr)[2] ^= 1;

	/* Recompute the IP header checksum after modifying the addresses */
	ih->check = 0;
	ih->check = ip_fast_csum((unsigned char *)ih, ih->ihl);

	/* Route to the peer device */
	dest = (dev == snull_devs[0]) ? snull_devs[1] : snull_devs[0];

	/* Simulate RX on the destination side */
	priv = netdev_priv(dest);
	tx_buffer = snull_get_tx_buffer(dev);
	tx_buffer->datalen = len;
	memcpy(tx_buffer->data, buf, len);
	snull_enqueue_buf(dest, tx_buffer);

	if (priv->rx_int_enabled) {
		priv->status |= SNULL_RX_INTR;
		snull_interrupt(0, dest, NULL);
	}

	/* Simulate TX-complete on the source side */
	priv = netdev_priv(dev);
	priv->tx_packetlen = len;
	priv->tx_packetdata = buf;
	priv->status |= SNULL_TX_INTR;
	snull_interrupt(0, dev, NULL);
}

static int snull_tx(struct sk_buff *skb, struct net_device *dev)
{
	int len;
	char *data, shortpkt[ETH_ZLEN];
	struct snull_priv *priv = netdev_priv(dev);

	data = skb->data;
	len = skb->len;

	/* Pad frames shorter than the minimum Ethernet payload size */
	if (len < ETH_ZLEN) {
		memset(shortpkt, 0, ETH_ZLEN);
		memcpy(shortpkt, skb->data, skb->len);
		len = ETH_ZLEN;
		data = shortpkt;
	}

	priv->skb = skb;
	snull_hw_tx(data, len, dev);

	return 0;
}

static struct net_device_stats *snull_stats(struct net_device *dev)
{
	struct snull_priv *priv = netdev_priv(dev);
	return &priv->stats;
}

/*
 * snull_header - build the Ethernet header for an outgoing packet
 *
 * The key operation is toggling the LSB of the destination MAC address
 * (h_dest[ETH_ALEN-1] ^= 0x01) so that sn0's MAC maps to sn1's MAC and
 * vice versa, enabling the two virtual interfaces to communicate.
 *
 * Fix: removed ~15 debug printk() calls that printed MAC addresses, protocol
 * type, addr_len, and hard_header_len on every single packet transmission.
 */
static int snull_header(struct sk_buff *skb, struct net_device *dev,
			unsigned short type, const void *daddr,
			const void *saddr, unsigned int len)
{
	struct ethhdr *eth = (struct ethhdr *)skb_push(skb, ETH_HLEN);

	eth->h_proto = htons(type);

	memcpy(eth->h_source, saddr ? saddr : dev->dev_addr, dev->addr_len);
	memcpy(eth->h_dest,   daddr ? daddr : dev->dev_addr, dev->addr_len);

	/*
	 * Toggle the LSB of the destination MAC so that packets from sn0
	 * (\0SNUL0) are addressed to sn1 (\0SNUL1), and vice versa.
	 */
	eth->h_dest[ETH_ALEN - 1] ^= 0x01;

	return dev->hard_header_len;
}

static int snull_change_mtu(struct net_device *dev, int new_mtu)
{
	unsigned long flags;
	struct snull_priv *priv = netdev_priv(dev);

	if (new_mtu < 68 || new_mtu > 1500)
		return -EINVAL;

	spin_lock_irqsave(&priv->lock, flags);
	dev->mtu = new_mtu;
	spin_unlock_irqrestore(&priv->lock, flags);
	return 0;
}

static const struct header_ops snull_header_ops = {
	.create = snull_header,
};

static const struct net_device_ops snull_netdev_ops = {
	.ndo_open	 = snull_open,
	.ndo_stop	 = snull_release,
	.ndo_start_xmit  = snull_tx,
	.ndo_get_stats	 = snull_stats,
	.ndo_change_mtu  = snull_change_mtu,
};

static void snull_init(struct net_device *dev)
{
	struct snull_priv *priv;

	ether_setup(dev);

	dev->header_ops = &snull_header_ops;
	dev->netdev_ops = &snull_netdev_ops;
	dev->flags      |= IFF_NOARP;
	dev->features   |= NETIF_F_HW_CSUM;

	priv = netdev_priv(dev);
	memset(priv, 0, sizeof(*priv));
	spin_lock_init(&priv->lock);
	snull_rx_ints(dev, 1);
	snull_setup_pool(dev);
}

void snull_module_exit(void)
{
	int i;

	for (i = 0; i < 2; i++) {
		if (snull_devs[i]) {
			unregister_netdev(snull_devs[i]);
			snull_teardown_pool(snull_devs[i]);
			free_netdev(snull_devs[i]);
		}
	}
}

/*
 * snull_module_init - allocate and register both virtual network devices
 *
 * Fix: the original loop set ret=0 whenever *any* device registered
 * successfully, masking failures of the other device and always returning 0
 * even when both registrations failed. Now we track each result independently
 * and call snull_module_exit() (which cleans up both devices) if either fails.
 */
static int snull_module_init(void)
{
	int i;
	int result;

	snull_interrupt = snull_regular_interrupt;

	snull_devs[0] = alloc_netdev(sizeof(struct snull_priv), "sn%d",
				     NET_NAME_UNKNOWN, snull_init);
	snull_devs[1] = alloc_netdev(sizeof(struct snull_priv), "sn%d",
				     NET_NAME_UNKNOWN, snull_init);

	if (!snull_devs[0] || !snull_devs[1]) {
		pr_err("snull: alloc_netdev failed\n");
		snull_module_exit();
		return -ENOMEM;
	}

	for (i = 0; i < 2; i++) {
		result = register_netdev(snull_devs[i]);
		if (result) {
			pr_err("snull: register_netdev failed for %s, err=%d\n",
			       snull_devs[i]->name, result);
			snull_module_exit();
			return result;	/* fix: was always returning 0 */
		}
	}

	pr_info("snull: module loaded, sn0 and sn1 registered\n");
	return 0;
}

module_init(snull_module_init);
module_exit(snull_module_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Simple virtual network driver: sn0/sn1 loopback pair");
