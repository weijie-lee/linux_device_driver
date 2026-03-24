/*
 * test_platform.c — Ch06 Platform 设备驱动用户态测试程序
 *
 * 测试项目：
 *   TC01: open/close
 *   TC02: write 写入数据
 *   TC03: read 读取数据（验证一致性）
 *   TC04: sysfs 接口验证（通过 shell 脚本完成）
 *
 * 编译：gcc -o test_platform test_platform.c
 * 运行：sudo ./test_platform
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define DEVICE  "/dev/virt_plat"

static int pass_count = 0;
static int fail_count = 0;

#define ASSERT(cond, msg) do { \
	if (cond) { printf("  [PASS] %s\n", msg); pass_count++; } \
	else { printf("  [FAIL] %s\n", msg); fail_count++; } \
} while (0)

int main(void)
{
	int fd;
	char wbuf[] = "Platform Device Test Data";
	char rbuf[256];
	ssize_t n;

	printf("=== Ch06 Platform Device Driver Test ===\n\n");

	/* TC01: open */
	printf("[TC01] open/close\n");
	fd = open(DEVICE, O_RDWR);
	ASSERT(fd >= 0, "open /dev/virt_plat");
	if (fd < 0) { perror("open"); return 1; }

	/* TC02: write */
	printf("[TC02] write\n");
	n = write(fd, wbuf, strlen(wbuf));
	ASSERT(n == (ssize_t)strlen(wbuf), "write returns correct byte count");

	/* TC03: read */
	printf("[TC03] read\n");
	memset(rbuf, 0, sizeof(rbuf));
	lseek(fd, 0, SEEK_SET);
	n = read(fd, rbuf, sizeof(rbuf));
	ASSERT(n == (ssize_t)strlen(wbuf), "read returns same byte count");
	ASSERT(memcmp(rbuf, wbuf, strlen(wbuf)) == 0, "read data matches written data");

	close(fd);

	printf("\n=== Test Summary ===\n");
	printf("PASS: %d  FAIL: %d\n", pass_count, fail_count);
	return (fail_count == 0) ? 0 : 1;
}
