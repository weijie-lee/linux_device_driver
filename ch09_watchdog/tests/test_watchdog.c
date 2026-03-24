/*
 * test_watchdog.c — Ch09 Watchdog 驱动用户态测试程序
 *
 * 测试项目：
 *   TC01: 打开 /dev/watchdog（启动看门狗）
 *   TC02: ioctl WDIOC_GETSUPPORT 获取看门狗信息
 *   TC03: ioctl WDIOC_SETTIMEOUT 设置超时时间
 *   TC04: ioctl WDIOC_GETTIMELEFT 获取剩余时间
 *   TC05: 写入数据喂狗
 *   TC06: 写入 'V' 正常关闭看门狗
 *
 * 编译：gcc -o test_watchdog test_watchdog.c
 * 运行：sudo ./test_watchdog
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/watchdog.h>

/* 优先使用 /dev/watchdog0，回退到 /dev/watchdog */
#define DEVICE0 "/dev/watchdog0"
#define DEVICE1 "/dev/watchdog"

static int pass_count = 0, fail_count = 0;
#define ASSERT(cond, msg) do { \
	if (cond) { printf("  [PASS] %s\n", msg); pass_count++; } \
	else { printf("  [FAIL] %s\n", msg); fail_count++; } \
} while (0)

int main(void)
{
	int fd, ret;
	struct watchdog_info info;
	int new_timeout = 10, timeleft = 0;
	const char *device;

	printf("=== Ch09 Watchdog Driver Test ===\n\n");

	/* 选择设备节点 */
	device = (access(DEVICE0, F_OK) == 0) ? DEVICE0 : DEVICE1;
	printf("Using device: %s\n\n", device);

	/* TC01: 打开看门狗 */
	printf("[TC01] Open watchdog (starts timer)\n");
	fd = open(device, O_RDWR);
	ASSERT(fd >= 0, "open watchdog device");
	if (fd < 0) { perror("open"); return 1; }
	printf("  Watchdog started\n");

	/* TC02: 获取看门狗信息 */
	printf("[TC02] WDIOC_GETSUPPORT\n");
	ret = ioctl(fd, WDIOC_GETSUPPORT, &info);
	ASSERT(ret == 0, "ioctl WDIOC_GETSUPPORT returns 0");
	if (ret == 0) {
		printf("  identity: %s\n", info.identity);
		printf("  options:  0x%x\n", info.options);
		ASSERT(info.options & WDIOF_SETTIMEOUT, "supports SETTIMEOUT");
		ASSERT(info.options & WDIOF_KEEPALIVEPING, "supports KEEPALIVEPING");
	}

	/* TC03: 设置超时时间 */
	printf("[TC03] WDIOC_SETTIMEOUT (set to %d seconds)\n", new_timeout);
	ret = ioctl(fd, WDIOC_SETTIMEOUT, &new_timeout);
	ASSERT(ret == 0, "ioctl WDIOC_SETTIMEOUT returns 0");
	printf("  timeout set to: %d seconds\n", new_timeout);

	/* TC04: 获取剩余时间 */
	printf("[TC04] WDIOC_GETTIMELEFT\n");
	ret = ioctl(fd, WDIOC_GETTIMELEFT, &timeleft);
	ASSERT(ret == 0, "ioctl WDIOC_GETTIMELEFT returns 0");
	printf("  timeleft: %d seconds\n", timeleft);
	ASSERT(timeleft > 0 && timeleft <= new_timeout, "timeleft in valid range");

	/* TC05: 喂狗（写入任意数据） */
	printf("[TC05] Keepalive ping\n");
	ret = write(fd, "1", 1);
	ASSERT(ret == 1, "write ping returns 1");
	printf("  Watchdog fed\n");

	/* TC06: 正常关闭（写入 'V' 表示有意关闭） */
	printf("[TC06] Magic close (write 'V')\n");
	ret = write(fd, "V", 1);
	ASSERT(ret == 1, "write 'V' returns 1");
	close(fd);
	printf("  Watchdog closed normally\n");

	printf("\n=== Test Summary ===\n");
	printf("PASS: %d  FAIL: %d\n", pass_count, fail_count);
	return (fail_count == 0) ? 0 : 1;
}
