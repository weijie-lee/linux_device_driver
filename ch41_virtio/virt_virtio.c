// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/virtio.h>

static int virt_probe(struct virtio_device *vdev)
{
	dev_info(&vdev->dev, "virt_virtio probe\n");
	return 0;
}
static void virt_remove(struct virtio_device *vdev)
{
}
static const struct virtio_device_id virt_ids[] = {
	{ 0xffff, VIRTIO_DEV_ANY_ID },
	{ 0 },
};
static struct virtio_driver virt_drv = {
	.driver.name = "virt_virtio",
	.driver.owner = THIS_MODULE,
	.id_table = virt_ids,
	.probe = virt_probe,
	.remove = virt_remove,
};
static int __init virt_init(void)
{
	return register_virtio_driver(&virt_drv);
}
static void __exit virt_exit(void)
{
	unregister_virtio_driver(&virt_drv);
}
module_init(virt_init);
module_exit(virt_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Virtual virtio driver registration");
