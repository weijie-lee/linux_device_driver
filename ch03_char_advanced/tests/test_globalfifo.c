/*
 * test_globalfifo.c — Ch03 字符设备进阶（globalfifo）用户态测试程序
 *
 * 测试项目：
 *   TC01: 基本 read/write
 *   TC02: FIFO 满时 write 阻塞（非阻塞模式返回 EAGAIN）
 *   TC03: FIFO 空时 read 阻塞（非阻塞模式返回 EAGAIN）
 *   TC04: poll/select 可读可写状态检测
 *   TC05: FIFO 清空 ioctl
 *
 * 编译：gcc -o test_globalfifo test_globalfifo.c
 * 运行：sudo ./test_globalfifo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/ioctl.h>

#define DEVICE      "/dev/globalfifo"
#define FIFO_SIZE   0x1000  /* 4096 bytes */
#define FIFO_CLEAR  0x1     /* ioctl 清空命令 */

static int pass_count = 0, fail_count = 0;
#define ASSERT(cond, msg) do { \
	if (cond) { printf("  [PASS] %s\n", msg); pass_count++; } \
	else { printf("  [FAIL] %s\n", msg); fail_count++; } \
} while (0)

int main(void)
{
	int fd, fd_nb;
	char wbuf[64] = "FIFO test data";
	char rbuf[64];
	ssize_t n;
	fd_set rfds, wfds;
	struct timeval tv;

	printf("=== Ch03 Globalfifo Character Device Test ===\n\n");

	fd    = open(DEVICE, O_RDWR);
	fd_nb = open(DEVICE, O_RDWR | O_NONBLOCK);
	ASSERT(fd >= 0 && fd_nb >= 0, "open /dev/globalfifo (blocking + nonblocking)");
	if (fd < 0) { perror("open"); return 1; }

	/* TC01: 基本 read/write */
	printf("[TC01] Basic read/write\n");
	n = write(fd, wbuf, strlen(wbuf));
	ASSERT(n == (ssize_t)strlen(wbuf), "write returns correct byte count");
	memset(rbuf, 0, sizeof(rbuf));
	n = read(fd, rbuf, strlen(wbuf));
	ASSERT(n == (ssize_t)strlen(wbuf), "read returns correct byte count");
	ASSERT(memcmp(rbuf, wbuf, strlen(wbuf)) == 0, "read data matches written data");

	/* TC02: 非阻塞 read 空 FIFO 返回 EAGAIN */
	printf("[TC02] Non-blocking read on empty FIFO\n");
	n = read(fd_nb, rbuf, sizeof(rbuf));
	ASSERT(n < 0 && errno == EAGAIN, "non-blocking read on empty FIFO returns EAGAIN");

	/* TC03: poll 可写检测 */
	printf("[TC03] poll writable\n");
	FD_ZERO(&wfds);
	FD_SET(fd, &wfds);
	tv.tv_sec = 1; tv.tv_usec = 0;
	int ret = select(fd + 1, NULL, &wfds, NULL, &tv);
	ASSERT(ret > 0 && FD_ISSET(fd, &wfds), "poll reports writable when FIFO has space");

	/* TC04: 写入数据后 poll 可读检测 */
	printf("[TC04] poll readable after write\n");
	write(fd, wbuf, strlen(wbuf));
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	tv.tv_sec = 1; tv.tv_usec = 0;
	ret = select(fd + 1, &rfds, NULL, NULL, &tv);
	ASSERT(ret > 0 && FD_ISSET(fd, &rfds), "poll reports readable after write");

	/* TC05: ioctl 清空 FIFO */
	printf("[TC05] ioctl FIFO_CLEAR\n");
	ret = ioctl(fd, FIFO_CLEAR);
	ASSERT(ret == 0, "ioctl FIFO_CLEAR returns 0");
	/* 清空后非阻塞读应返回 EAGAIN */
	n = read(fd_nb, rbuf, sizeof(rbuf));
	ASSERT(n < 0 && errno == EAGAIN, "after clear, non-blocking read returns EAGAIN");

	close(fd);
	close(fd_nb);

	printf("\n=== Test Summary ===\n");
	printf("PASS: %d  FAIL: %d\n", pass_count, fail_count);
	return (fail_count == 0) ? 0 : 1;
}
