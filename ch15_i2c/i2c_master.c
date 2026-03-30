/*
 * i2c_master.c - Virtual I2C adapter (master) driver
 *
 * Registers a software-simulated I2C adapter with the Linux I2C subsystem.
 * The adapter maintains a small per-address register file (256 bytes) that
 * acts as a virtual I2C EEPROM/sensor. Write transactions store data into the
 * register file; read transactions return the stored data. This makes it
 * possible to exercise the full I2C driver stack without any I2C hardware.
 *
 * Verification path:
 *   1. insmod i2c_master.ko  → creates /dev/i2c-N (N = allocated bus number)
 *   2. i2cdetect -y N        → shows device at address 0x50
 *   3. insmod i2c_slave.ko   → probes against this adapter
 *   4. Read/write /dev/i2c_virt or use i2cget/i2cset for direct access
 *
 * Key concepts demonstrated:
 *   - i2c_add_adapter() / i2c_del_adapter()
 *   - struct i2c_algorithm.master_xfer() — the core transfer callback
 *   - i2c_msg READ/WRITE flag handling
 *   - Simulating a register-addressed I2C slave inside the adapter
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#define DRIVER_NAME     "i2c_virt_master"
#define VIRT_SLAVE_ADDR 0x50		/* simulated device address */
#define REG_FILE_SIZE   256		/* 256-byte register space */

struct i2c_virt_master {
	struct i2c_adapter adapter;
	u8 reg_file[REG_FILE_SIZE];	/* virtual register storage */
	u8 reg_ptr;			/* current register pointer */
};

/*
 * i2c_virt_xfer - simulate I2C transactions
 *
 * Protocol emulation:
 *   WRITE msg: first byte sets the register pointer; subsequent bytes are
 *              stored at reg_file[reg_ptr++].
 *   READ  msg: bytes are read from reg_file[reg_ptr++].
 *
 * Any address other than VIRT_SLAVE_ADDR returns -ENXIO (no ACK), which
 * causes i2cdetect to show "--" for that address.
 */
static int i2c_virt_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	struct i2c_virt_master *priv =
		container_of(adap, struct i2c_virt_master, adapter);
	int i;

	for (i = 0; i < num; i++) {
		struct i2c_msg *msg = &msgs[i];

		/* Only respond to our simulated slave address */
		if (msg->addr != VIRT_SLAVE_ADDR) {
			pr_debug("i2c_virt_master: NACK addr=0x%02x\n", msg->addr);
			return -ENXIO;
		}

		if (msg->flags & I2C_M_RD) {
			/* READ: return bytes from the register file */
			int j;
			for (j = 0; j < msg->len; j++) {
				msg->buf[j] = priv->reg_file[priv->reg_ptr];
				pr_debug("i2c_virt_master: READ reg[0x%02x]=0x%02x\n",
					 priv->reg_ptr, msg->buf[j]);
				priv->reg_ptr = (priv->reg_ptr + 1) % REG_FILE_SIZE;
			}
		} else {
			/* WRITE: first byte is the register address */
			int j;
			if (msg->len == 0)
				continue;

			priv->reg_ptr = msg->buf[0];
			pr_debug("i2c_virt_master: WRITE set reg_ptr=0x%02x\n",
				 priv->reg_ptr);

			for (j = 1; j < msg->len; j++) {
				priv->reg_file[priv->reg_ptr] = msg->buf[j];
				pr_debug("i2c_virt_master: WRITE reg[0x%02x]=0x%02x\n",
					 priv->reg_ptr, msg->buf[j]);
				priv->reg_ptr = (priv->reg_ptr + 1) % REG_FILE_SIZE;
			}
		}
	}

	return num;	/* return the number of messages processed */
}

static u32 i2c_virt_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm i2c_virt_algo = {
	.master_xfer   = i2c_virt_xfer,
	.functionality = i2c_virt_func,
};

/* ------------------------------------------------------------------ */
/*  Module init / exit                                                  */
/* ------------------------------------------------------------------ */

static struct i2c_virt_master *g_master;

static int __init i2c_virt_master_init(void)
{
	int ret;

	g_master = kzalloc(sizeof(*g_master), GFP_KERNEL);
	if (!g_master)
		return -ENOMEM;

	/* Pre-fill the register file with a recognisable pattern */
	memset(g_master->reg_file, 0xAB, REG_FILE_SIZE);
	g_master->reg_file[0x00] = 0x50;	/* "device ID" at reg 0 */
	g_master->reg_file[0x01] = 0x01;	/* "version" at reg 1 */

	/* Describe the adapter */
	g_master->adapter.owner   = THIS_MODULE;
	g_master->adapter.class   = I2C_CLASS_HWMON;
	g_master->adapter.algo    = &i2c_virt_algo;
	g_master->adapter.nr      = -1;		/* dynamic bus number */
	strscpy(g_master->adapter.name, "i2c-virt-master",
		sizeof(g_master->adapter.name));

	ret = i2c_add_adapter(&g_master->adapter);
	if (ret) {
		pr_err("i2c_virt_master: i2c_add_adapter failed: %d\n", ret);
		kfree(g_master);
		return ret;
	}

	pr_info("i2c_virt_master: registered as i2c-%d, virtual slave at 0x%02x\n",
		g_master->adapter.nr, VIRT_SLAVE_ADDR);
	return 0;
}

static void __exit i2c_virt_master_exit(void)
{
	i2c_del_adapter(&g_master->adapter);
	kfree(g_master);
	pr_info("i2c_virt_master: module unloaded\n");
}

module_init(i2c_virt_master_init);
module_exit(i2c_virt_master_exit);

/* Export the bus number so i2c_slave.ko can find the adapter */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Virtual I2C adapter with built-in register-file slave (no hardware required)");
