// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <crypto/internal/skcipher.h>
#include <linux/scatterlist.h>

static int virt_setkey(struct crypto_skcipher *tfm, const u8 *key, unsigned int len)
{
	return 0;
}
static int virt_crypt(struct skcipher_request *req)
{
	struct skcipher_walk walk;
	int ret;

	ret = skcipher_walk_virt(&walk, req, false);
	while (walk.nbytes) {
		memcpy(walk.dst.virt.addr, walk.src.virt.addr, walk.nbytes);
		ret = skcipher_walk_done(&walk, 0);
		if (ret)
			break;
	}
	return ret;
}
static struct skcipher_alg virt_alg = {
	.base.cra_name = "virt-null",
	.base.cra_driver_name = "virt-null-generic",
	.base.cra_priority = 100,
	.base.cra_blocksize = 1,
	.base.cra_ctxsize = 0,
	.base.cra_module = THIS_MODULE,
	.min_keysize = 1,
	.max_keysize = 32,
	.setkey = virt_setkey,
	.encrypt = virt_crypt,
	.decrypt = virt_crypt,
};
static int __init virt_init(void)
{
	return crypto_register_skcipher(&virt_alg);
}
static void __exit virt_exit(void)
{
	crypto_unregister_skcipher(&virt_alg);
}
module_init(virt_init);
module_exit(virt_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("liweijie");
MODULE_DESCRIPTION("Virtual skcipher that copies input to output");
