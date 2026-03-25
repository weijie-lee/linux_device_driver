/*
 * USB 总线驱动 - 基础示例
 * 
 * 功能：
 * 1. USB 设备枚举和探测
 * 2. USB 配置和接口选择
 * 3. USB 端点管理
 * 4. 基本的 USB 驱动框架
 * 
 * 编译：make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
 */

#include <linux/module.h>
#include <linux/usb.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Linux Driver");
MODULE_DESCRIPTION("USB Bus Basic Driver");
MODULE_VERSION("1.0");

/* USB 设备 ID 表 */
static const struct usb_device_id usb_basic_ids[] = {
	/* 虚拟 USB 设备 ID（用于 QEMU 模拟） */
	{ USB_DEVICE(0x1234, 0x5678) },  /* Vendor ID: 0x1234, Product ID: 0x5678 */
	{ }
};
MODULE_DEVICE_TABLE(usb, usb_basic_ids);

/* 驱动私有数据结构 */
struct usb_basic_dev {
	struct usb_device *udev;
	struct usb_interface *interface;
	unsigned char *bulk_in_buffer;
	size_t bulk_in_size;
	__u8 bulk_in_endpointAddr;
	__u8 bulk_out_endpointAddr;
	struct urb *bulk_in_urb;
	struct urb *bulk_out_urb;
};

/*
 * USB 中断完成回调函数
 */
static void usb_basic_bulk_callback(struct urb *urb)
{
	struct usb_basic_dev *dev = urb->context;

	if (urb->status) {
		pr_warn("[USB BASIC] Bulk callback error: %d\n", urb->status);
		return;
	}

	pr_info("[USB BASIC] Bulk transfer completed: %d bytes\n", urb->actual_length);
}

/*
 * USB 设备探测函数
 * 当 USB 总线发现匹配的设备时调用
 */
static int usb_basic_probe(struct usb_interface *interface,
			   const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(interface);
	struct usb_basic_dev *dev;
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int i, ret;

	pr_info("[USB BASIC] Probing USB device: %04x:%04x\n",
		udev->descriptor.idVendor, udev->descriptor.idProduct);

	/* 分配驱动私有数据 */
	dev = devm_kzalloc(&interface->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->udev = usb_get_dev(udev);
	dev->interface = interface;

	/* 获取当前接口描述符 */
	iface_desc = interface->cur_altsetting;

	pr_info("[USB BASIC] Interface: %d\n", iface_desc->desc.bInterfaceNumber);
	pr_info("[USB BASIC] Endpoints: %d\n", iface_desc->desc.bNumEndpoints);

	/* 扫描端点 */
	for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
		endpoint = &iface_desc->endpoint[i].desc;

		pr_info("[USB BASIC] Endpoint %d: addr=0x%02x, attr=0x%02x, maxpkt=%d\n",
			i, endpoint->bEndpointAddress, endpoint->bmAttributes,
			le16_to_cpu(endpoint->wMaxPacketSize));

		/* 识别 Bulk IN 端点 */
		if (!dev->bulk_in_endpointAddr &&
		    usb_endpoint_is_bulk_in(endpoint)) {
			dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
			dev->bulk_in_size = le16_to_cpu(endpoint->wMaxPacketSize);
			pr_info("[USB BASIC] Found Bulk IN endpoint: 0x%02x\n",
				dev->bulk_in_endpointAddr);
		}

		/* 识别 Bulk OUT 端点 */
		if (!dev->bulk_out_endpointAddr &&
		    usb_endpoint_is_bulk_out(endpoint)) {
			dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
			pr_info("[USB BASIC] Found Bulk OUT endpoint: 0x%02x\n",
				dev->bulk_out_endpointAddr);
		}
	}

	/* 检查是否找到了必要的端点 */
	if (!dev->bulk_in_endpointAddr) {
		pr_warn("[USB BASIC] No Bulk IN endpoint found\n");
	}
	if (!dev->bulk_out_endpointAddr) {
		pr_warn("[USB BASIC] No Bulk OUT endpoint found\n");
	}

	/* 分配 Bulk IN 缓冲区 */
	if (dev->bulk_in_endpointAddr) {
		dev->bulk_in_buffer = devm_kmalloc(&interface->dev,
						   dev->bulk_in_size, GFP_KERNEL);
		if (!dev->bulk_in_buffer) {
			pr_err("[USB BASIC] Failed to allocate Bulk IN buffer\n");
			ret = -ENOMEM;
			goto error;
		}
		pr_info("[USB BASIC] Bulk IN buffer allocated: %zu bytes\n",
			dev->bulk_in_size);
	}

	/* 保存驱动私有数据 */
	usb_set_intfdata(interface, dev);

	pr_info("[USB BASIC] Device probed successfully\n");
	return 0;

error:
	usb_put_dev(dev->udev);
	return ret;
}

/*
 * USB 设备断开连接函数
 * 当 USB 设备被移除时调用
 */
static void usb_basic_disconnect(struct usb_interface *interface)
{
	struct usb_basic_dev *dev = usb_get_intfdata(interface);

	pr_info("[USB BASIC] Disconnecting USB device\n");

	usb_set_intfdata(interface, NULL);
	usb_put_dev(dev->udev);

	pr_info("[USB BASIC] Device disconnected\n");
}

/* USB 驱动结构 */
static struct usb_driver usb_basic_driver = {
	.name = "usb_basic",
	.id_table = usb_basic_ids,
	.probe = usb_basic_probe,
	.disconnect = usb_basic_disconnect,
};

/*
 * 模块初始化函数
 */
static int __init usb_basic_init(void)
{
	int ret;

	pr_info("[USB BASIC] Initializing USB Basic Driver\n");

	ret = usb_register(&usb_basic_driver);
	if (ret) {
		pr_err("[USB BASIC] Failed to register USB driver\n");
		return ret;
	}

	pr_info("[USB BASIC] USB driver registered successfully\n");
	return 0;
}

/*
 * 模块清理函数
 */
static void __exit usb_basic_exit(void)
{
	pr_info("[USB BASIC] Exiting USB Basic Driver\n");
	usb_deregister(&usb_basic_driver);
	pr_info("[USB BASIC] USB driver unregistered\n");
}

module_init(usb_basic_init);
module_exit(usb_basic_exit);
