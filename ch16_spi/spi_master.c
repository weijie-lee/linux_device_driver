/*
 * spi_master.c - Virtual SPI master controller driver
 *
 * Implements a software-simulated SPI master controller that registers itself
 * with the Linux SPI subsystem. Because no real hardware is involved, the
 * transfer function simply echoes TX data back into the RX buffer (loopback),
 * making it possible to verify the full SPI driver stack on any Linux machine
 * without dedicated SPI hardware.
 *
 * Verification path:
 *   1. insmod spi_master.ko  → creates /sys/bus/spi/devices/spi0.*
 *   2. insmod spi_slave.ko   → probes against this master via spi_board_info
 *   3. echo / cat /dev/spi_virt → exercises the full transfer path
 *
 * Key concepts demonstrated:
 *   - spi_alloc_master() / spi_register_master()
 *   - struct spi_master.transfer_one_message()
 *   - spi_message / spi_transfer iteration
 *   - Platform device as the parent for the SPI controller
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>

#define DRIVER_NAME "spi_virt_master"

/* Per-controller private data */
struct spi_virt_master {
	struct spi_master *master;
};

/*
 * transfer_one_message - process a complete spi_message
 *
 * Iterates over every spi_transfer in the message. For each transfer the
 * function copies tx_buf into rx_buf (loopback), simulating a full-duplex
 * exchange. If tx_buf is NULL the rx_buf is filled with 0xAA to make the
 * loopback visible in test output.
 */
static int spi_virt_transfer_one_message(struct spi_master *master,
					 struct spi_message *msg)
{
	struct spi_transfer *xfer;
	int total = 0;

	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		if (!xfer->len)
			continue;

		if (xfer->rx_buf) {
			if (xfer->tx_buf)
				/* loopback: echo TX into RX */
				memcpy((void *)xfer->rx_buf, xfer->tx_buf,
				       xfer->len);
			else
				/* no TX: fill RX with a recognisable pattern */
				memset((void *)xfer->rx_buf, 0xAA, xfer->len);
		}

		total += xfer->len;
		pr_debug("spi_virt_master: transfer %d bytes (speed=%u Hz, bpw=%u)\n",
			 xfer->len, xfer->speed_hz, xfer->bits_per_word);
	}

	msg->actual_length = total;
	msg->status = 0;
	spi_finalize_current_message(master);
	return 0;
}

/* ------------------------------------------------------------------ */
/*  Platform driver probe / remove                                      */
/* ------------------------------------------------------------------ */

static int spi_virt_master_probe(struct platform_device *pdev)
{
	struct spi_master *master;
	struct spi_virt_master *priv;
	int ret;

	/*
	 * Allocate an spi_master together with our private data appended
	 * immediately after it (the second argument is the extra size).
	 */
	master = spi_alloc_master(&pdev->dev, sizeof(*priv));
	if (!master) {
		dev_err(&pdev->dev, "spi_alloc_master failed\n");
		return -ENOMEM;
	}

	priv = spi_master_get_devdata(master);
	priv->master = master;

	/* Fill in the master descriptor */
	master->bus_num          = 0;		/* will appear as spi0 */
	master->num_chipselect   = 4;
	master->mode_bits        = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH;
	master->bits_per_word_mask = SPI_BPW_MASK(8) | SPI_BPW_MASK(16);
	master->max_speed_hz     = 50000000;	/* 50 MHz cap */
	master->transfer_one_message = spi_virt_transfer_one_message;

	platform_set_drvdata(pdev, master);

	ret = spi_register_master(master);
	if (ret) {
		dev_err(&pdev->dev, "spi_register_master failed: %d\n", ret);
		spi_master_put(master);
		return ret;
	}

	dev_info(&pdev->dev,
		 "virtual SPI master registered as spi%d (loopback mode)\n",
		 master->bus_num);
	return 0;
}

static int spi_virt_master_remove(struct platform_device *pdev)
{
	struct spi_master *master = platform_get_drvdata(pdev);

	spi_unregister_master(master);
	dev_info(&pdev->dev, "virtual SPI master unregistered\n");
	return 0;
}

static struct platform_driver spi_virt_master_driver = {
	.driver = {
		.name  = DRIVER_NAME,
		.owner = THIS_MODULE,
	},
	.probe  = spi_virt_master_probe,
	.remove = spi_virt_master_remove,
};

/* ------------------------------------------------------------------ */
/*  Module init / exit: self-register a platform_device so the driver  */
/*  can be tested without a Device Tree or ACPI table.                  */
/* ------------------------------------------------------------------ */

static struct platform_device *spi_virt_pdev;

static int __init spi_virt_master_init(void)
{
	int ret;

	/* Create a transient platform device to act as the controller's parent */
	spi_virt_pdev = platform_device_alloc(DRIVER_NAME, -1);
	if (!spi_virt_pdev)
		return -ENOMEM;

	ret = platform_device_add(spi_virt_pdev);
	if (ret) {
		platform_device_put(spi_virt_pdev);
		return ret;
	}

	ret = platform_driver_register(&spi_virt_master_driver);
	if (ret) {
		platform_device_del(spi_virt_pdev);
		platform_device_put(spi_virt_pdev);
		return ret;
	}

	pr_info("spi_virt_master: module loaded\n");
	return 0;
}

static void __exit spi_virt_master_exit(void)
{
	platform_driver_unregister(&spi_virt_master_driver);
	platform_device_unregister(spi_virt_pdev);
	pr_info("spi_virt_master: module unloaded\n");
}

module_init(spi_virt_master_init);
module_exit(spi_virt_master_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Virtual SPI master controller driver (loopback, no hardware required)");
