// SPDX-License-Identifier: GPL-2.0
/*
 * virt_can.c — Virtual CAN controller
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/can/dev.h>

#define DRV_NAME "virt_can"

static const struct can_bittiming_const virt_bt = {
	.name = "virt_can",
	.tseg1_min = 1,
	.tseg1_max = 16,
	.tseg2_min = 1,
	.tseg2_max = 8,
	.sjw_max = 4,
	.brp_min = 1,
	.brp_max = 64,
	.brp_inc = 1,
};
static int virt_set_bt(struct net_device *dev)
{
	return 0;
}
static netdev_tx_t virt_xmit(struct sk_buff *skb, struct net_device *dev)
{
	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;
	kfree_skb(skb);
	return NETDEV_TX_OK;
}
static int virt_ndo_open(struct net_device *dev)
{
	int ret = open_candev(dev);

	if (ret)
		return ret;
	netif_start_queue(dev);
	return 0;
}
static int virt_ndo_stop(struct net_device *dev)
{
	netif_stop_queue(dev);
	close_candev(dev);
	return 0;
}
static const struct net_device_ops virt_ndo = {
	.ndo_open = virt_ndo_open,
	.ndo_stop = virt_ndo_stop,
	.ndo_start_xmit = virt_xmit,
};
static int virt_probe(struct platform_device *pdev)
{
	struct net_device *ndev;
	struct can_priv *priv;
	int ret;

	ndev = alloc_candev(0, 1);
	if (!ndev)
		return -ENOMEM;
	priv = netdev_priv(ndev);
	priv->bittiming_const = &virt_bt;
	priv->do_set_bittiming = virt_set_bt;
	priv->clock.freq = 8000000;
	priv->state = CAN_STATE_STOPPED;
	ndev->netdev_ops = &virt_ndo;
	SET_NETDEV_DEV(ndev, &pdev->dev);
	ret = register_candev(ndev);
	if (ret) {
		free_candev(ndev);
		return ret;
	}
	platform_set_drvdata(pdev, ndev);
	return 0;
}
static int virt_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);

	unregister_candev(ndev);
	free_candev(ndev);
	return 0;
}

static struct platform_driver virt_pdrv = {
	.probe = virt_probe,
	.remove = virt_remove,
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
	},
};

static struct platform_device *g_pdev;

static int __init virt_mod_init(void)
{
	int ret;

	g_pdev = platform_device_alloc(DRV_NAME, -1);
	if (!g_pdev)
		return -ENOMEM;
	ret = platform_device_add(g_pdev);
	if (ret) {
		platform_device_put(g_pdev);
		return ret;
	}
	ret = platform_driver_register(&virt_pdrv);
	if (ret) {
		platform_device_unregister(g_pdev);
		return ret;
	}
	if (!g_pdev->dev.driver) {
		platform_driver_unregister(&virt_pdrv);
		platform_device_unregister(g_pdev);
		pr_err("%s: probe did not bind\n", DRV_NAME);
		return -ENODEV;
	}
	return 0;
}

static void __exit virt_mod_exit(void)
{
	platform_driver_unregister(&virt_pdrv);
	platform_device_unregister(g_pdev);
}

module_init(virt_mod_init);
module_exit(virt_mod_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Virtual CAN controller");
