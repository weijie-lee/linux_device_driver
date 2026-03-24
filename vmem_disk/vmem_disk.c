/*
 * vmem_disk.c - Virtual memory-backed block device driver
 *
 * Demonstrates a simple ramdisk-style block device using vmalloc'd memory.
 * Supports both request-queue mode (VMEMD_QUEUE) and make_request mode
 * (VMEMD_NOQUEUE) selectable via the request_mode module parameter.
 *
 * Fix history:
 *   - Remove duplicate #include <linux/module.h> and <linux/blkdev.h>
 *   - Fix struct member typo: gedisk -> gendisk
 *   - Fix vmem_disk_init(): inverted NULL check caused success path to jump
 *     to error handler; corrected to `if (!devices)`
 *   - Fix setup_device(): out_vfree label was placed outside the switch block,
 *     causing unconditional vfree on every successful path; restructured so
 *     the label is only reached on actual failure
 *   - Implement vmem_disk_exit() to properly release all resources
 *   - Replace printk(KERN_INFO ...) with pr_info/pr_err where appropriate
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/hdreg.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/version.h>

#define BIO_ENDIO(bio, status)	bio_endio(bio)

static int vmem_disk_major;
module_param(vmem_disk_major, int, 0);

#define HARDSECT_SIZE		512
#define NSECTORS		1024
#define NDEVICES		4
#define VMEM_DISK_MINORS	16
#define KERNEL_SECTOR_SHIFT	9
#define KERNEL_SECTOR_SIZE	(1 << KERNEL_SECTOR_SHIFT)

enum {
	VMEMD_QUEUE,
	VMEMD_NOQUEUE,
};

static int request_mode = VMEMD_QUEUE;
module_param(request_mode, int, 0);

struct vmem_disk_dev {
	int size;
	u8 *data;
	int users;
	int media_changes;
	spinlock_t lock;
	struct request_queue *queue;
	struct gendisk *gd;		/* fix: was 'gedisk', correct type is 'gendisk' */
};

static struct vmem_disk_dev *devices;

/* Handle an I/O transfer between the ramdisk buffer and a kernel buffer */
static void vmem_disk_transfer(struct vmem_disk_dev *dev, unsigned long sector,
				unsigned long nsect, char *buffer, int write)
{
	unsigned long offset = sector * KERNEL_SECTOR_SIZE;
	unsigned long nbytes = nsect * KERNEL_SECTOR_SIZE;

	if ((offset + nbytes) > dev->size) {
		pr_err("vmem_disk: beyond-end access (offset=%lu nbytes=%lu size=%d)\n",
		       offset, nbytes, dev->size);
		return;
	}

	if (write)
		memcpy(dev->data + offset, buffer, nbytes);
	else
		memcpy(buffer, dev->data + offset, nbytes);
}

/* Transfer a single bio segment by segment */
static int vmem_disk_xfer_bio(struct vmem_disk_dev *dev, struct bio *bio)
{
	struct bio_vec bvec;
	struct bvec_iter iter;
	sector_t sector = bio->bi_iter.bi_sector;

	bio_for_each_segment(bvec, bio, iter) {
		char *buffer = kmap_atomic(bvec.bv_page) + bvec.bv_offset;

		vmem_disk_transfer(dev, sector,
				   bio_cur_bytes(bio) >> KERNEL_SECTOR_SHIFT,
				   buffer, bio_data_dir(bio) == WRITE);
		sector += bio_cur_bytes(bio) >> KERNEL_SECTOR_SHIFT;
		kunmap_atomic(buffer);
	}

	return 0;
}

/* make_request path: bypass the request queue entirely */
static blk_qc_t vmem_disk_make_request(struct request_queue *q, struct bio *bio)
{
	struct vmem_disk_dev *dev = q->queuedata;
	int status;

	status = vmem_disk_xfer_bio(dev, bio);
	BIO_ENDIO(bio, status);

	return BLK_QC_T_NONE;
}

