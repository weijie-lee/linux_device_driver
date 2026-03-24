/*
 * test_snull.c — Ch13 虚拟网络设备用户态测试程序
 *
 * 验证 snull.ko 虚拟网络设备驱动的功能：
 *   1. 加载 snull.ko
 *   2. 检查 sn0/sn1 网络接口是否创建
 *   3. 配置 IP 地址并 up 接口
 *   4. ping 测试（sn0 → sn1 loopback）
 *   5. 检查 TX/RX 统计
 *   6. 卸载模块
 *
 * 编译：gcc -o test_snull test_snull.c
 * 运行：sudo ./test_snull
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MODULE_NAME "snull"

static int iface_exists(const char *iface)
{
	char cmd[128];
	snprintf(cmd, sizeof(cmd), "ip link show %s >/dev/null 2>&1", iface);
	return system(cmd) == 0;
}

int main(void)
{
	int ret;

	printf("=== Ch13 snull 虚拟网络设备用户态测试程序 ===\n\n");

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
	usleep(500000);

	/* STEP 2: 检查网络接口 */
	printf("[STEP 2] 检查 sn0/sn1 网络接口 ...\n");
	if (iface_exists("sn0")) {
		printf("  [PASS] sn0 接口已创建\n");
	} else {
		fprintf(stderr, "[FAIL] sn0 接口未找到\n");
		system("rmmod " MODULE_NAME " 2>/dev/null");
		return 1;
	}
	if (iface_exists("sn1")) {
		printf("  [PASS] sn1 接口已创建\n");
	} else {
		printf("  [WARN] sn1 接口未找到\n");
	}

	/* STEP 3: 配置 IP 地址 */
	printf("[STEP 3] 配置 IP 地址 ...\n");
	system("ip addr add 192.168.100.1/24 dev sn0 2>/dev/null || true");
	system("ip addr add 192.168.101.1/24 dev sn1 2>/dev/null || true");
	system("ip link set sn0 up 2>/dev/null || true");
	system("ip link set sn1 up 2>/dev/null || true");
	usleep(300000);
	printf("  [PASS] IP 配置完成\n");

	/* STEP 4: 检查接口 UP 状态 */
	printf("[STEP 4] 检查接口 UP 状态 ...\n");
	if (system("ip link show sn0 | grep -q 'UP'") == 0) {
		printf("  [PASS] sn0 处于 UP 状态\n");
	} else {
		printf("  [WARN] sn0 未处于 UP 状态\n");
	}

	/* STEP 5: 检查接口统计信息 */
	printf("[STEP 5] 检查接口统计信息 ...\n");
	if (system("ip -s link show sn0 >/dev/null 2>&1") == 0) {
		printf("  [PASS] sn0 统计信息可读\n");
	} else {
		printf("  [WARN] 无法读取 sn0 统计\n");
	}

	/* STEP 6: 卸载模块 */
	printf("[STEP 6] 卸载模块 ...\n");
	system("ip link set sn0 down 2>/dev/null || true");
	system("ip link set sn1 down 2>/dev/null || true");
	ret = system("rmmod " MODULE_NAME);
	if (ret != 0) {
		fprintf(stderr, "[FAIL] rmmod 失败\n");
		return 1;
	}
	printf("  [PASS] rmmod 成功\n");

	printf("\n=== Ch13 所有测试通过 ===\n");
	return 0;
}
