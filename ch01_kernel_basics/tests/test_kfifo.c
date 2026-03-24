/*
 * test_kfifo.c — Ch01 内核基础工具用户态测试程序
 *
 * 本程序通过 insmod/rmmod 验证 kfifo_demo_static.ko 模块能正常加载和卸载，
 * 并通过 dmesg 检查内核日志确认 kfifo 操作输出。
 *
 * 编译：gcc -o test_kfifo test_kfifo.c
 * 运行：sudo ./test_kfifo
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MODULE_NAME "kfifo_demo_static"

static int run_cmd(const char *cmd)
{
	return system(cmd);
}

static int check_dmesg_keyword(const char *keyword)
{
	char cmd[256];
	snprintf(cmd, sizeof(cmd),
		 "dmesg | tail -20 | grep -qi '%s'", keyword);
	return system(cmd) == 0;
}

int main(void)
{
	int ret;

	printf("=== Ch01 kfifo 用户态测试程序 ===\n\n");

	/* 清理可能残留的模块 */
	run_cmd("rmmod " MODULE_NAME " 2>/dev/null");

	/* STEP 1: 加载模块 */
	printf("[STEP 1] 加载 %s.ko ...\n", MODULE_NAME);
	ret = run_cmd("insmod " MODULE_NAME ".ko");
	if (ret != 0) {
		fprintf(stderr, "[FAIL] insmod 失败 (ret=%d)\n", ret);
		return 1;
	}
	printf("  [PASS] insmod 成功\n");
	sleep(1);

	/* STEP 2: 检查 dmesg 中的 kfifo 输出 */
	printf("[STEP 2] 检查 dmesg 输出 ...\n");
	if (check_dmesg_keyword("kfifo") || check_dmesg_keyword("fifo") ||
	    check_dmesg_keyword("enqueue") || check_dmesg_keyword("dequeue")) {
		printf("  [PASS] dmesg 中发现 kfifo 相关输出\n");
	} else {
		printf("  [WARN] dmesg 中未发现 kfifo 关键字（可能已被刷新）\n");
	}

	/* STEP 3: 检查模块已加载 */
	printf("[STEP 3] 检查模块状态 ...\n");
	if (system("lsmod | grep -q " MODULE_NAME) == 0) {
		printf("  [PASS] 模块 %s 已加载\n", MODULE_NAME);
	} else {
		fprintf(stderr, "  [FAIL] 模块未在 lsmod 中找到\n");
		return 1;
	}

	/* STEP 4: 卸载模块 */
	printf("[STEP 4] 卸载模块 ...\n");
	ret = run_cmd("rmmod " MODULE_NAME);
	if (ret != 0) {
		fprintf(stderr, "[FAIL] rmmod 失败 (ret=%d)\n", ret);
		return 1;
	}
	printf("  [PASS] rmmod 成功\n");

	/* STEP 5: 确认模块已卸载 */
	printf("[STEP 5] 确认模块已卸载 ...\n");
	if (system("lsmod | grep -q " MODULE_NAME) != 0) {
		printf("  [PASS] 模块已从内核中移除\n");
	} else {
		fprintf(stderr, "  [FAIL] 模块仍然存在于 lsmod\n");
		return 1;
	}

	printf("\n=== Ch01 所有测试通过 ===\n");
	return 0;
}
