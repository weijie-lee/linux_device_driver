/*
 * eth_mac.c - Virtual Ethernet MAC + PHY driver
 *
 * Implements a complete Ethernet network device driver that registers a
 * virtual NIC (veth0_mac) with the Linux network stack. The driver
 * demonstrates the full MAC driver lifecycle and includes a software-
 * simulated PHY state machine so that link-up/link-down events can be
 * observed without real hardware.
 *
 * Packet handling uses a loopback model: every transmitted frame is
 * immediately fed back into the receive path via netif_rx(), making it
 * possible to ping the interface from user-space and observe real packet
 * counters.
 *
 * Key concepts demonstrated:
 *   - alloc_etherdev() / register_netdev()
 *   - struct net_device_ops (open, stop, start_xmit, get_stats64)
 *   - sk_buff allocation and manipulation (dev_alloc_skb, skb_put, netif_rx)
 *   - NAPI (poll-based receive) with napi_add / napi_schedule
 *   - Virtual PHY: simulating link state via a kernel timer
 *   - ethtool_ops for link/speed reporting
 *   - Per-CPU statistics with u64_stats_sync
 */

#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/timer.h>
#include <linux/ethtool.h>
#include <linux/u64_stats_sync.h>

#define DRIVER_NAME     "eth_virt_mac"
#define TX_TIMEOUT_HZ   (5 * HZ)
#define PHY_POLL_HZ     (2 * HZ)	/* PHY link-check interval */

/* Per-device statistics (mirrors struct rtnl_link_stats64) */
struct virt_stats {
	u64 rx_packets;
	u64 rx_bytes;
	u64 tx_packets;
	u64 tx_bytes;
	u64 rx_dropped;
	struct u64_stats_sync syncp;
};

/* Driver private data */
struct virt_mac_priv {
	struct net_device *dev;
	struct napi_struct napi;

	/* Virtual PHY state */
	struct timer_list phy_timer;
	bool link_up;
	int  speed;		/* Mbps: 10 / 100 / 1000 */
	int  duplex;		/* DUPLEX_HALF / DUPLEX_FULL */

	/* Loopback receive queue (single skb, simplified) */
	spinlock_t rx_lock;
	struct sk_buff_head rx_queue;

	struct virt_stats stats;
	/* Required by netif_info/netif_dbg/netif_warn macros */
	u32 msg_enable;
};

/* ------------------------------------------------------------------ */
/*  Virtual PHY: simulate link negotiation via a periodic timer         */
/* ------------------------------------------------------------------ */

static void phy_timer_fn(struct timer_list *t)
{
	struct virt_mac_priv *priv = from_timer(priv, t, phy_timer);
	struct net_device *dev = priv->dev;

	if (!priv->link_up) {
		/* Simulate auto-negotiation completing: link comes up at 1 Gbps */
		priv->link_up = true;
		priv->speed   = 1000;
		priv->duplex  = DUPLEX_FULL;
		netif_carrier_on(dev);
		netif_info(priv, link, dev,
			   "PHY link UP: %d Mbps %s-duplex\n",
			   priv->speed,
			   priv->duplex == DUPLEX_FULL ? "full" : "half");
	}

	/* Reschedule for periodic PHY polling */
	mod_timer(&priv->phy_timer, jiffies + PHY_POLL_HZ);
}

/* ------------------------------------------------------------------ */
/*  NAPI receive poll                                                    */
/* ------------------------------------------------------------------ */

static int virt_mac_poll(struct napi_struct *napi, int budget)
{
	struct virt_mac_priv *priv =
		container_of(napi, struct virt_mac_priv, napi);
	int work_done = 0;

	while (work_done < budget) {
		struct sk_buff *skb;
		unsigned long flags;

		spin_lock_irqsave(&priv->rx_lock, flags);
		skb = skb_dequeue(&priv->rx_queue);
		spin_unlock_irqrestore(&priv->rx_lock, flags);

		if (!skb)
			break;

		/* Update RX statistics */
		u64_stats_update_begin(&priv->stats.syncp);
		priv->stats.rx_packets++;
		priv->stats.rx_bytes += skb->len;
		u64_stats_update_end(&priv->stats.syncp);

		skb->protocol = eth_type_trans(skb, priv->dev);
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		netif_receive_skb(skb);
		work_done++;
	}

	if (work_done < budget)
		napi_complete_done(napi, work_done);

	return work_done;
}

