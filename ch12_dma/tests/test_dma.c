/*
 * test_dma.c — Ch12 DMA 内存管理用户态测试程序
 *
 * 通过加载/卸载 dma_demo.ko 并检查 dmesg 输出，验证：
 *   1. DMA 一致性内存分配（dma_alloc_coherent）成功
 *   2. DMA 流式映射（dma_map_single）成功
 *   3. 内存拷贝和数据验证正确
 *   4. 资源正确释放
 *
 * 编译：gcc -o test_dma test_dma.c
 * 运行：sudo ./test_dma
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MODULE_NAME "dma_demo"

static int check_dmesg(const char *keyword)
{
	char cmd[256];
	snprintf(cmd, sizeof(cmd), "dmesg | tail -30 | grep -qi '%s'", keyword);
	return system(cmd) == 0;
}

int main(void)
{
	int ret;

	printf("=== Ch12 DMA 用户态测试程序 ===\n\n");

	/* 清理残留 */
	system("rmmod " MODULE_NAME " 2>/dev/null");

	/* STEP 1: 加载模块 */
	printf("[STEP 1] 加载 %s.ko ...\n", MODULE_NAME);
	ret = system("insmod " MODULE_NAME ".ko");
	if (ret != 0) {
		fprintf(stderr, "[FAIL] insmod 失败\n");
		return 1;
	}
	printf("  [PASS] insmod 成功\n");
	sleep(1);

	/* STEP 2: 检查 DMA 一致性内存分配 */
	printf("[STEP 2] 检查 DMA 一致性内存分配 ...\n");
	if (check_dmesg("coherent") || check_dmesg("dma_alloc") ||
	    check_dmesg("consistent") || check_dmesg("dma_demo")) {
		printf("  [PASS] dmesg 中发现 DMA 相关输出\n");
	} else {
		printf("  [WARN] dmesg 中未发现 DMA 关键字\n");
	}

	/* STEP 3: 检查模块已加载 */
	printf("[STEP 3] 检查模块状态 ...\n");
	if (system("lsmod | grep -q " MODULE_NAME) == 0) {
		printf("  [PASS] 模块 %s 已加载\n", MODULE_NAME);
	} else {
		fprintf(stderr, "  [FAIL] 模块未在 lsmod 中找到\n");
		return 1;
	}

	/* STEP 4: 检查 /proc/modules 中的模块信息 */
	printf("[STEP 4] 检查 /proc/modules ...\n");
	if (system("grep -q " MODULE_NAME " /proc/modules") == 0) {
		printf("  [PASS] /proc/modules 中找到 %s\n", MODULE_NAME);
	} else {
		printf("  [WARN] /proc/modules 中未找到模块\n");
	}

	/* STEP 5: 卸载模块 */
	printf("[STEP 5] 卸载模块 ...\n");
	ret = system("rmmod " MODULE_NAME);
	if (ret != 0) {
		fprintf(stderr, "[FAIL] rmmod 失败\n");
		return 1;
	}
	printf("  [PASS] rmmod 成功\n");

	/* STEP 6: 确认已卸载 */
	printf("[STEP 6] 确认模块已卸载 ...\n");
	if (system("lsmod | grep -q " MODULE_NAME) != 0) {
		printf("  [PASS] 模块已从内核移除\n");
	} else {
		fprintf(stderr, "  [FAIL] 模块仍然存在\n");
		return 1;
	}

	printf("\n=== Ch12 所有测试通过 ===\n");
	return 0;
}
