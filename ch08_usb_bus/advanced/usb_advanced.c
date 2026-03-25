/*
 * USB 总线驱动 - 高级示例
 * 
 * 功能：
 * 1. USB URB（请求块）管理
 * 2. USB 异步传输
 * 3. 字符设备接口
 * 4. USB 电源管理
 * 5. 热插拔支持
 * 
 * 编译：make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
 */

#include <linux/module.h>
#include <linux/usb.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Linux Driver");
MODULE_DESCRIPTION("USB Bus Advanced Driver");
MODULE_VERSION("2.0");

#define USB_ADVANCED_NAME "usb_advanced"
#define USB_ADVANCED_MINOR_BASE 0
#define USB_ADVANCED_NR_MINORS 16

/* USB 设备 ID 表 */
static const struct usb_device_id usb_advanced_ids[] = {
	{ USB_DEVICE(0x1234, 0x5679) },  /* 高级驱动设备 ID */
	{ }
};
MODULE_DEVICE_TABLE(usb, usb_advanced_ids);

/* 驱动私有数据结构 */
struct usb_advanced_dev {
	struct usb_device *udev;
	struct usb_interface *interface;
	struct cdev cdev;
	dev_t devno;
	
	/* 端点信息 */
	__u8 bulk_in_endpointAddr;
	__u8 bulk_out_endpointAddr;
	__u8 int_in_endpointAddr;
	
	/* URB 和缓冲区 */
	struct urb *bulk_in_urb;
	struct urb *bulk_out_urb;
	struct urb *int_in_urb;
	
	unsigned char *bulk_in_buffer;
	unsigned char *bulk_out_buffer;
	unsigned char *int_in_buffer;
	
	size_t bulk_in_size;
	size_t bulk_out_size;
	size_t int_in_size;
	
	/* 同步和统计 */
	struct mutex io_mutex;
	unsigned long bulk_in_count;
	unsigned long bulk_out_count;
	unsigned long int_in_count;
	
	/* 设备状态 */
	int open_count;
	int disconnected;
};

static struct class *usb_advanced_class;
static dev_t usb_advanced_devno;

/*
 * Bulk IN URB 完成回调
 */
static void usb_advanced_bulk_in_callback(struct urb *urb)
{
	struct usb_advanced_dev *dev = urb->context;

	if (urb->status) {
		if (urb->status != -ENOENT && urb->status != -ECONNRESET &&
		    urb->status != -ESHUTDOWN) {
			pr_warn("[USB ADV] Bulk IN error: %d\n", urb->status);
		}
		return;
	}

	dev->bulk_in_count++;
	pr_info("[USB ADV] Bulk IN completed: %d bytes (count: %lu)\n",
		urb->actual_length, dev->bulk_in_count);
}

/*
 * Bulk OUT URB 完成回调
 */
static void usb_advanced_bulk_out_callback(struct urb *urb)
{
	struct usb_advanced_dev *dev = urb->context;

	if (urb->status) {
		if (urb->status != -ENOENT && urb->status != -ECONNRESET &&
		    urb->status != -ESHUTDOWN) {
			pr_warn("[USB ADV] Bulk OUT error: %d\n", urb->status);
		}
		return;
	}

	dev->bulk_out_count++;
	pr_info("[USB ADV] Bulk OUT completed: %d bytes (count: %lu)\n",
		urb->actual_length, dev->bulk_out_count);
}

/*
 * 中断 IN URB 完成回调
 */
static void usb_advanced_int_in_callback(struct urb *urb)
{
	struct usb_advanced_dev *dev = urb->context;
	int ret;

	if (urb->status) {
		if (urb->status != -ENOENT && urb->status != -ECONNRESET &&
		    urb->status != -ESHUTDOWN) {
			pr_warn("[USB ADV] Interrupt IN error: %d\n", urb->status);
		}
		return;
	}

	dev->int_in_count++;
	pr_info("[USB ADV] Interrupt IN completed: %d bytes (count: %lu)\n",
		urb->actual_length, dev->int_in_count);

	/* 重新提交 URB 以继续接收中断 */
	ret = usb_submit_urb(urb, GFP_ATOMIC);
	if (ret) {
		pr_err("[USB ADV] Failed to resubmit interrupt URB: %d\n", ret);
	}
}

/*
 * 字符设备 open 函数
 */
