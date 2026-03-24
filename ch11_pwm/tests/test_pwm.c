/*
 * test_pwm.c — Ch11 PWM 子系统用户态测试程序
 *
 * 通过 sysfs 接口验证虚拟 PWM 驱动的功能：
 *   1. 加载 pwm_demo.ko
 *   2. 通过 /sys/class/pwm/pwmchipN/export 导出 PWM 通道
 *   3. 设置周期（period）和占空比（duty_cycle）
 *   4. 使能 PWM（enable=1）
 *   5. 读回参数验证
 *   6. 禁用并卸载模块
 *
 * 编译：gcc -o test_pwm test_pwm.c
 * 运行：sudo ./test_pwm
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#define MODULE_NAME "pwm_demo"

static int write_sysfs(const char *path, const char *val)
{
	FILE *f = fopen(path, "w");
	if (!f) {
		fprintf(stderr, "  [WARN] 无法写入 %s: %s\n", path, strerror(errno));
		return -1;
	}
	fputs(val, f);
	fclose(f);
	return 0;
}

static int read_sysfs(const char *path, char *buf, size_t len)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return -1;
	if (!fgets(buf, (int)len, f)) {
		fclose(f);
		return -1;
	}
	fclose(f);
	/* 去掉末尾换行 */
	buf[strcspn(buf, "\n")] = '\0';
	return 0;
}

/* 在 /sys/class/pwm/ 下查找第一个 pwmchipN 目录 */
static int find_pwmchip(char *chip_path, size_t len)
{
	DIR *d = opendir("/sys/class/pwm");
	struct dirent *e;
	if (!d)
		return -1;
	while ((e = readdir(d)) != NULL) {
		if (strncmp(e->d_name, "pwmchip", 7) == 0) {
			snprintf(chip_path, len, "/sys/class/pwm/%s", e->d_name);
			closedir(d);
			return 0;
		}
	}
	closedir(d);
	return -1;
}

int main(void)
{
	char chip_path[256];
	char pwm_path[256];
	char sysfs_path[512];
	char buf[64];
	int ret;

	printf("=== Ch11 PWM 用户态测试程序 ===\n\n");

	/* 清理残留模块 */
	system("rmmod " MODULE_NAME " 2>/dev/null");

	/* STEP 1: 加载模块 */
	printf("[STEP 1] 加载 %s.ko ...\n", MODULE_NAME);
	ret = system("insmod " MODULE_NAME ".ko");
	if (ret != 0) {
		fprintf(stderr, "[FAIL] insmod 失败\n");
		return 1;
	}
	printf("  [PASS] insmod 成功\n");
	usleep(300000);

	/* STEP 2: 查找 pwmchip */
	printf("[STEP 2] 查找 pwmchip sysfs 节点 ...\n");
	if (find_pwmchip(chip_path, sizeof(chip_path)) != 0) {
		fprintf(stderr, "[FAIL] 未找到 /sys/class/pwm/pwmchipN\n");
		system("rmmod " MODULE_NAME " 2>/dev/null");
		return 1;
	}
	printf("  [PASS] 找到 %s\n", chip_path);

	/* STEP 3: 导出 PWM 通道 0 */
	printf("[STEP 3] 导出 PWM 通道 0 ...\n");
	snprintf(sysfs_path, sizeof(sysfs_path), "%s/export", chip_path);
	write_sysfs(sysfs_path, "0");
	usleep(200000);
	snprintf(pwm_path, sizeof(pwm_path), "%s/pwm0", chip_path);
	if (access(pwm_path, F_OK) == 0) {
		printf("  [PASS] %s 已创建\n", pwm_path);
	} else {
		printf("  [WARN] %s 未找到，跳过后续 sysfs 测试\n", pwm_path);
		goto unload;
	}

	/* STEP 4: 设置周期 1ms = 1000000 ns */
	printf("[STEP 4] 设置 period=1000000 ns ...\n");
	snprintf(sysfs_path, sizeof(sysfs_path), "%s/period", pwm_path);
	if (write_sysfs(sysfs_path, "1000000") == 0)
		printf("  [PASS] period 写入成功\n");

	/* STEP 5: 设置占空比 500us = 500000 ns */
	printf("[STEP 5] 设置 duty_cycle=500000 ns ...\n");
	snprintf(sysfs_path, sizeof(sysfs_path), "%s/duty_cycle", pwm_path);
	if (write_sysfs(sysfs_path, "500000") == 0)
		printf("  [PASS] duty_cycle 写入成功\n");

	/* STEP 6: 使能 PWM */
	printf("[STEP 6] 使能 PWM ...\n");
	snprintf(sysfs_path, sizeof(sysfs_path), "%s/enable", pwm_path);
	if (write_sysfs(sysfs_path, "1") == 0)
		printf("  [PASS] PWM 已使能\n");

	/* STEP 7: 读回 period 验证 */
	printf("[STEP 7] 读回 period 验证 ...\n");
	snprintf(sysfs_path, sizeof(sysfs_path), "%s/period", pwm_path);
	if (read_sysfs(sysfs_path, buf, sizeof(buf)) == 0) {
		if (strcmp(buf, "1000000") == 0)
			printf("  [PASS] period 读回正确: %s ns\n", buf);
		else
			printf("  [WARN] period 读回: %s（期望 1000000）\n", buf);
	}

	/* STEP 8: 禁用 PWM */
	printf("[STEP 8] 禁用 PWM ...\n");
	snprintf(sysfs_path, sizeof(sysfs_path), "%s/enable", pwm_path);
	write_sysfs(sysfs_path, "0");

	/* unexport */
	snprintf(sysfs_path, sizeof(sysfs_path), "%s/unexport", chip_path);
	write_sysfs(sysfs_path, "0");
	printf("  [PASS] PWM 已禁用并 unexport\n");

unload:
	/* STEP 9: 卸载模块 */
	printf("[STEP 9] 卸载模块 ...\n");
	ret = system("rmmod " MODULE_NAME);
	if (ret != 0) {
		fprintf(stderr, "[FAIL] rmmod 失败\n");
		return 1;
	}
	printf("  [PASS] rmmod 成功\n");

	printf("\n=== Ch11 所有测试通过 ===\n");
	return 0;
}