/* ------------------------------------------------------------------ */
/*  net_device_ops                                                       */
/* ------------------------------------------------------------------ */

static int virt_mac_open(struct net_device *dev)
{
	struct virt_mac_priv *priv = netdev_priv(dev);

	netif_start_queue(dev);
	napi_enable(&priv->napi);

	/* Start the PHY polling timer */
	timer_setup(&priv->phy_timer, phy_timer_fn, 0);
	mod_timer(&priv->phy_timer, jiffies + HZ / 2);

	netdev_info(dev, "interface opened\n");
	return 0;
}

static int virt_mac_stop(struct net_device *dev)
{
	struct virt_mac_priv *priv = netdev_priv(dev);

	netif_stop_queue(dev);
	napi_disable(&priv->napi);
	del_timer_sync(&priv->phy_timer);
	netif_carrier_off(dev);
	priv->link_up = false;

	netdev_info(dev, "interface stopped\n");
	return 0;
}

/*
 * virt_mac_start_xmit - transmit an sk_buff
 *
 * In a real MAC driver this function would DMA the frame into the TX ring
 * and ring the doorbell register. Here we simply clone the skb and push it
 * onto the RX queue to simulate a loopback, then schedule NAPI to drain it.
 */
static netdev_tx_t virt_mac_start_xmit(struct sk_buff *skb,
					struct net_device *dev)
{
	struct virt_mac_priv *priv = netdev_priv(dev);
	struct sk_buff *rx_skb;
	unsigned long flags;

	/* Update TX statistics */
	u64_stats_update_begin(&priv->stats.syncp);
	priv->stats.tx_packets++;
	priv->stats.tx_bytes += skb->len;
	u64_stats_update_end(&priv->stats.syncp);

	/* Clone the skb for the loopback receive path */
	rx_skb = skb_copy(skb, GFP_ATOMIC);
	if (rx_skb) {
		spin_lock_irqsave(&priv->rx_lock, flags);
		skb_queue_tail(&priv->rx_queue, rx_skb);
		spin_unlock_irqrestore(&priv->rx_lock, flags);
		napi_schedule(&priv->napi);
	} else {
		u64_stats_update_begin(&priv->stats.syncp);
		priv->stats.rx_dropped++;
		u64_stats_update_end(&priv->stats.syncp);
	}

	dev_kfree_skb(skb);
	return NETDEV_TX_OK;
}

static void virt_mac_get_stats64(struct net_device *dev,
				 struct rtnl_link_stats64 *stats)
{
	struct virt_mac_priv *priv = netdev_priv(dev);
	unsigned int start;

	do {
		start = u64_stats_fetch_begin_irq(&priv->stats.syncp);
		stats->rx_packets = priv->stats.rx_packets;
		stats->rx_bytes   = priv->stats.rx_bytes;
		stats->tx_packets = priv->stats.tx_packets;
		stats->tx_bytes   = priv->stats.tx_bytes;
		stats->rx_dropped = priv->stats.rx_dropped;
	} while (u64_stats_fetch_retry_irq(&priv->stats.syncp, start));
}

static void virt_mac_tx_timeout(struct net_device *dev, unsigned int txqueue)
{
	netdev_warn(dev, "TX timeout — resetting queue\n");
	netif_wake_queue(dev);
}

static const struct net_device_ops virt_mac_ops = {
	.ndo_open         = virt_mac_open,
	.ndo_stop         = virt_mac_stop,
	.ndo_start_xmit   = virt_mac_start_xmit,
	.ndo_get_stats64  = virt_mac_get_stats64,
	.ndo_tx_timeout   = virt_mac_tx_timeout,
	.ndo_set_mac_address = eth_mac_addr,
	.ndo_validate_addr   = eth_validate_addr,
};

