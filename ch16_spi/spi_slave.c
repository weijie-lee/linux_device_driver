/*
 * spi_slave.c - Virtual SPI slave (peripheral) driver
 *
 * This module acts as an SPI peripheral device driver that sits on top of the
 * virtual SPI master registered by spi_master.ko. It exposes a character device
 * /dev/spi_virt so that user-space programs can perform read/write operations
 * that are translated into SPI full-duplex transfers.
 *
 * Because the master uses loopback mode, every byte written will be read back
 * unchanged — a convenient self-test that requires no real SPI hardware.
 *
 * Key concepts demonstrated:
 *   - spi_driver probe/remove lifecycle
 *   - spi_message / spi_transfer construction
 *   - spi_sync() for synchronous (blocking) transfers
 *   - Exposing an SPI device to user-space via a character device
 */

#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define DRIVER_NAME   "spi_virt_slave"
#define SPI_BUF_SIZE  256

static dev_t spi_devno;
static struct class *spi_class;

struct spi_virt_slave {
	struct spi_device *spi;
	struct cdev cdev;
	u8 tx_buf[SPI_BUF_SIZE];
	u8 rx_buf[SPI_BUF_SIZE];
};

static struct spi_virt_slave *g_slave;	/* single-instance shortcut */

/* ------------------------------------------------------------------ */
/*  Character device file operations                                    */
/* ------------------------------------------------------------------ */

static int spi_virt_open(struct inode *inode, struct file *filp)
{
	struct spi_virt_slave *slave =
		container_of(inode->i_cdev, struct spi_virt_slave, cdev);
	filp->private_data = slave;
	return 0;
}

/*
 * spi_virt_write - accept data from user-space and send it over SPI
 *
 * The write path constructs a full-duplex spi_transfer: the user data goes
 * into tx_buf, and the loopback master will echo it back into rx_buf.
 */
static ssize_t spi_virt_write(struct file *filp, const char __user *buf,
			      size_t count, loff_t *ppos)
{
	struct spi_virt_slave *slave = filp->private_data;
	struct spi_message msg;
	struct spi_transfer xfer = {};
	int ret;

	if (count > SPI_BUF_SIZE)
		count = SPI_BUF_SIZE;

	if (copy_from_user(slave->tx_buf, buf, count))
		return -EFAULT;

	memset(slave->rx_buf, 0, count);

	xfer.tx_buf   = slave->tx_buf;
	xfer.rx_buf   = slave->rx_buf;
	xfer.len      = count;
	xfer.speed_hz = 1000000;	/* 1 MHz */

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	ret = spi_sync(slave->spi, &msg);
	if (ret) {
		pr_err("spi_virt_slave: spi_sync failed: %d\n", ret);
		return ret;
	}

	pr_info("spi_virt_slave: wrote %zu bytes, actual=%u\n",
		count, msg.actual_length);
	return count;
}

/*
 * spi_virt_read - return the last RX buffer contents to user-space
 *
 * After a write, the loopback master has filled rx_buf with the echoed data.
 * A subsequent read returns those bytes so the user can verify the round-trip.
 */
static ssize_t spi_virt_read(struct file *filp, char __user *buf,
			     size_t count, loff_t *ppos)
{
	struct spi_virt_slave *slave = filp->private_data;

	if (count > SPI_BUF_SIZE)
		count = SPI_BUF_SIZE;

	if (copy_to_user(buf, slave->rx_buf, count))
		return -EFAULT;

	pr_info("spi_virt_slave: read %zu bytes from rx_buf\n", count);
	return count;
}

static const struct file_operations spi_virt_fops = {
	.owner = THIS_MODULE,
	.open  = spi_virt_open,
	.write = spi_virt_write,
	.read  = spi_virt_read,
};

/* ------------------------------------------------------------------ */
/*  SPI driver probe / remove                                           */
/* ------------------------------------------------------------------ */

