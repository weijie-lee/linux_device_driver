/*
 * vmem_disk.c - Virtual memory-backed block device driver
 *
 * 本驱动实现一个完全基于内存（vmalloc）的虚拟块设备，无需真实磁盘硬件。
 * 加载后内核会创建 /dev/vmem_disk0（及更多设备），可以像普通磁盘一样
 * 进行 mkfs、mount、fdisk 等操作。
 *
 * 适配说明（Linux 5.x blk-mq API）：
 *   - 旧 API blk_init_queue / blk_peek_request / blk_start_request 已在
 *     5.0 之后移除，本驱动改用 blk_mq_alloc_disk（单队列 blk-mq）。
 *   - request 处理在 blk_mq_ops.queue_rq 回调中完成。
 *
 * 整改记录：
 *   - 修复 init 中 if (devices) 逻辑取反 Bug
 *   - 修复 setup_device 中 goto out_vfree 位置错误导致内存泄漏
 *   - 修复 gendisk 结构体字段名 gedisk → gendisk
 *   - 删除重复 #include
 *   - 补全原本为空的 vmem_disk_exit()
 *   - 重写块设备队列为 blk-mq，适配 5.15 内核
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/hdreg.h>
#include <linux/slab.h>

/* ------------------------------------------------------------------ */
/*  模块参数                                                             */
/* ------------------------------------------------------------------ */

static int NDEVICES = 2;
module_param(NDEVICES, int, 0444);
MODULE_PARM_DESC(NDEVICES, "Number of virtual disk devices (default 2)");

static int HARDSECT_SIZE = 512;
static int NSECTORS      = 1024;
module_param(HARDSECT_SIZE, int, 0444);
module_param(NSECTORS,      int, 0444);

#define KERNEL_SECTOR_SIZE	512
#define KERNEL_SECTOR_SHIFT	9

/* ------------------------------------------------------------------ */
/*  设备私有数据结构                                                     */
/* ------------------------------------------------------------------ */

struct vmem_disk_dev {
int                    size;
u8                    *data;
spinlock_t             lock;
struct gendisk        *gd;
struct blk_mq_tag_set  tag_set;
};

static int                   vmem_disk_major;
static struct vmem_disk_dev *devices;

/* ------------------------------------------------------------------ */
/*  数据传输核心函数                                                     */
/* ------------------------------------------------------------------ */

static void vmem_disk_transfer(struct vmem_disk_dev *dev, sector_t sector,
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

static void vmem_disk_xfer_bio(struct vmem_disk_dev *dev, struct bio *bio)
{
struct bio_vec bvec;
struct bvec_iter iter;
sector_t sector = bio->bi_iter.bi_sector;

bio_for_each_segment(bvec, bio, iter) {
char *buffer = kmap_atomic(bvec.bv_page) + bvec.bv_offset;
unsigned int nsect = bvec.bv_len >> KERNEL_SECTOR_SHIFT;

vmem_disk_transfer(dev, sector, nsect, buffer,
   bio_data_dir(bio) == WRITE);
sector += nsect;
kunmap_atomic(buffer);
}
}

/* ------------------------------------------------------------------ */
/*  blk-mq 操作回调                                                     */
/* ------------------------------------------------------------------ */

static blk_status_t vmem_disk_queue_rq(struct blk_mq_hw_ctx *hctx,
const struct blk_mq_queue_data *bd)
{
struct request *req = bd->rq;
struct vmem_disk_dev *dev = req->q->queuedata;
struct bio *bio;
unsigned long flags;

blk_mq_start_request(req);

spin_lock_irqsave(&dev->lock, flags);
__rq_for_each_bio(bio, req)
vmem_disk_xfer_bio(dev, bio);
spin_unlock_irqrestore(&dev->lock, flags);

blk_mq_end_request(req, BLK_STS_OK);
return BLK_STS_OK;
}

static const struct blk_mq_ops vmem_disk_mq_ops = {
.queue_rq = vmem_disk_queue_rq,
};

/* ------------------------------------------------------------------ */
/*  block_device_operations                                             */
/* ------------------------------------------------------------------ */

static int vmem_disk_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
struct vmem_disk_dev *dev = bdev->bd_disk->private_data;
long size = dev->size / KERNEL_SECTOR_SIZE;

geo->heads     = 4;
geo->sectors   = 16;
geo->cylinders = size / (geo->heads * geo->sectors);
geo->start     = 0;
return 0;
}

