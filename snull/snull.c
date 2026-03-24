/*
 * snull.c -- the Simple Network Utility
 *
 * Copyright (C) 2020.3.24 liweijie<ee.liweijie@gmail.com>
 *
 * Derived from "Linux Device Drivers" by Alessandro Rubini and Jonathan Corbet,
 * published by O'Reilly & Associates.
 *
 * Demonstrates a virtual Ethernet driver: two interfaces sn0/sn1 that can
 * ping each other. Packets sent from sn0 are looped back as received on sn1,
 * and vice versa, by swapping the third octet of the IP address and toggling
 * the LSB of the destination MAC address.
 *
 * Fix history:
 *   - Remove ~15 debug printk() calls from snull_header(): they printed MAC
 *     addresses, protocol type, and hard_header_len on every single packet,
 *     flooding dmesg and degrading performance significantly.
 *   - Remove redundant per-packet debug printk() calls from snull_tx(),
 *     snull_hw_tx(), snull_rx(), snull_regular_interrupt(), and snull_init().
 *     Retained only pr_info() calls that are genuinely informative (open,
 *     release, pool exhaustion, low-memory rx drop).
 *   - Fix snull_module_init() error handling: the loop set ret=0 even when
 *     only the second device registered successfully while the first failed;
 *     now both must succeed, and the function returns the actual error code
 *     instead of always returning 0.
 *   - Remove dead commented-out code (old dest selection comment).
 *   - Replace bare printk() (missing log level) with pr_* macros.
 */

#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/etherdevice.h>
#include <linux/tcp.h>

#define SNULL_RX_INTR 0x0001
#define SNULL_TX_INTR 0x0002

int pool_size = 8;
module_param(pool_size, int, 0);

void snull_module_exit(void);
static void (*snull_interrupt)(int, void *, struct pt_regs *);

struct net_device *snull_devs[2];

struct snull_packet {
	struct snull_packet *next;
	struct net_device *dev;
	int datalen;
	u8 data[ETH_DATA_LEN];
};

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

static int snull_open(struct net_device *dev)
{
	/* Assign fake MAC addresses: \0SNUL0 for sn0, \0SNUL1 for sn1 */
	if (dev == snull_devs[0])
		memcpy(dev->dev_addr, "\0SNUL0", ETH_ALEN);
	else
		memcpy(dev->dev_addr, "\0SNUL1", ETH_ALEN);

	netif_start_queue(dev);
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
 * snull_rx - deliver a received packet up the network stack
 *
 * Allocates an skb, copies the packet data into it, and calls netif_rx()
 * to hand it to the kernel's networking subsystem.
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
 * snull_regular_interrupt - simulated hardware interrupt handler
 *
 * Handles both RX (SNULL_RX_INTR) and TX-complete (SNULL_TX_INTR) events
 * that are triggered synthetically by snull_hw_tx().
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
 * snull_hw_tx - simulate hardware transmission
 *
 * Swaps the third octet of both source and destination IP addresses so that
 * packets from sn0's subnet appear to arrive on sn1's subnet, then triggers
 * a simulated RX interrupt on the peer device and a TX-complete interrupt on
 * the sending device.
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