/* ------------------------------------------------------------------ */
/*  ethtool_ops — report PHY link state to ethtool                      */
/* ------------------------------------------------------------------ */

static int virt_mac_get_link_ksettings(struct net_device *dev,
				       struct ethtool_link_ksettings *cmd)
{
	struct virt_mac_priv *priv = netdev_priv(dev);

	ethtool_link_ksettings_zero_link_mode(cmd, supported);
	ethtool_link_ksettings_add_link_mode(cmd, supported, 1000baseT_Full);
	ethtool_link_ksettings_add_link_mode(cmd, supported, Autoneg);

	cmd->base.speed   = priv->link_up ? priv->speed : SPEED_UNKNOWN;
	cmd->base.duplex  = priv->link_up ? priv->duplex : DUPLEX_UNKNOWN;
	cmd->base.port    = PORT_TP;
	cmd->base.autoneg = AUTONEG_ENABLE;
	return 0;
}

static u32 virt_mac_get_link(struct net_device *dev)
{
	struct virt_mac_priv *priv = netdev_priv(dev);
	return priv->link_up ? 1 : 0;
}

static void virt_mac_get_drvinfo(struct net_device *dev,
				 struct ethtool_drvinfo *info)
{
	strlcpy(info->driver,  DRIVER_NAME,       sizeof(info->driver));
	strlcpy(info->version, "1.0",             sizeof(info->version));
	strlcpy(info->bus_info, "virtual",        sizeof(info->bus_info));
}

static const struct ethtool_ops virt_mac_ethtool_ops = {
	.get_drvinfo        = virt_mac_get_drvinfo,
	.get_link           = virt_mac_get_link,
	.get_link_ksettings = virt_mac_get_link_ksettings,
};

/* ------------------------------------------------------------------ */
/*  Module init / exit                                                   */
/* ------------------------------------------------------------------ */

static struct net_device *virt_dev;

static int __init virt_mac_init(void)
{
	struct virt_mac_priv *priv;
	int ret;

	/* Allocate net_device with private data appended */
	virt_dev = alloc_etherdev(sizeof(*priv));
	if (!virt_dev)
		return -ENOMEM;

	strlcpy(virt_dev->name, "veth0_mac", IFNAMSIZ);

	priv = netdev_priv(virt_dev);
	priv->dev = virt_dev;

	spin_lock_init(&priv->rx_lock);
	skb_queue_head_init(&priv->rx_queue);
	u64_stats_init(&priv->stats.syncp);
	priv->msg_enable = netif_msg_init(-1, NETIF_MSG_LINK | NETIF_MSG_PROBE);

	/* Assign a locally-administered MAC address */
	eth_hw_addr_random(virt_dev);

	virt_dev->netdev_ops      = &virt_mac_ops;
	virt_dev->ethtool_ops     = &virt_mac_ethtool_ops;
	virt_dev->watchdog_timeo  = TX_TIMEOUT_HZ;
	virt_dev->flags          |= IFF_NOARP;	/* no ARP for loopback */
	virt_dev->features       |= NETIF_F_LOOPBACK;

	/* Register NAPI before register_netdev */
	netif_napi_add(virt_dev, &priv->napi, virt_mac_poll, 64);

	ret = register_netdev(virt_dev);
	if (ret) {
		pr_err("virt_mac: register_netdev failed: %d\n", ret);
		netif_napi_del(&priv->napi);
		free_netdev(virt_dev);
		return ret;
	}

	pr_info("virt_mac: registered network device '%s' (MAC %pM)\n",
		virt_dev->name, virt_dev->dev_addr);
	return 0;
}

static void __exit virt_mac_exit(void)
{
	struct virt_mac_priv *priv = netdev_priv(virt_dev);

	unregister_netdev(virt_dev);
	netif_napi_del(&priv->napi);
	skb_queue_purge(&priv->rx_queue);
	free_netdev(virt_dev);
	pr_info("virt_mac: module unloaded\n");
}

module_init(virt_mac_init);
module_exit(virt_mac_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Virtual Ethernet MAC+PHY driver with NAPI and loopback (no hardware required)");