static const struct block_device_operations vmem_disk_ops = {
.owner   = THIS_MODULE,
.getgeo  = vmem_disk_getgeo,
};

/* ------------------------------------------------------------------ */
/*  设备初始化与清理                                                     */
/* ------------------------------------------------------------------ */

static int setup_device(struct vmem_disk_dev *dev, int which)
{
int ret;

memset(dev, 0, sizeof(*dev));
dev->size = NSECTORS * HARDSECT_SIZE;

dev->data = vmalloc(dev->size);
if (!dev->data) {
pr_err("vmem_disk: vmalloc failure for device %d\n", which);
return -ENOMEM;
}
memset(dev->data, 0, dev->size);
spin_lock_init(&dev->lock);

dev->tag_set.ops          = &vmem_disk_mq_ops;
dev->tag_set.nr_hw_queues = 1;
dev->tag_set.queue_depth  = 128;
dev->tag_set.numa_node    = NUMA_NO_NODE;
dev->tag_set.cmd_size     = 0;
dev->tag_set.flags        = BLK_MQ_F_SHOULD_MERGE;

ret = blk_mq_alloc_tag_set(&dev->tag_set);
if (ret) {
pr_err("vmem_disk: blk_mq_alloc_tag_set failed: %d\n", ret);
goto out_vfree;
}

dev->gd = blk_mq_alloc_disk(&dev->tag_set, dev);
if (IS_ERR(dev->gd)) {
ret = PTR_ERR(dev->gd);
pr_err("vmem_disk: blk_mq_alloc_disk failed: %d\n", ret);
goto out_tag_set;
}

dev->gd->major        = vmem_disk_major;
dev->gd->first_minor  = which;
dev->gd->minors       = 1;
dev->gd->fops         = &vmem_disk_ops;
dev->gd->private_data = dev;
snprintf(dev->gd->disk_name, DISK_NAME_LEN, "vmem_disk%d", which);

blk_queue_logical_block_size(dev->gd->queue, HARDSECT_SIZE);
blk_queue_physical_block_size(dev->gd->queue, HARDSECT_SIZE);
set_capacity(dev->gd, NSECTORS * (HARDSECT_SIZE / KERNEL_SECTOR_SIZE));

ret = add_disk(dev->gd);
if (ret) {
pr_err("vmem_disk: add_disk failed: %d\n", ret);
goto out_disk;
}

pr_info("vmem_disk: device %s registered, size=%d KB\n",
dev->gd->disk_name, dev->size / 1024);
return 0;

out_disk:
put_disk(dev->gd);
out_tag_set:
blk_mq_free_tag_set(&dev->tag_set);
out_vfree:
vfree(dev->data);
dev->data = NULL;
return ret;
}

static void cleanup_device(struct vmem_disk_dev *dev)
{
if (dev->gd) {
del_gendisk(dev->gd);
put_disk(dev->gd);
blk_mq_free_tag_set(&dev->tag_set);
}
if (dev->data) {
vfree(dev->data);
dev->data = NULL;
}
}

/* ------------------------------------------------------------------ */
/*  模块入口 / 出口                                                     */
/* ------------------------------------------------------------------ */

static int __init vmem_disk_init(void)
{
int i, ret;

vmem_disk_major = register_blkdev(0, "vmem_disk");
if (vmem_disk_major <= 0) {
pr_err("vmem_disk: unable to get major number\n");
return -EBUSY;
}
pr_info("vmem_disk: registered with major number %d\n", vmem_disk_major);

devices = kcalloc(NDEVICES, sizeof(struct vmem_disk_dev), GFP_KERNEL);
if (!devices) {
ret = -ENOMEM;
goto out_unregister;
}

for (i = 0; i < NDEVICES; i++) {
ret = setup_device(devices + i, i);
if (ret) {
pr_err("vmem_disk: setup_device[%d] failed: %d\n", i, ret);
goto out_cleanup;
}
}

return 0;

out_cleanup:
while (--i >= 0)
cleanup_device(devices + i);
kfree(devices);
out_unregister:
unregister_blkdev(vmem_disk_major, "vmem_disk");
return ret;
}

static void __exit vmem_disk_exit(void)
{
int i;

for (i = 0; i < NDEVICES; i++)
cleanup_device(devices + i);

kfree(devices);
unregister_blkdev(vmem_disk_major, "vmem_disk");
pr_info("vmem_disk: module unloaded\n");
}

module_init(vmem_disk_init);
module_exit(vmem_disk_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Virtual memory-backed block device (blk-mq, no hardware required)");