static int usb_advanced_open(struct inode *inode, struct file *filp)
{
	struct usb_advanced_dev *dev;
	int ret = 0;

	dev = container_of(inode->i_cdev, struct usb_advanced_dev, cdev);
	filp->private_data = dev;

	mutex_lock(&dev->io_mutex);

	if (dev->disconnected) {
		ret = -ENODEV;
		goto error;
	}

	dev->open_count++;
	pr_info("[USB ADV] Device opened (count: %d)\n", dev->open_count);

error:
	mutex_unlock(&dev->io_mutex);
	return ret;
}

/*
 * 字符设备 read 函数
 */
static ssize_t usb_advanced_read(struct file *filp, char __user *buf,
				  size_t count, loff_t *ppos)
{
	struct usb_advanced_dev *dev = filp->private_data;
	int ret;

	if (!dev->bulk_in_buffer)
		return -ENOMEM;

	if (count > dev->bulk_in_size)
		count = dev->bulk_in_size;

	ret = copy_to_user(buf, dev->bulk_in_buffer, count);
	if (ret)
		return -EFAULT;

	pr_info("[USB ADV] Read %zu bytes\n", count);
	return count;
}

/*
 * 字符设备 write 函数
 */
static ssize_t usb_advanced_write(struct file *filp, const char __user *buf,
				   size_t count, loff_t *ppos)
{
	struct usb_advanced_dev *dev = filp->private_data;
	int ret;

	if (!dev->bulk_out_buffer)
		return -ENOMEM;

	if (count > dev->bulk_out_size)
		count = dev->bulk_out_size;

	ret = copy_from_user(dev->bulk_out_buffer, buf, count);
	if (ret)
		return -EFAULT;

	pr_info("[USB ADV] Write %zu bytes\n", count);
	return count;
}

/*
 * 字符设备 ioctl 函数
 */
static long usb_advanced_ioctl(struct file *filp, unsigned int cmd,
			       unsigned long arg)
{
	struct usb_advanced_dev *dev = filp->private_data;

	switch (cmd) {
	case 0x2001:  /* GET_BULK_IN_COUNT */
		if (copy_to_user((void __user *)arg, &dev->bulk_in_count,
				  sizeof(dev->bulk_in_count)))
			return -EFAULT;
		return 0;

	case 0x2002:  /* GET_BULK_OUT_COUNT */
		if (copy_to_user((void __user *)arg, &dev->bulk_out_count,
				  sizeof(dev->bulk_out_count)))
			return -EFAULT;
		return 0;

	case 0x2003:  /* GET_INT_IN_COUNT */
		if (copy_to_user((void __user *)arg, &dev->int_in_count,
				  sizeof(dev->int_in_count)))
			return -EFAULT;
		return 0;

	default:
		return -ENOTTY;
	}
}

/*
 * 字符设备 close 函数
 */
static int usb_advanced_release(struct inode *inode, struct file *filp)
{
	struct usb_advanced_dev *dev = filp->private_data;

	mutex_lock(&dev->io_mutex);
	dev->open_count--;
	pr_info("[USB ADV] Device closed (count: %d)\n", dev->open_count);
	mutex_unlock(&dev->io_mutex);

	return 0;
}

static const struct file_operations usb_advanced_fops = {
	.owner = THIS_MODULE,
	.open = usb_advanced_open,
	.read = usb_advanced_read,
	.write = usb_advanced_write,
	.unlocked_ioctl = usb_advanced_ioctl,
	.release = usb_advanced_release,
};

/*
 * USB 设备探测函数
 */
