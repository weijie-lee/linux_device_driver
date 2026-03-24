/*
 * test_rtc.c — Ch10 RTC 驱动用户态测试程序
 *
 * 测试项目：
 *   TC01: 找到虚拟 RTC 设备
 *   TC02: ioctl RTC_RD_TIME 读取时间
 *   TC03: ioctl RTC_SET_TIME 设置时间
 *   TC04: 读回验证时间设置
 *   TC05: ioctl RTC_ALM_SET + RTC_ALM_READ 闹钟读写
 *
 * 编译：gcc -o test_rtc test_rtc.c
 * 运行：sudo ./test_rtc
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>
#include <dirent.h>

static int pass_count = 0, fail_count = 0;
#define ASSERT(cond, msg) do { \
	if (cond) { printf("  [PASS] %s\n", msg); pass_count++; } \
	else { printf("  [FAIL] %s\n", msg); fail_count++; } \
} while (0)

/* 查找名为 "virt_rtc" 的 RTC 设备 */
static int find_virt_rtc(char *path, size_t len)
{
	DIR *dir = opendir("/sys/class/rtc");
	struct dirent *ent;
	char name_path[256], name[64];
	FILE *f;

	if (!dir) return -1;
	while ((ent = readdir(dir)) != NULL) {
		if (strncmp(ent->d_name, "rtc", 3) != 0) continue;
		snprintf(name_path, sizeof(name_path),
			 "/sys/class/rtc/%s/name", ent->d_name);
		f = fopen(name_path, "r");
		if (!f) continue;
		memset(name, 0, sizeof(name));
		fgets(name, sizeof(name), f);
		fclose(f);
		name[strcspn(name, "\n")] = '\0';
		if (strstr(name, "virt_rtc") || strstr(name, "virtual")) {
			snprintf(path, len, "/dev/%s", ent->d_name);
			closedir(dir);
			return 0;
		}
	}
	closedir(dir);
	/* 回退：使用最后一个 rtc 设备 */
	snprintf(path, len, "/dev/rtc1");
	return 0;
}

int main(void)
{
	char device[64];
	int fd, ret;
	struct rtc_time tm_set, tm_read;
	struct rtc_wkalrm alrm;

	printf("=== Ch10 RTC Driver Test ===\n\n");

	/* TC01: 找到设备 */
	printf("[TC01] Find virtual RTC device\n");
	find_virt_rtc(device, sizeof(device));
	printf("  Using device: %s\n", device);
	fd = open(device, O_RDWR);
	ASSERT(fd >= 0, "open RTC device");
	if (fd < 0) { perror("open"); return 1; }

	/* TC02: 读取时间 */
	printf("[TC02] RTC_RD_TIME\n");
	ret = ioctl(fd, RTC_RD_TIME, &tm_read);
	ASSERT(ret == 0, "ioctl RTC_RD_TIME returns 0");
	if (ret == 0) {
		printf("  Current time: %04d-%02d-%02d %02d:%02d:%02d\n",
		       tm_read.tm_year + 1900, tm_read.tm_mon + 1, tm_read.tm_mday,
		       tm_read.tm_hour, tm_read.tm_min, tm_read.tm_sec);
		ASSERT(tm_read.tm_year >= 100, "year >= 2000");
	}

	/* TC03: 设置时间 */
	printf("[TC03] RTC_SET_TIME\n");
	memset(&tm_set, 0, sizeof(tm_set));
	tm_set.tm_year = 124;  /* 2024 */
	tm_set.tm_mon  = 5;    /* June (0-indexed) */
	tm_set.tm_mday = 15;
	tm_set.tm_hour = 10;
	tm_set.tm_min  = 30;
	tm_set.tm_sec  = 0;
	ret = ioctl(fd, RTC_SET_TIME, &tm_set);
	ASSERT(ret == 0, "ioctl RTC_SET_TIME returns 0");

	/* TC04: 读回验证 */
	printf("[TC04] Read back time\n");
	ret = ioctl(fd, RTC_RD_TIME, &tm_read);
	ASSERT(ret == 0, "ioctl RTC_RD_TIME after set returns 0");
	if (ret == 0) {
		printf("  Time after set: %04d-%02d-%02d %02d:%02d:%02d\n",
		       tm_read.tm_year + 1900, tm_read.tm_mon + 1, tm_read.tm_mday,
		       tm_read.tm_hour, tm_read.tm_min, tm_read.tm_sec);
		ASSERT(tm_read.tm_year == 124, "year matches (2024)");
		ASSERT(tm_read.tm_mon == 5,   "month matches (June)");
	}

	/* TC05: 闹钟读写 */
	printf("[TC05] RTC_ALM_SET + RTC_ALM_READ\n");
	memset(&alrm, 0, sizeof(alrm));
	alrm.enabled = 0;  /* 不实际触发，只测试读写 */
	alrm.time.tm_hour = 11;
	alrm.time.tm_min  = 0;
	alrm.time.tm_sec  = 0;
	ret = ioctl(fd, RTC_ALM_SET, &alrm);
	ASSERT(ret == 0, "ioctl RTC_ALM_SET returns 0");

	memset(&alrm, 0, sizeof(alrm));
	ret = ioctl(fd, RTC_ALM_READ, &alrm);
	ASSERT(ret == 0, "ioctl RTC_ALM_READ returns 0");
	if (ret == 0) {
		printf("  Alarm time: %02d:%02d:%02d\n",
		       alrm.time.tm_hour, alrm.time.tm_min, alrm.time.tm_sec);
		ASSERT(alrm.time.tm_hour == 11, "alarm hour matches");
	}

	close(fd);
	printf("\n=== Test Summary ===\n");
	printf("PASS: %d  FAIL: %d\n", pass_count, fail_count);
	return (fail_count == 0) ? 0 : 1;
}
