// SPDX-License-Identifier: GPL-2.0
/*
 * virt_alsa.c — Virtual ALSA card
 * Virtual driver. Creates its own platform device so it can be
 * loaded on the QEMU test kernel without a board file.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>

#define DRV_NAME "virt_alsa"

static struct snd_pcm_hardware virt_hw = {
	.info = SNDRV_PCM_INFO_INTERLEAVED,
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rates = SNDRV_PCM_RATE_48000,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
	.buffer_bytes_max = 64 * 1024,
	.period_bytes_min = 256,
	.period_bytes_max = 32 * 1024,
	.periods_min = 1,
	.periods_max = 2,
};
static int virt_open(struct snd_pcm_substream *ss)
{
	ss->runtime->hw = virt_hw;
	return 0;
}
static int virt_close(struct snd_pcm_substream *ss) { return 0; }
static int virt_hw_params(struct snd_pcm_substream *ss,
			  struct snd_pcm_hw_params *p)
{
	return 0;
}
static int virt_prepare(struct snd_pcm_substream *ss) { return 0; }
static int virt_trigger(struct snd_pcm_substream *ss, int cmd) { return 0; }
static snd_pcm_uframes_t virt_pointer(struct snd_pcm_substream *ss)
{
	return 0;
}
static const struct snd_pcm_ops virt_pcm_ops = {
	.open = virt_open,
	.close = virt_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = virt_hw_params,
	.prepare = virt_prepare,
	.trigger = virt_trigger,
	.pointer = virt_pointer,
};
static int virt_probe(struct platform_device *pdev)
{
	struct snd_card *card;
	struct snd_pcm *pcm;
	int ret;

	ret = snd_card_new(&pdev->dev, -1, "virt", THIS_MODULE, 0, &card);
	if (ret)
		return ret;
	ret = snd_pcm_new(card, "virt PCM", 0, 1, 1, &pcm);
	if (ret)
		goto err;
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &virt_pcm_ops);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &virt_pcm_ops);
	strscpy(card->driver, "virt_alsa", sizeof(card->driver));
	strscpy(card->shortname, "virt", sizeof(card->shortname));
	strscpy(card->longname, "virtual sound card", sizeof(card->longname));
	ret = snd_card_register(card);
	if (ret)
		goto err;
	platform_set_drvdata(pdev, card);
	return 0;
err:
	snd_card_free(card);
	return ret;
}
static int virt_remove(struct platform_device *pdev)
{
	snd_card_free(platform_get_drvdata(pdev));
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
MODULE_DESCRIPTION("Virtual ALSA card");