static int usb_advanced_probe(struct usb_interface *interface,
			      const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(interface);
	struct usb_advanced_dev *dev;
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int i, ret;

	pr_info("[USB ADV] Probing USB device: %04x:%04x\n",
		udev->descriptor.idVendor, udev->descriptor.idProduct);

	/* 分配驱动私有数据 */
	dev = devm_kzalloc(&interface->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->udev = usb_get_dev(udev);
	dev->interface = interface;
	mutex_init(&dev->io_mutex);

	/* 获取当前接口描述符 */
	iface_desc = interface->cur_altsetting;

	/* 扫描端点 */
	for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
		endpoint = &iface_desc->endpoint[i].desc;

		if (usb_endpoint_is_bulk_in(endpoint)) {
			dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
			dev->bulk_in_size = le16_to_cpu(endpoint->wMaxPacketSize);
			pr_info("[USB ADV] Bulk IN: 0x%02x, size=%zu\n",
				dev->bulk_in_endpointAddr, dev->bulk_in_size);
		}

		if (usb_endpoint_is_bulk_out(endpoint)) {
			dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
			dev->bulk_out_size = le16_to_cpu(endpoint->wMaxPacketSize);
			pr_info("[USB ADV] Bulk OUT: 0x%02x, size=%zu\n",
				dev->bulk_out_endpointAddr, dev->bulk_out_size);
		}

		if (usb_endpoint_is_int_in(endpoint)) {
			dev->int_in_endpointAddr = endpoint->bEndpointAddress;
			dev->int_in_size = le16_to_cpu(endpoint->wMaxPacketSize);
			pr_info("[USB ADV] Interrupt IN: 0x%02x, size=%zu\n",
				dev->int_in_endpointAddr, dev->int_in_size);
		}
	}

	/* 分配缓冲区 */
	if (dev->bulk_in_size > 0) {
		dev->bulk_in_buffer = devm_kmalloc(&interface->dev,
						   dev->bulk_in_size, GFP_KERNEL);
		if (!dev->bulk_in_buffer)
			goto error;
	}

	if (dev->bulk_out_size > 0) {
		dev->bulk_out_buffer = devm_kmalloc(&interface->dev,
						    dev->bulk_out_size, GFP_KERNEL);
		if (!dev->bulk_out_buffer)
			goto error;
	}

	if (dev->int_in_size > 0) {
		dev->int_in_buffer = devm_kmalloc(&interface->dev,
						  dev->int_in_size, GFP_KERNEL);
		if (!dev->int_in_buffer)
			goto error;
	}

	/* 注册字符设备 */
	ret = alloc_chrdev_region(&dev->devno, USB_ADVANCED_MINOR_BASE, 1,
				  USB_ADVANCED_NAME);
	if (ret) {
		pr_err("[USB ADV] Failed to allocate chrdev region\n");
		goto error;
	}

	cdev_init(&dev->cdev, &usb_advanced_fops);
	dev->cdev.owner = THIS_MODULE;
	ret = cdev_add(&dev->cdev, dev->devno, 1);
	if (ret) {
		pr_err("[USB ADV] Failed to add cdev\n");
		goto error_unregister;
	}

	/* 创建设备类 */
	if (!usb_advanced_class) {
		usb_advanced_class = class_create(THIS_MODULE, USB_ADVANCED_NAME);
		if (IS_ERR(usb_advanced_class)) {
			ret = PTR_ERR(usb_advanced_class);
			goto error_cdev_del;
		}
	}

	/* 创建设备节点 */
	device_create(usb_advanced_class, NULL, dev->devno, NULL,
		      "%s", USB_ADVANCED_NAME);

	usb_set_intfdata(interface, dev);

	pr_info("[USB ADV] Device probed successfully\n");
	return 0;

error_cdev_del:
	cdev_del(&dev->cdev);
error_unregister:
	unregister_chrdev_region(dev->devno, 1);
error:
	usb_put_dev(dev->udev);
	return ret;
}

/*
 * USB 设备断开连接函数
 */
static void usb_advanced_disconnect(struct usb_interface *interface)
{
	struct usb_advanced_dev *dev = usb_get_intfdata(interface);

	pr_info("[USB ADV] Disconnecting USB device\n");

	mutex_lock(&dev->io_mutex);
	dev->disconnected = 1;
	mutex_unlock(&dev->io_mutex);

	device_destroy(usb_advanced_class, dev->devno);
	cdev_del(&dev->cdev);
	unregister_chrdev_region(dev->devno, 1);

	usb_set_intfdata(interface, NULL);
	usb_put_dev(dev->udev);

	pr_info("[USB ADV] Device disconnected\n");
}

/* USB 驱动结构 */
static struct usb_driver usb_advanced_driver = {
	.name = "usb_advanced",
	.id_table = usb_advanced_ids,
	.probe = usb_advanced_probe,
	.disconnect = usb_advanced_disconnect,
};

/*
 * 模块初始化函数
 */
static int __init usb_advanced_init(void)
{
	int ret;

	pr_info("[USB ADV] Initializing USB Advanced Driver\n");

	ret = usb_register(&usb_advanced_driver);
	if (ret) {
		pr_err("[USB ADV] Failed to register USB driver\n");
		return ret;
	}

	pr_info("[USB ADV] USB driver registered successfully\n");
	return 0;
}

/*
 * 模块清理函数
 */
static void __exit usb_advanced_exit(void)
{
	pr_info("[USB ADV] Exiting USB Advanced Driver\n");
	usb_deregister(&usb_advanced_driver);
	if (usb_advanced_class) {
		class_destroy(usb_advanced_class);
		usb_advanced_class = NULL;
	}
	pr_info("[USB ADV] USB driver unregistered\n");
}

module_init(usb_advanced_init);
module_exit(usb_advanced_exit);
