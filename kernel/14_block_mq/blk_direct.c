// SPDX-License-Identifier: GPL-2.0
/*
 * blk_direct.c — 对 /dev/vblk0 用 O_DIRECT 写一个扇区再读回。
 *
 * O_DIRECT 跳过页缓存，write 系统调用在返回前就把 bio 交给块层，
 * 跟踪窗口里因此能看到同一个进程从 submit_bio 一直走到驱动的 queue_rq。
 * O_DIRECT 要求缓冲区和长度按逻辑块大小对齐。
 */
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int pass_count, fail_count;

static void check(int cond, const char *what)
{
	printf("  [%s] %s\n", cond ? "PASS" : "FAIL", what);
	if (cond)
		pass_count++;
	else
		fail_count++;
}

int main(int argc, char **argv)
{
	const char *dev = argc > 1 ? argv[1] : "/dev/vblk0";
	char *wbuf, *rbuf;
	ssize_t n;
	int fd;

	if (posix_memalign((void **)&wbuf, 4096, 512) ||
	    posix_memalign((void **)&rbuf, 4096, 512))
		return 1;
	memset(wbuf, 0, 512);
	memset(rbuf, 0, 512);
	strcpy(wbuf, "ldd blk-mq sector 0");

	fd = open(dev, O_RDWR | O_DIRECT);
	check(fd >= 0, "open with O_DIRECT");
	if (fd < 0)
		return 1;

	n = pwrite(fd, wbuf, 512, 0);
	check(n == 512, "pwrite 512 bytes at sector 0");
	n = pread(fd, rbuf, 512, 0);
	check(n == 512, "pread 512 bytes at sector 0");
	check(memcmp(wbuf, rbuf, 512) == 0, "read back matches");
	printf("  read back: %s\n", rbuf);

	close(fd);
	free(wbuf);
	free(rbuf);
	printf("PASS: %d  FAIL: %d\n", pass_count, fail_count);
	return fail_count ? 1 : 0;
}
