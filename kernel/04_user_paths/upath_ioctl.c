// SPDX-License-Identifier: GPL-2.0
/*
 * upath_ioctl.c — /dev/virt_upath 的 ioctl 检查。同一份源码编成 64 位和 32 位两个程序。
 *
 * 先以 root 打开设备、读值、写值，再把 uid 换成 65534，用已经打开的 fd 再写一次。
 * 打开文件的权限检查在 open 时做完了，第二次写失败只能来自驱动里的 capable()。
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "virt_upath.h"

static int pass_count, fail_count;

static void check(int cond, const char *what)
{
	printf("  [%s] %s\n", cond ? "PASS" : "FAIL", what);
	if (cond)
		pass_count++;
	else
		fail_count++;
}

static void show_caps(void)
{
	char line[128];
	FILE *f = fopen("/proc/self/status", "r");

	if (!f)
		return;
	while (fgets(line, sizeof(line), f))
		if (!strncmp(line, "Uid:", 4) || !strncmp(line, "CapEff:", 7))
			printf("  %s", line);
	fclose(f);
}

int main(void)
{
	struct upath_info info;
	__u32 v = 7;
	int fd, ret;

	printf("abi: sizeof(long)=%zu sizeof(struct upath_info)=%zu UPATH_GET=0x%lx\n",
	       sizeof(long), sizeof(info), (unsigned long)UPATH_GET);

	fd = open("/dev/virt_upath", O_RDWR);
	check(fd >= 0, "open /dev/virt_upath as root");
	if (fd < 0)
		return 1;

	ret = ioctl(fd, UPATH_SET, &v);
	check(ret == 0, "UPATH_SET 7 as root");

	memset(&info, 0xff, sizeof(info));
	ret = ioctl(fd, UPATH_GET, &info);
	check(ret == 0 && info.value == 7, "UPATH_GET returns 7");
	printf("  value=%u calls=%u\n", info.value, info.calls);

	if (setresgid(65534, 65534, 65534) || setresuid(65534, 65534, 65534)) {
		perror("setresuid");
		return 1;
	}
	show_caps();

	v = 9;
	errno = 0;
	ret = ioctl(fd, UPATH_SET, &v);
	printf("  UPATH_SET 9 as uid 65534: ret=%d errno=%s\n", ret, strerror(errno));
	check(ret == -1 && errno == EPERM, "UPATH_SET without CAP_SYS_ADMIN is EPERM");

	ret = ioctl(fd, UPATH_GET, &info);
	check(ret == 0 && info.value == 7, "UPATH_GET still 7");

	close(fd);
	printf("PASS: %d  FAIL: %d\n", pass_count, fail_count);
	return fail_count ? 1 : 0;
}
