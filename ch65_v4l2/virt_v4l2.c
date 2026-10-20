// SPDX-License-Identifier: GPL-2.0
/*
 * virt_v4l2.c — Virtual V4L2 device
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>

#define DRV_NAME "virt_v4l2"

struct virt_v4l {
	struct v4l2_device v4l2;
	struct video_device *vdev;
};
static int virt_querycap(struct file *file, void *fh, struct v4l2_capability *cap)
{
	strscpy(cap->driver, "virt_v4l2", sizeof(cap->driver));
	strscpy(cap->card, "virtual", sizeof(cap->card));
	strscpy(cap->bus_info, "platform:virt_v4l2", sizeof(cap->bus_info));
	return 0;
}
static const struct v4l2_ioctl_ops virt_ioctl = {
	.vidioc_querycap = virt_querycap,
};
static int virt_vopen(struct file *file)
{
	return 0;
}
static int virt_vrelease(struct file *file)
{
	return 0;
}
static const struct v4l2_file_operations virt_fops = {
	.owner = THIS_MODULE,
	.open = virt_vopen,
	.release = virt_vrelease,
	.unlocked_ioctl = video_ioctl2,
};
static int virt_probe(struct platform_device *pdev)
{
	struct virt_v4l *v;
	int ret;

	v = devm_kzalloc(&pdev->dev, sizeof(*v), GFP_KERNEL);
	if (!v)
		return -ENOMEM;
	ret = v4l2_device_register(&pdev->dev, &v->v4l2);
	if (ret)
		return ret;
	v->vdev = video_device_alloc();
	if (!v->vdev) {
		v4l2_device_unregister(&v->v4l2);
		return -ENOMEM;
	}
	strscpy(v->vdev->name, "virt_v4l2", sizeof(v->vdev->name));
	v->vdev->v4l2_dev = &v->v4l2;
	v->vdev->fops = &virt_fops;
	v->vdev->ioctl_ops = &virt_ioctl;
	v->vdev->release = video_device_release;
	v->vdev->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_READWRITE;
	video_set_drvdata(v->vdev, v);
	ret = video_register_device(v->vdev, VFL_TYPE_VIDEO, -1);
	if (ret) {
		video_device_release(v->vdev);
		v4l2_device_unregister(&v->v4l2);
		return ret;
	}
	platform_set_drvdata(pdev, v);
	return 0;
}
static int virt_remove(struct platform_device *pdev)
{
	struct virt_v4l *v = platform_get_drvdata(pdev);

	video_unregister_device(v->vdev);
	v4l2_device_unregister(&v->v4l2);
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
MODULE_DESCRIPTION("Virtual V4L2 device");
