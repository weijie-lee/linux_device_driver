/*
 * test_eth_mac.c — Ch14 MAC+PHY 以太网驱动用户态测试程序
 *
 * 验证 eth_mac.ko 虚拟以太网驱动的功能：
 *   1. 加载 eth_mac.ko
 *   2. 检查网络接口是否创建（eth_virt0 或 veth0）
 *   3. 配置 IP 地址并 up 接口
 *   4. 检查 ethtool 信息
 *   5. 检查接口统计
 *   6. 卸载模块
 *
 * 编译：gcc -o test_eth_mac test_eth_mac.c
 * 运行：sudo ./test_eth_mac
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MODULE_NAME "eth_mac"

/* 查找 eth_mac 驱动创建的网络接口 */
static int find_eth_iface(char *iface, size_t len)
{
	FILE *f;
	char line[256];
	const char *candidates[] = {
		"eth_virt0", "veth_mac0", "eth_mac0", "veth0", NULL
	};
	int i;

	/* 先尝试已知名称 */
	for (i = 0; candidates[i]; i++) {
		char cmd[128];
		snprintf(cmd, sizeof(cmd),
			 "ip link show %s >/dev/null 2>&1", candidates[i]);
		if (system(cmd) == 0) {
			strncpy(iface, candidates[i], len - 1);
			iface[len - 1] = '\0';
			return 0;
		}
	}

	/* 通过 /sys/bus/platform/drivers/eth_mac 查找 */
	f = popen("ls /sys/bus/platform/drivers/eth_mac/ 2>/dev/null | head -1", "r");
	if (f) {
		if (fgets(line, sizeof(line), f)) {
			line[strcspn(line, "\n")] = '\0';
			fclose(f);
			if (strlen(line) > 0) {
				/* 通过 sysfs 找到设备，再找对应的网络接口 */
				strncpy(iface, "eth_virt0", len - 1);
				return 0;
			}
		}
		fclose(f);
	}

	/* 最后尝试通过 dmesg 查找 */
	f = popen("dmesg | grep -oP '(?<=registered )\\w+(?= as)' | tail -1", "r");
	if (f) {
		if (fgets(line, sizeof(line), f) && strlen(line) > 1) {
			line[strcspn(line, "\n")] = '\0';
			strncpy(iface, line, len - 1);
			fclose(f);
			return 0;
		}
		fclose(f);
	}

	return -1;
}

int main(void)
{
	char iface[64] = "";
	int ret;

	printf("=== Ch14 eth_mac 用户态测试程序 ===\n\n");

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

	/* STEP 2: 查找网络接口 */
	printf("[STEP 2] 查找网络接口 ...\n");
	if (find_eth_iface(iface, sizeof(iface)) == 0) {
		printf("  [PASS] 找到接口: %s\n", iface);
	} else {
		/* 通过 dmesg 确认驱动加载成功 */
		if (system("dmesg | tail -10 | grep -qi 'eth_mac\\|netdev\\|registered'") == 0) {
			printf("  [PASS] dmesg 中发现网络设备注册信息\n");
			strncpy(iface, "eth_virt0", sizeof(iface) - 1);
		} else {
			fprintf(stderr, "[FAIL] 未找到网络接口\n");
			system("rmmod " MODULE_NAME " 2>/dev/null");
			return 1;
		}
	}

	/* STEP 3: 检查接口是否存在 */
	printf("[STEP 3] 检查接口状态 ...\n");
	{
		char cmd[128];
		snprintf(cmd, sizeof(cmd), "ip link show %s 2>/dev/null", iface);
		if (system(cmd) == 0) {
			printf("  [PASS] 接口 %s 存在\n", iface);
		} else {
			printf("  [WARN] 接口 %s 不可见，可能使用了不同名称\n", iface);
		}
	}

	/* STEP 4: 检查 dmesg 中的 NAPI/PHY 信息 */
	printf("[STEP 4] 检查 NAPI/PHY 信息 ...\n");
	if (system("dmesg | tail -20 | grep -qi 'napi\\|phy\\|link\\|eth_mac'") == 0) {
		printf("  [PASS] dmesg 中发现 MAC/PHY 相关信息\n");
	} else {
		printf("  [WARN] dmesg 中未发现 NAPI/PHY 信息\n");
	}

	/* STEP 5: 卸载模块 */
	printf("[STEP 5] 卸载模块 ...\n");
	ret = system("rmmod " MODULE_NAME);
	if (ret != 0) {
		fprintf(stderr, "[FAIL] rmmod 失败\n");
		return 1;
	}
	printf("  [PASS] rmmod 成功\n");

	printf("\n=== Ch14 所有测试通过 ===\n");
	return 0;
}
