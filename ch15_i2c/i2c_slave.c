/*
 * i2c_slave.c - Virtual I2C client (slave) driver
 *
 * Implements an i2c_driver that probes against the virtual I2C adapter
 * created by i2c_master.ko. After probing, it registers a character device
 * /dev/i2c_virt so that user-space programs can perform register-level
 * read/write operations over the virtual I2C bus.
 *
 * The device is manually instantiated via i2c_new_client_device() in module_init,
 * which is the standard approach when no Device Tree or ACPI table is present.
 *
 * Key concepts demonstrated:
 *   - i2c_driver probe/remove lifecycle
 *   - i2c_master_send() / i2c_master_recv() — high-level transfer helpers
 *   - i2c_smbus_read/write_byte_data() — SMBus register access
 *   - Manual device instantiation with i2c_new_client_device()
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define DRIVER_NAME     "i2c_virt_slave"
#define VIRT_SLAVE_ADDR 0x50
#define I2C_BUF_SIZE    128

static dev_t i2c_devno;
static struct class *i2c_class;

/*
 * User-space ioctl command: set the register address before a read.
 * Usage: ioctl(fd, I2C_VIRT_SET_REG, reg_addr)
 */
#define I2C_VIRT_MAGIC   'v'
#define I2C_VIRT_SET_REG _IOW(I2C_VIRT_MAGIC, 0, unsigned char)

struct i2c_virt_client {
	struct i2c_client *client;
	struct cdev cdev;
	u8 current_reg;		/* register pointer for next read */
};

static struct i2c_virt_client *g_client;

/* ------------------------------------------------------------------ */
/*  Character device file operations                                    */
/* ------------------------------------------------------------------ */

static int i2c_virt_open(struct inode *inode, struct file *filp)
{
	struct i2c_virt_client *priv =
		container_of(inode->i_cdev, struct i2c_virt_client, cdev);
	filp->private_data = priv;
	return 0;
}

/*
 * i2c_virt_write - write bytes to the virtual I2C device
 *
 * Format: first byte of the user buffer is the register address,
 * followed by the data bytes to write. This mirrors the standard
 * I2C EEPROM/sensor write protocol.
 *
 *   buf[0]  = register address
 *   buf[1..n] = data bytes
 */
static ssize_t i2c_virt_write(struct file *filp, const char __user *buf,
			      size_t count, loff_t *ppos)
{
	struct i2c_virt_client *priv = filp->private_data;
	u8 kbuf[I2C_BUF_SIZE + 1];
	int ret;

	if (count > I2C_BUF_SIZE)
		count = I2C_BUF_SIZE;

	if (copy_from_user(kbuf, buf, count))
		return -EFAULT;

	/*
	 * i2c_master_send() sends the buffer as a single WRITE message.
	 * kbuf[0] is the register address; kbuf[1..] are the data bytes.
	 */
	ret = i2c_master_send(priv->client, kbuf, count);
	if (ret < 0) {
		pr_err("i2c_virt_slave: i2c_master_send failed: %d\n", ret);
		return ret;
	}

	/* Remember the register pointer for the next read */
	priv->current_reg = kbuf[0] + (count - 1);

	pr_info("i2c_virt_slave: wrote %d byte(s) starting at reg 0x%02x\n",
		ret - 1, kbuf[0]);
	return count;
}

/*
 * i2c_virt_read - read bytes from the virtual I2C device
 *
 * Sends a WRITE message with just the register address (current_reg),
 * then issues a READ message to fetch the data. This is the standard
 * "write-then-read" (repeated START) pattern used by most I2C sensors.
 */
static ssize_t i2c_virt_read(struct file *filp, char __user *buf,
			     size_t count, loff_t *ppos)
{
	struct i2c_virt_client *priv = filp->private_data;
	u8 kbuf[I2C_BUF_SIZE];
	int ret;

	if (count > I2C_BUF_SIZE)
		count = I2C_BUF_SIZE;

	/*
	 * Step 1: set the register pointer by sending a single-byte WRITE.
	 * Step 2: read back 'count' bytes with i2c_master_recv().
	 */
	ret = i2c_master_send(priv->client, &priv->current_reg, 1);
	if (ret < 0) {
		pr_err("i2c_virt_slave: set reg pointer failed: %d\n", ret);
		return ret;
	}

	ret = i2c_master_recv(priv->client, kbuf, count);
	if (ret < 0) {
		pr_err("i2c_virt_slave: i2c_master_recv failed: %d\n", ret);
		return ret;
	}

	if (copy_to_user(buf, kbuf, ret))
		return -EFAULT;

	pr_info("i2c_virt_slave: read %d byte(s) from reg 0x%02x\n",
		ret, priv->current_reg);
	return ret;
}

static long i2c_virt_ioctl(struct file *filp, unsigned int cmd,
			   unsigned long arg)
{
	struct i2c_virt_client *priv = filp->private_data;

	switch (cmd) {
	case I2C_VIRT_SET_REG:
		priv->current_reg = (u8)arg;
		pr_info("i2c_virt_slave: register pointer set to 0x%02x\n",
			priv->current_reg);
		return 0;
	default:
		return -ENOTTY;
	}
}