static int spi_virt_slave_probe(struct spi_device *spi)
{
	struct spi_virt_slave *slave;
	int ret;

	slave = kzalloc(sizeof(*slave), GFP_KERNEL);
	if (!slave)
		return -ENOMEM;

	slave->spi = spi;
	spi_set_drvdata(spi, slave);
	g_slave = slave;

	/* Register a character device so user-space can access the SPI bus */
	ret = alloc_chrdev_region(&spi_devno, 0, 1, DRIVER_NAME);
	if (ret) {
		pr_err("spi_virt_slave: alloc_chrdev_region failed: %d\n", ret);
		goto err_free;
	}

	cdev_init(&slave->cdev, &spi_virt_fops);
	slave->cdev.owner = THIS_MODULE;
	ret = cdev_add(&slave->cdev, spi_devno, 1);
	if (ret) {
		pr_err("spi_virt_slave: cdev_add failed: %d\n", ret);
		goto err_unreg;
	}

	spi_class = class_create("spi_virt");
	if (IS_ERR(spi_class)) {
		ret = PTR_ERR(spi_class);
		goto err_cdev;
	}

	device_create(spi_class, NULL, spi_devno, NULL, "spi_virt");

	dev_info(&spi->dev,
		 "SPI slave probed on bus %d cs %d, /dev/spi_virt created\n",
		 spi->master->bus_num, spi->chip_select);
	return 0;

err_cdev:
	cdev_del(&slave->cdev);
err_unreg:
	unregister_chrdev_region(spi_devno, 1);
err_free:
	kfree(slave);
	return ret;
}

static void spi_virt_slave_remove(struct spi_device *spi)
{
	struct spi_virt_slave *slave = spi_get_drvdata(spi);

	device_destroy(spi_class, spi_devno);
	class_destroy(spi_class);
	cdev_del(&slave->cdev);
	unregister_chrdev_region(spi_devno, 1);
	kfree(slave);
	dev_info(&spi->dev, "SPI slave removed\n");
}

/* Match table: the master will instantiate a device with this modalias */
static const struct spi_device_id spi_virt_id[] = {
	{ "spi-virt-dev", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, spi_virt_id);

static struct spi_driver spi_virt_slave_driver = {
	.driver = {
		.name  = DRIVER_NAME,
		.owner = THIS_MODULE,
	},
	.id_table = spi_virt_id,
	.probe    = spi_virt_slave_probe,
	.remove   = spi_virt_slave_remove,
};

/* ------------------------------------------------------------------ */
/*  Module init / exit: register the slave driver and manually          */
/*  instantiate an SPI device on bus 0, CS 0 so the probe fires.        */
/* ------------------------------------------------------------------ */

static struct spi_device *spi_virt_dev;

static int __init spi_virt_slave_init(void)
{
	struct spi_master *master;
	struct spi_board_info board = {
		.modalias     = "spi-virt-dev",
		.max_speed_hz = 1000000,
		.bus_num      = 0,
		.chip_select  = 0,
		.mode         = SPI_MODE_0,
	};
	int ret;

	ret = spi_register_driver(&spi_virt_slave_driver);
	if (ret) {
		pr_err("spi_virt_slave: spi_register_driver failed: %d\n", ret);
		return ret;
	}

	/* Manually instantiate the SPI device on the virtual master's bus */
	master = spi_busnum_to_controller(0);
	if (!master) {
		pr_err("spi_virt_slave: spi master 0 not found — load spi_master.ko first\n");
		spi_unregister_driver(&spi_virt_slave_driver);
		return -ENODEV;
	}

	spi_virt_dev = spi_new_device(master, &board);
	put_device(&master->dev);

	if (!spi_virt_dev) {
		pr_err("spi_virt_slave: spi_new_device failed\n");
		spi_unregister_driver(&spi_virt_slave_driver);
		return -ENODEV;
	}

	pr_info("spi_virt_slave: module loaded, /dev/spi_virt ready\n");
	return 0;
}

static void __exit spi_virt_slave_exit(void)
{
	if (spi_virt_dev)
		spi_unregister_device(spi_virt_dev);
	spi_unregister_driver(&spi_virt_slave_driver);
	pr_info("spi_virt_slave: module unloaded\n");
}

module_init(spi_virt_slave_init);
module_exit(spi_virt_slave_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Virtual SPI slave driver — exposes /dev/spi_virt for loopback testing");
