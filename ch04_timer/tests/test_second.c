/*
 * test_second.c — Ch04 内核定时器（second）用户态测试程序
 *
 * 测试项目：
 *   TC01: open /dev/second
 *   TC02: read 读取已运行秒数（初始为 0）
 *   TC03: 等待 2 秒后再次 read，验证计数增加
 *   TC04: 多次 read 验证单调递增
 *
 * 编译：gcc -o test_second test_second.c
 * 运行：sudo ./test_second
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define DEVICE "/dev/second"

static int pass_count = 0, fail_count = 0;
#define ASSERT(cond, msg) do { \
	if (cond) { printf("  [PASS] %s\n", msg); pass_count++; } \
	else { printf("  [FAIL] %s\n", msg); fail_count++; } \
} while (0)

int main(void)
{
	int fd;
	int seconds1 = 0, seconds2 = 0, seconds3 = 0;
	ssize_t n;

	printf("=== Ch04 Second Timer Driver Test ===\n\n");

	printf("[TC01] open /dev/second\n");
	fd = open(DEVICE, O_RDONLY);
	ASSERT(fd >= 0, "open /dev/second");
	if (fd < 0) { perror("open"); return 1; }

	printf("[TC02] read initial seconds\n");
	n = read(fd, &seconds1, sizeof(int));
	ASSERT(n == sizeof(int), "read returns sizeof(int) bytes");
	printf("  seconds = %d\n", seconds1);
	ASSERT(seconds1 >= 0, "initial seconds >= 0");

	printf("[TC03] wait 2 seconds and read again\n");
	sleep(2);
	n = read(fd, &seconds2, sizeof(int));
	ASSERT(n == sizeof(int), "second read returns sizeof(int) bytes");
	printf("  seconds after 2s sleep = %d\n", seconds2);
	ASSERT(seconds2 >= seconds1 + 1, "seconds increased after sleep");

	printf("[TC04] read again immediately\n");
	n = read(fd, &seconds3, sizeof(int));
	ASSERT(n == sizeof(int), "third read returns sizeof(int) bytes");
	printf("  seconds (immediate) = %d\n", seconds3);
	ASSERT(seconds3 >= seconds2, "seconds monotonically non-decreasing");

	close(fd);
	printf("\n=== Test Summary ===\n");
	printf("PASS: %d  FAIL: %d\n", pass_count, fail_count);
	return (fail_count == 0) ? 0 : 1;
}