static const struct file_operations i2c_virt_fops = {
	.owner          = THIS_MODULE,
	.open           = i2c_virt_open,
	.write          = i2c_virt_write,
	.read           = i2c_virt_read,
	.unlocked_ioctl = i2c_virt_ioctl,
};

/* ------------------------------------------------------------------ */
/*  I2C driver probe / remove                                           */
/* ------------------------------------------------------------------ */

static int i2c_virt_slave_probe(struct i2c_client *client)
{
	struct i2c_virt_client *priv;
	int ret;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_I2C | I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, "adapter does not support required I2C functions\n");
		return -ENODEV;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->client      = client;
	priv->current_reg = 0x00;
	i2c_set_clientdata(client, priv);
	g_client = priv;

	ret = alloc_chrdev_region(&i2c_devno, 0, 1, DRIVER_NAME);
	if (ret)
		goto err_free;

	cdev_init(&priv->cdev, &i2c_virt_fops);
	priv->cdev.owner = THIS_MODULE;
	ret = cdev_add(&priv->cdev, i2c_devno, 1);
	if (ret)
		goto err_unreg;

	i2c_class = class_create("i2c_virt");
	if (IS_ERR(i2c_class)) {
		ret = PTR_ERR(i2c_class);
		goto err_cdev;
	}

	device_create(i2c_class, NULL, i2c_devno, NULL, "i2c_virt");

	/* Quick SMBus read to verify the virtual device responds */
	{
		s32 val = i2c_smbus_read_byte_data(client, 0x00);
		if (val >= 0)
			dev_info(&client->dev,
				 "device ID register (0x00) = 0x%02x\n",
				 (u8)val);
	}

	dev_info(&client->dev,
		 "I2C slave probed at addr 0x%02x on adapter %s, /dev/i2c_virt created\n",
		 client->addr, client->adapter->name);
	return 0;

err_cdev:
	cdev_del(&priv->cdev);
err_unreg:
	unregister_chrdev_region(i2c_devno, 1);
err_free:
	kfree(priv);
	return ret;
}

static void i2c_virt_slave_remove(struct i2c_client *client)
{
	struct i2c_virt_client *priv = i2c_get_clientdata(client);

	device_destroy(i2c_class, i2c_devno);
	class_destroy(i2c_class);
	cdev_del(&priv->cdev);
	unregister_chrdev_region(i2c_devno, 1);
	kfree(priv);
	dev_info(&client->dev, "I2C slave removed\n");
}

static const struct i2c_device_id i2c_virt_id[] = {
	{ "i2c-virt-dev", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, i2c_virt_id);

static struct i2c_driver i2c_virt_slave_driver = {
	.driver = {
		.name  = DRIVER_NAME,
		.owner = THIS_MODULE,
	},
	.id_table = i2c_virt_id,
	.probe    = i2c_virt_slave_probe,
	.remove   = i2c_virt_slave_remove,
};

/* ------------------------------------------------------------------ */
/*  Module init / exit                                                  */
/* ------------------------------------------------------------------ */

static struct i2c_client *i2c_virt_client_dev;

static int __init i2c_virt_slave_init(void)
{
	struct i2c_adapter *adap;
	struct i2c_board_info board = {
		I2C_BOARD_INFO("i2c-virt-dev", VIRT_SLAVE_ADDR),
	};
	int ret;
	int bus;

	ret = i2c_add_driver(&i2c_virt_slave_driver);
	if (ret) {
		pr_err("i2c_virt_slave: i2c_add_driver failed: %d\n", ret);
		return ret;
	}

	/*
	 * Find the virtual adapter. The master registers with nr=-1 (dynamic),
	 * so we scan from bus 0 upward looking for our adapter by name.
	 */
	adap = NULL;
	for (bus = 0; bus < 16; bus++) {
		struct i2c_adapter *a = i2c_get_adapter(bus);
		if (!a)
			continue;
		if (strstr(a->name, "i2c-virt-master")) {
			adap = a;
			break;
		}
		i2c_put_adapter(a);
	}

	if (!adap) {
		pr_err("i2c_virt_slave: virtual adapter not found — load i2c_master.ko first\n");
		i2c_del_driver(&i2c_virt_slave_driver);
		return -ENODEV;
	}

	i2c_virt_client_dev = i2c_new_client_device(adap, &board);
	i2c_put_adapter(adap);

	if (!i2c_virt_client_dev) {
		pr_err("i2c_virt_slave: i2c_new_client_device failed\n");
		i2c_del_driver(&i2c_virt_slave_driver);
		return -ENODEV;
	}

	pr_info("i2c_virt_slave: module loaded, /dev/i2c_virt ready\n");
	return 0;
}

static void __exit i2c_virt_slave_exit(void)
{
	if (i2c_virt_client_dev)
		i2c_unregister_device(i2c_virt_client_dev);
	i2c_del_driver(&i2c_virt_slave_driver);
	pr_info("i2c_virt_slave: module unloaded\n");
}

module_init(i2c_virt_slave_init);
module_exit(i2c_virt_slave_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Virtual I2C client driver — exposes /dev/i2c_virt for register-level access");
