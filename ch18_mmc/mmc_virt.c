/*
 * mmc_virt.c - Virtual MMC/eMMC host controller driver
 *
 * Implements a complete MMC host controller driver that registers with the
 * Linux MMC subsystem. The "card" is backed by a vmalloc'd memory buffer
 * (default 64 MiB), making it possible to exercise the full MMC driver stack
 * — including mmc_block, partition tables, and filesystem operations — on any
 * Linux machine without an SD card or eMMC chip.
 *
 * How it works:
 *   1. mmc_alloc_host() allocates an mmc_host.
 *   2. The host's .request() callback intercepts every MMC command and
 *      translates CMD17/CMD18 (read) and CMD24/CMD25 (write) into memcpy
 *      operations on the backing buffer.
 *   3. mmc_add_host() triggers card detection; the MMC core sends CMD0/CMD2/
 *      CMD3/CMD7/CMD9/CMD13 to enumerate the card, which this driver answers
 *      with plausible responses.
 *   4. The MMC core then instantiates an mmc_card and mmc_block, which
 *      creates /dev/mmcblk0 — a fully functional block device.
 *
 * Verification path:
 *   sudo insmod mmc_virt.ko
 *   lsblk                          → shows mmcblk0 (64 MiB)
 *   sudo mkfs.ext4 /dev/mmcblk0    → format the virtual card
 *   sudo mount /dev/mmcblk0 /mnt   → mount and use like a real card
 *
 * Key concepts demonstrated:
 *   - mmc_alloc_host() / mmc_add_host() / mmc_remove_host()
 *   - struct mmc_host_ops.request() — the core command dispatch callback
 *   - MMC command set: CMD0, CMD2, CMD3, CMD7, CMD9, CMD13, CMD17/18, CMD24/25
 *   - mmc_request / mmc_command / mmc_data / mmc_host lifecycle
 *   - sg_copy_from_buffer / sg_copy_to_buffer for scatter-gather I/O
 */

#include <linux/module.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/card.h>
#include <linux/scatterlist.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

#define DRIVER_NAME       "mmc_virt"
#define CARD_SIZE_MB      64UL
#define CARD_SIZE_BYTES   (CARD_SIZE_MB * 1024 * 1024)
#define CARD_BLOCK_SIZE   512

/* Fake CID: Manufacturer=0x01 (Panasonic), OEM="VM", name="VRTMC" */
static const u32 fake_cid[4] = {
	0x01564d00,	/* [127:96] MID=0x01, OID="VM" */
	0x5652544d,	/* [95:64]  PNM="VRTMC" (bytes 4-5) */
	0x43010000,	/* [63:32]  PNM byte 6 + PRV + PSN */
	0x00000000,	/* [31:0]   MDT + CRC */
};

/* Fake CSD for a 64 MiB card (CSD version 1.0) */
static const u32 fake_csd[4] = {
	0x005e0032,
	0x5f5a83c8,
	0xffffff80,
	0x0a400000,
};

struct mmc_virt_host {
	struct mmc_host *mmc;
	void            *card_data;	/* vmalloc'd backing store */
	size_t           card_size;
	u16              rca;		/* Relative Card Address */
};

/* ------------------------------------------------------------------ */
/*  Scatter-gather helpers                                               */
/* ------------------------------------------------------------------ */

static void sg_copy_data(struct mmc_data *data, void *card_buf,
			 unsigned long offset, bool to_card)
{
	struct scatterlist *sg;
	unsigned int i;
	unsigned long pos = offset;

	for_each_sg(data->sg, sg, data->sg_len, i) {
		void *buf = sg_virt(sg);
		unsigned int len = sg->length;

		if (to_card)
			memcpy(card_buf + pos, buf, len);
		else
			memcpy(buf, card_buf + pos, len);

		pos += len;
	}
}

/* ------------------------------------------------------------------ */
/*  MMC command handler                                                  */
/* ------------------------------------------------------------------ */

static void handle_command(struct mmc_virt_host *priv, struct mmc_request *mrq)
{
	struct mmc_command *cmd  = mrq->cmd;
	struct mmc_data    *data = mrq->data;
	unsigned long offset;

	cmd->error = 0;

	switch (cmd->opcode) {

	case MMC_GO_IDLE_STATE:		/* CMD0 */
		break;

	case MMC_ALL_SEND_CID:		/* CMD2 */
		memcpy(cmd->resp, fake_cid, sizeof(fake_cid));
		break;

	case MMC_SET_RELATIVE_ADDR:	/* CMD3 */
		priv->rca = 1;
		cmd->resp[0] = priv->rca << 16;
		break;

	case MMC_SELECT_CARD:		/* CMD7 */
		cmd->resp[0] = R1_STATE_TRAN << 9;
		break;

	case MMC_SEND_CSD:		/* CMD9 */
		memcpy(cmd->resp, fake_csd, sizeof(fake_csd));
		break;

	case MMC_SEND_STATUS:		/* CMD13 */
		cmd->resp[0] = R1_STATE_TRAN << 9;
		break;

	case MMC_SET_BLOCKLEN:		/* CMD16 */
		cmd->resp[0] = R1_STATE_TRAN << 9;
		break;

	case MMC_READ_SINGLE_BLOCK:	/* CMD17 */
	case MMC_READ_MULTIPLE_BLOCK:	/* CMD18 */
		if (!data) {
			cmd->error = -EINVAL;
			break;
		}
		offset = (unsigned long)cmd->arg * CARD_BLOCK_SIZE;
		if (offset + data->blocks * CARD_BLOCK_SIZE > priv->card_size) {
			cmd->error = -ERANGE;
			break;
		}
		sg_copy_data(data, priv->card_data, offset, false);
		data->bytes_xfered = data->blocks * CARD_BLOCK_SIZE;
		cmd->resp[0] = R1_STATE_TRAN << 9;
		break;

	case MMC_WRITE_BLOCK:		/* CMD24 */
	case MMC_WRITE_MULTIPLE_BLOCK:	/* CMD25 */
		if (!data) {
			cmd->error = -EINVAL;
			break;
		}
		offset = (unsigned long)cmd->arg * CARD_BLOCK_SIZE;
		if (offset + data->blocks * CARD_BLOCK_SIZE > priv->card_size) {
			cmd->error = -ERANGE;
			break;
		}
		sg_copy_data(data, priv->card_data, offset, true);
		data->bytes_xfered = data->blocks * CARD_BLOCK_SIZE;
		cmd->resp[0] = R1_STATE_TRAN << 9;
		break;

	case MMC_STOP_TRANSMISSION:	/* CMD12 */
		cmd->resp[0] = R1_STATE_TRAN << 9;
		break;

	case MMC_SEND_OP_COND:		/* CMD1 — report card is ready */
		cmd->resp[0] = 0x80FF8000;	/* OCR: busy=0, 3.3V, sector mode */
		break;

	default:
		pr_debug("mmc_virt: unhandled CMD%u arg=0x%08x\n",
			 cmd->opcode, cmd->arg);
		cmd->resp[0] = R1_STATE_TRAN << 9;
		break;
	}

	if (mrq->stop)
		mrq->stop->error = 0;
}

