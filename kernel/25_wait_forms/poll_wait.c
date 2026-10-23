// SPDX-License-Identifier: GPL-2.0
/*
 * poll_wait.c — 对 /dev/virt_wait 调两次 poll。
 *
 * 第一次没有数据，在 300 ms 时限上返回 0。
 * 第二次之前起一个子进程，200 ms 后写入 "data"。驱动在写里 wake_up，
 * 父进程从 poll 里醒来，revents 带 POLLIN。
 */
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <sys/wait.h>
#include <time.h>
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

static long now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int main(void)
{
	struct pollfd p;
	long t0, dt;
	pid_t pid;
	int fd, r;

	fd = open("/dev/virt_wait", O_RDWR);
	check(fd >= 0, "open /dev/virt_wait");
	if (fd < 0)
		return 1;
	p.fd = fd;
	p.events = POLLIN;

	t0 = now_ms();
	r = poll(&p, 1, 300);
	dt = now_ms() - t0;
	printf("  poll #1: ret=%d after %ld ms revents=0x%x\n", r, dt, p.revents);
	check(r == 0 && dt >= 300, "no data: poll times out");

	pid = fork();
	if (pid == 0) {
		int wfd = open("/dev/virt_wait", O_WRONLY);

		usleep(200000);
		if (wfd < 0 || write(wfd, "data", 4) != 4)
			_exit(1);
		_exit(0);
	}

	t0 = now_ms();
	r = poll(&p, 1, 3000);
	dt = now_ms() - t0;
	printf("  poll #2: ret=%d after %ld ms revents=0x%x\n", r, dt, p.revents);
	check(r == 1 && (p.revents & POLLIN), "data written: poll returns POLLIN");
	check(dt >= 150 && dt < 3000, "woken by the write, not by the timeout");

	waitpid(pid, NULL, 0);
	close(fd);
	printf("PASS: %d  FAIL: %d\n", pass_count, fail_count);
	return fail_count ? 1 : 0;
}
