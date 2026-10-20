// SPDX-License-Identifier: GPL-2.0
/*
 * virt_tty.c — virtual tty/serial port
 */
#include <linux/module.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>

static struct tty_driver *virt_drv;
static struct tty_port virt_port;

static int virt_open(struct tty_struct *tty, struct file *filp)
{
	return tty_port_open(&virt_port, tty, filp);
}
static void virt_close(struct tty_struct *tty, struct file *filp)
{
	tty_port_close(&virt_port, tty, filp);
}
static int virt_write(struct tty_struct *tty, const unsigned char *buf, int count)
{
	return count;
}
static unsigned int virt_write_room(struct tty_struct *tty)
{
	return 4096;
}
static const struct tty_operations virt_ops = {
	.open = virt_open,
	.close = virt_close,
	.write = virt_write,
	.write_room = virt_write_room,
};

static int __init virt_init(void)
{
	int ret;

	virt_drv = tty_alloc_driver(1, TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV);
	if (IS_ERR(virt_drv))
		return PTR_ERR(virt_drv);
	virt_drv->driver_name = "virt_tty";
	virt_drv->name = "ttyVIRT";
	virt_drv->type = TTY_DRIVER_TYPE_SERIAL;
	virt_drv->subtype = SERIAL_TYPE_NORMAL;
	virt_drv->init_termios = tty_std_termios;
	virt_drv->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	tty_set_operations(virt_drv, &virt_ops);
	tty_port_init(&virt_port);
	ret = tty_register_driver(virt_drv);
	if (ret)
		goto err_drv;
	tty_port_register_device(&virt_port, virt_drv, 0, NULL);
	pr_info("virt_tty: /dev/ttyVIRT registered\n");
	return 0;
err_drv:
	tty_port_destroy(&virt_port);
	tty_driver_kref_put(virt_drv);
	return ret;
}
static void __exit virt_exit(void)
{
	tty_unregister_device(virt_drv, 0);
	tty_unregister_driver(virt_drv);
	tty_port_destroy(&virt_port);
	tty_driver_kref_put(virt_drv);
}
module_init(virt_init);
module_exit(virt_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Virtual tty/serial port");
