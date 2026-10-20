// SPDX-License-Identifier: GPL-2.0
/*
 * virt_scsi.c — Virtual SCSI host
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>

#define DRV_NAME "virt_scsi"

static int virt_qcmd(struct Scsi_Host *shost, struct scsi_cmnd *cmd)
{
	cmd->result = DID_NO_CONNECT << 16;
	cmd->scsi_done(cmd);
	return 0;
}
static struct scsi_host_template virt_sht = {
	.module = THIS_MODULE,
	.name = "virt_scsi",
	.proc_name = "virt_scsi",
	.queuecommand = virt_qcmd,
	.can_queue = 1,
	.this_id = -1,
	.sg_tablesize = 1,
	.cmd_per_lun = 1,
	.max_sectors = 64,
};
static int virt_probe(struct platform_device *pdev)
{
	struct Scsi_Host *shost;
	int ret;

	shost = scsi_host_alloc(&virt_sht, 0);
	if (!shost)
		return -ENOMEM;
	ret = scsi_add_host(shost, &pdev->dev);
	if (ret) {
		scsi_host_put(shost);
		return ret;
	}
	platform_set_drvdata(pdev, shost);
	return 0;
}
static int virt_remove(struct platform_device *pdev)
{
	struct Scsi_Host *shost = platform_get_drvdata(pdev);

	scsi_remove_host(shost);
	scsi_host_put(shost);
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
MODULE_DESCRIPTION("Virtual SCSI host");