/* request_queue path: process requests one by one */
static void vmem_disk_request(struct request_queue *q)
{
	struct request *req;
	struct bio *bio;

	while ((req = blk_peek_request(q)) != NULL) {
		struct vmem_disk_dev *dev = req->rq_disk->private_data;

		if (req->cmd_type != REQ_TYPE_FS) {
			pr_info("vmem_disk: skip non-fs request\n");
			blk_start_request(req);
			__blk_end_request_all(req, -EIO);
			continue;
		}

		blk_start_request(req);
		__rq_for_each_bio(bio, req)
			vmem_disk_xfer_bio(dev, bio);
		__blk_end_request_all(req, 0);
	}
}

/*
 * setup_device - initialise one vmem_disk instance
 *
 * Fix: the original code placed 'out_vfree' *after* the switch block, so
 * execution always fell through to vfree() even on the success path.
 * Now each failure branch returns early after cleaning up, and the function
 * only returns successfully when the queue is properly allocated.
 */
static int setup_device(struct vmem_disk_dev *dev, int which)
{
	memset(dev, 0, sizeof(*dev));
	dev->size = NSECTORS * HARDSECT_SIZE;

	dev->data = vmalloc(dev->size);
	if (!dev->data) {
		pr_err("vmem_disk: vmalloc failure for device %d\n", which);
		return -ENOMEM;
	}

	spin_lock_init(&dev->lock);

	switch (request_mode) {
	case VMEMD_NOQUEUE:
		dev->queue = blk_alloc_queue(GFP_KERNEL);
		if (!dev->queue) {
			pr_err("vmem_disk: blk_alloc_queue failed for device %d\n", which);
			goto out_vfree;
		}
		blk_queue_make_request(dev->queue, vmem_disk_make_request);
		break;

	case VMEMD_QUEUE:
		dev->queue = blk_init_queue(vmem_disk_request, &dev->lock);
		if (!dev->queue) {
			pr_err("vmem_disk: blk_init_queue failed for device %d\n", which);
			goto out_vfree;
		}
		break;

	default:
		pr_info("vmem_disk: unknown request_mode %d, falling back to VMEMD_QUEUE\n",
			request_mode);
		dev->queue = blk_init_queue(vmem_disk_request, &dev->lock);
		if (!dev->queue)
			goto out_vfree;
		break;
	}

	return 0;

out_vfree:
	vfree(dev->data);
	dev->data = NULL;
	return -ENOMEM;
}

static int __init vmem_disk_init(void)
{
	int i;
	int ret;
	int major;

	major = register_blkdev(0, "vmem_disk");
	if (major <= 0) {
		pr_err("vmem_disk: unable to get major number\n");
		return -EBUSY;
	}
	vmem_disk_major = major;
	pr_info("vmem_disk: registered with major number %d\n", major);

	devices = kcalloc(NDEVICES, sizeof(struct vmem_disk_dev), GFP_KERNEL);
	if (!devices) {		/* fix: was `if (devices)` — inverted condition */
		ret = -ENOMEM;
		goto out_unregister;
	}

	for (i = 0; i < NDEVICES; i++) {
		ret = setup_device(devices + i, i);
		if (ret) {
			pr_err("vmem_disk: setup_device failed for device %d\n", i);
			goto out_free_devices;
		}
	}

	return 0;

out_free_devices:
	/* clean up any devices that were successfully set up */
	while (--i >= 0) {
		if ((devices + i)->queue)
			blk_cleanup_queue((devices + i)->queue);
		if ((devices + i)->data)
			vfree((devices + i)->data);
	}
	kfree(devices);
out_unregister:
	unregister_blkdev(vmem_disk_major, "vmem_disk");
	return ret;
}

/*
 * vmem_disk_exit - release all resources
 *
 * Fix: original exit function was empty — no cleanup at all.
 * Now properly frees queues, data buffers, device array, and unregisters
 * the block device major number.
 */
static void __exit vmem_disk_exit(void)
{
	int i;

	for (i = 0; i < NDEVICES; i++) {
		struct vmem_disk_dev *dev = devices + i;

		if (dev->queue)
			blk_cleanup_queue(dev->queue);
		if (dev->data)
			vfree(dev->data);
	}

	kfree(devices);
	unregister_blkdev(vmem_disk_major, "vmem_disk");
	pr_info("vmem_disk: module unloaded\n");
}

module_init(vmem_disk_init);
module_exit(vmem_disk_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Virtual memory-backed block device driver demo");