/* ------------------------------------------------------------------ */
/*  mmc_host_ops                                                         */
/* ------------------------------------------------------------------ */

static void mmc_virt_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct mmc_virt_host *priv = mmc_priv(mmc);

	handle_command(priv, mrq);
	mmc_request_done(mmc, mrq);
}

static void mmc_virt_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	/* Accept any bus width / clock / power setting silently */
	pr_debug("mmc_virt: set_ios clock=%u bus_width=%u\n",
		 ios->clock, ios->bus_width);
}

static int mmc_virt_get_cd(struct mmc_host *mmc)
{
	return 1;	/* card always present */
}

static int mmc_virt_get_ro(struct mmc_host *mmc)
{
	return 0;	/* card is read-write */
}

static const struct mmc_host_ops mmc_virt_ops = {
	.request = mmc_virt_request,
	.set_ios = mmc_virt_set_ios,
	.get_cd  = mmc_virt_get_cd,
	.get_ro  = mmc_virt_get_ro,
};

/* ------------------------------------------------------------------ */
/*  Module init / exit                                                   */
/* ------------------------------------------------------------------ */

static struct mmc_virt_host *g_virt;

static struct platform_device *g_pdev;

static int __init mmc_virt_init(void)
{
	struct mmc_host      *mmc;
	struct mmc_virt_host *priv;
	int ret;

	/*
	 * mmc_alloc_host 需要一个有效的 struct device 指针（用于 devm_ 内存管理）。
	 * 创建一个虚拟 platform_device 作为 parent，避免传 NULL 导致的 NULL 指针解引用。
	 */
	g_pdev = platform_device_alloc(DRIVER_NAME, -1);
	if (!g_pdev)
		return -ENOMEM;
	ret = platform_device_add(g_pdev);
	if (ret) {
		platform_device_put(g_pdev);
		return ret;
	}

	mmc = mmc_alloc_host(sizeof(*priv), &g_pdev->dev);
	if (!mmc) {
		platform_device_unregister(g_pdev);
		return -ENOMEM;
	}

	priv = mmc_priv(mmc);
	priv->mmc       = mmc;
	priv->card_size = CARD_SIZE_BYTES;

	priv->card_data = vzalloc(CARD_SIZE_BYTES);
	if (!priv->card_data) {
		ret = -ENOMEM;
		goto err_free_host;
	}

	/* Describe the host controller capabilities */
	mmc->ops         = &mmc_virt_ops;
	mmc->f_min       = 400000;		/* 400 kHz (identification) */
	mmc->f_max       = 52000000;		/* 52 MHz (high-speed MMC) */
	mmc->ocr_avail   = MMC_VDD_32_33 | MMC_VDD_33_34;
	mmc->caps        = MMC_CAP_4_BIT_DATA | MMC_CAP_MMC_HIGHSPEED;
	mmc->max_segs    = 128;
	mmc->max_seg_size = CARD_SIZE_BYTES;
	mmc->max_blk_size = CARD_BLOCK_SIZE;
	mmc->max_blk_count = CARD_SIZE_BYTES / CARD_BLOCK_SIZE;
	mmc->max_req_size  = CARD_SIZE_BYTES;

	g_virt = priv;

	ret = mmc_add_host(mmc);
	if (ret) {
		pr_err("mmc_virt: mmc_add_host failed: %d\n", ret);
		goto err_free_buf;
	}

	pr_info("mmc_virt: virtual MMC host registered, %lu MiB backing store\n",
		CARD_SIZE_MB);
	pr_info("mmc_virt: look for /dev/mmcblk0 after card detection completes\n");
	return 0;

err_free_buf:
	vfree(priv->card_data);
err_free_host:
	mmc_free_host(mmc);
	return ret;
}

static void __exit mmc_virt_exit(void)
{
	struct mmc_host *mmc = g_virt->mmc;

	mmc_remove_host(mmc);
	vfree(g_virt->card_data);
	mmc_free_host(mmc);
	platform_device_unregister(g_pdev);
	pr_info("mmc_virt: module unloaded\n");
}

module_init(mmc_virt_init);
module_exit(mmc_virt_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Virtual MMC/eMMC host controller driver backed by vmalloc memory (no hardware required)");
