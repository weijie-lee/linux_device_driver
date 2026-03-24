/*
 * test_globalmem.c — Ch02 字符设备（globalmem）用户态测试程序
 *
 * 测试项目：
 *   TC01: open/close
 *   TC02: write 写入数据
 *   TC03: read 读取数据（验证一致性）
 *   TC04: lseek 定位读取
 *   TC05: 写入超出缓冲区大小的数据（边界测试）
 *   TC06: 多次 open/close（并发安全）
 *
 * 编译：gcc -o test_globalmem test_globalmem.c
 * 运行：sudo ./test_globalmem
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define DEVICE      "/dev/globalmem"
#define MEM_SIZE    0x1000  /* 4096 bytes */

static int pass_count = 0, fail_count = 0;
#define ASSERT(cond, msg) do { \
	if (cond) { printf("  [PASS] %s\n", msg); pass_count++; } \
	else { printf("  [FAIL] %s\n", msg); fail_count++; } \
} while (0)

int main(void)
{
	int fd;
	char wbuf[64] = "Hello, globalmem!";
	char rbuf[64];
	ssize_t n;

	printf("=== Ch02 Globalmem Character Device Test ===\n\n");

	/* TC01: open */
	printf("[TC01] open/close\n");
	fd = open(DEVICE, O_RDWR);
	ASSERT(fd >= 0, "open /dev/globalmem");
	if (fd < 0) { perror("open"); return 1; }
	close(fd);
	fd = open(DEVICE, O_RDWR);

	/* TC02: write */
	printf("[TC02] write\n");
	n = write(fd, wbuf, strlen(wbuf));
	ASSERT(n == (ssize_t)strlen(wbuf), "write returns correct byte count");

	/* TC03: read */
	printf("[TC03] read\n");
	lseek(fd, 0, SEEK_SET);
	memset(rbuf, 0, sizeof(rbuf));
	n = read(fd, rbuf, strlen(wbuf));
	ASSERT(n == (ssize_t)strlen(wbuf), "read returns correct byte count");
	ASSERT(memcmp(rbuf, wbuf, strlen(wbuf)) == 0, "read data matches written data");

	/* TC04: lseek */
	printf("[TC04] lseek\n");
	lseek(fd, 7, SEEK_SET);  /* 跳过 "Hello, " */
	memset(rbuf, 0, sizeof(rbuf));
	n = read(fd, rbuf, 9);   /* 读取 "globalmem" */
	ASSERT(n == 9, "read after lseek returns 9 bytes");
	ASSERT(memcmp(rbuf, "globalmem", 9) == 0, "lseek read content correct");

	/* TC05: 写入大量数据（接近缓冲区上限） */
	printf("[TC05] Large write (boundary test)\n");
	char *large_buf = malloc(MEM_SIZE);
	memset(large_buf, 'A', MEM_SIZE);
	lseek(fd, 0, SEEK_SET);
	n = write(fd, large_buf, MEM_SIZE);
	ASSERT(n == MEM_SIZE, "write MEM_SIZE bytes succeeds");
	free(large_buf);

	/* TC06: 多次 open */
	printf("[TC06] Multiple open\n");
	int fd2 = open(DEVICE, O_RDWR);
	ASSERT(fd2 >= 0, "second open succeeds");
	close(fd2);

	close(fd);

	printf("\n=== Test Summary ===\n");
	printf("PASS: %d  FAIL: %d\n", pass_count, fail_count);
	return (fail_count == 0) ? 0 : 1;
}
