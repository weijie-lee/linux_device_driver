/* SPDX-License-Identifier: GPL-2.0 */
/* 用户态检查：read/write、ioctl、poll、mmap 与 sysfs/debugfs 看到同一块缓冲。 */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "virt_uapi_cmd.h"

static void fail(const char *what)
{
	fprintf(stderr, "uapi FAIL: %s: %s\n", what, strerror(errno));
	exit(1);
}

static void expect(int cond, const char *what)
{
	if (!cond) {
		fprintf(stderr, "uapi FAIL: %s\n", what);
		exit(1);
	}
	printf("uapi ok: %s\n", what);
}

static unsigned read_u32_file(const char *path)
{
	char buf[32];
	int fd;
	ssize_t n;
	unsigned v;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		fail(path);
	n = read(fd, buf, sizeof(buf) - 1);
	if (n < 0)
		fail(path);
	buf[n] = '\0';
	close(fd);
	v = (unsigned)strtoul(buf, NULL, 0);
	return v;
}

int main(void)
{
	const char payload[] = "hello-uapi";
	char got[32];
	int fd, pr;
	uint32_t len = 0;
	struct pollfd pfd;
	void *map;
	ssize_t n;

	fd = open("/dev/virt_uapi", O_RDWR);
	if (fd < 0)
		fail("open /dev/virt_uapi");

	n = write(fd, payload, sizeof(payload) - 1);
	expect(n == (ssize_t)(sizeof(payload) - 1), "write");

	if (lseek(fd, 0, SEEK_SET) != 0)
		fail("lseek");
	n = read(fd, got, sizeof(got));
	expect(n == (ssize_t)(sizeof(payload) - 1), "read length");
	got[n] = '\0';
	expect(strcmp(got, payload) == 0, "read data");

	if (ioctl(fd, VIRT_UAPI_GET_LEN, &len) < 0)
		fail("ioctl GET_LEN");
	expect(len == sizeof(payload) - 1, "ioctl length");
	expect(read_u32_file("/sys/class/misc/virt_uapi/bytes") == len,
	       "sysfs bytes");
	expect(read_u32_file("/sys/kernel/debug/virt_uapi/flags") == 1,
	       "debugfs flags");

	map = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (map == MAP_FAILED)
		fail("mmap");
	expect(memcmp(map, payload, sizeof(payload) - 1) == 0, "mmap read");
	((char *)map)[0] = 'H';
	if (lseek(fd, 0, SEEK_SET) != 0)
		fail("lseek2");
	n = read(fd, got, sizeof(payload) - 1);
	expect(n == (ssize_t)(sizeof(payload) - 1), "read after mmap store");
	got[n] = '\0';
	expect(strcmp(got, "Hello-uapi") == 0, "mmap store visible to read");

	pfd.fd = fd;
	pfd.events = POLLIN;
	pr = poll(&pfd, 1, 0);
	expect(pr == 1 && (pfd.revents & POLLIN), "poll readable");

	if (ioctl(fd, VIRT_UAPI_CLEAR) < 0)
		fail("ioctl CLEAR");
	pfd.events = POLLIN;
	pr = poll(&pfd, 1, 200);
	expect(pr == 0, "poll idle after clear");
	expect(read_u32_file("/sys/class/misc/virt_uapi/bytes") == 0,
	       "sysfs bytes cleared");

	munmap(map, 4096);
	close(fd);
	printf("uapi ok: all\n");
	return 0;
}
