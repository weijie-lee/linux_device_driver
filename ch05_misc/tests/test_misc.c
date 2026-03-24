/*
 * test_misc.c — Ch05 Misc 设备驱动用户态测试程序
 *
 * 测试项目：
 *   TC01: open/close 基本操作
 *   TC02: write 写入数据
 *   TC03: read 读取数据（验证与写入一致）
 *   TC04: ioctl MISC_GET_LEN 获取数据长度
 *   TC05: ioctl MISC_CLEAR 清空缓冲区
 *   TC06: 清空后 read 返回 0（EOF）
 *
 * 编译：gcc -o test_misc test_misc.c
 * 运行：sudo ./test_misc
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define DEVICE      "/dev/misc_demo"
#define MISC_MAGIC  'M'
#define MISC_CLEAR      _IO(MISC_MAGIC, 0)
#define MISC_GET_LEN    _IOR(MISC_MAGIC, 1, int)

/* 测试结果统计 */
static int pass_count = 0;
static int fail_count = 0;

#define ASSERT(cond, msg) do { \
	if (cond) { \
		printf("  [PASS] %s\n", msg); \
		pass_count++; \
	} else { \
		printf("  [FAIL] %s\n", msg); \
		fail_count++; \
	} \
} while (0)

int main(void)
{
	int fd;
	char wbuf[] = "Hello, Misc Device!";
	char rbuf[256];
	int len = 0;
	ssize_t n;

	printf("=== Ch05 Misc Device Driver Test ===\n\n");

	/* TC01: open */
	printf("[TC01] open/close\n");
	fd = open(DEVICE, O_RDWR);
	ASSERT(fd >= 0, "open /dev/misc_demo");
	if (fd < 0) {
		perror("open");
		return 1;
	}
	close(fd);
	fd = open(DEVICE, O_RDWR);

	/* TC02: write */
	printf("[TC02] write\n");
	n = write(fd, wbuf, strlen(wbuf));
	ASSERT(n == (ssize_t)strlen(wbuf), "write returns correct byte count");

	/* TC03: read */
	printf("[TC03] read\n");
	memset(rbuf, 0, sizeof(rbuf));
	lseek(fd, 0, SEEK_SET);
	n = read(fd, rbuf, sizeof(rbuf));
	ASSERT(n == (ssize_t)strlen(wbuf), "read returns same byte count as written");
	ASSERT(memcmp(rbuf, wbuf, strlen(wbuf)) == 0, "read data matches written data");

	/* TC04: ioctl MISC_GET_LEN */
	printf("[TC04] ioctl MISC_GET_LEN\n");
	len = 0;
	int ret = ioctl(fd, MISC_GET_LEN, &len);
	ASSERT(ret == 0, "ioctl MISC_GET_LEN returns 0");
	ASSERT(len == (int)strlen(wbuf), "MISC_GET_LEN returns correct length");
	printf("  data_len = %d (expected %zu)\n", len, strlen(wbuf));

	/* TC05: ioctl MISC_CLEAR */
	printf("[TC05] ioctl MISC_CLEAR\n");
	ret = ioctl(fd, MISC_CLEAR);
	ASSERT(ret == 0, "ioctl MISC_CLEAR returns 0");

	/* TC06: read after clear returns 0 */
	printf("[TC06] read after clear\n");
	lseek(fd, 0, SEEK_SET);
	n = read(fd, rbuf, sizeof(rbuf));
	ASSERT(n == 0, "read after clear returns 0 (EOF)");

	close(fd);

	/* 汇总 */
	printf("\n=== Test Summary ===\n");
	printf("PASS: %d  FAIL: %d\n", pass_count, fail_count);
	return (fail_count == 0) ? 0 : 1;
}
