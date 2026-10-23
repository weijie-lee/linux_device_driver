// SPDX-License-Identifier: GPL-2.0
/*
 * fault_touch.c — mmap /dev/virt_fault，看驱动的 fault 回调在第一次访问时才被调用。
 *
 * 程序自己往 /sys/kernel/tracing/tracing_on 写 1 和 0，跟踪窗口只包住第一次读映射区。
 * 读映射区用内联的循环：如果调用 memcpy，memcpy 所在的代码页本身也可能缺页。
 */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define FAULTS "/sys/class/misc/virt_fault/faults"
#define TRACING_ON "/sys/kernel/tracing/tracing_on"

static int pass_count, fail_count;

static void check(int cond, const char *what)
{
	printf("  [%s] %s\n", cond ? "PASS" : "FAIL", what);
	if (cond)
		pass_count++;
	else
		fail_count++;
}

static long faults(void)
{
	FILE *f = fopen(FAULTS, "r");
	long v = -1;

	if (f) {
		if (fscanf(f, "%ld", &v) != 1)
			v = -1;
		fclose(f);
	}
	return v;
}

static void tracing(int on)
{
	int fd = open(TRACING_ON, O_WRONLY);

	if (fd < 0)
		return;
	if (write(fd, on ? "1" : "0", 1) != 1)
		perror("tracing_on");
	close(fd);
}

static void show_mapping(void)
{
	char line[256];
	FILE *f = fopen("/proc/self/maps", "r");

	if (!f)
		return;
	while (fgets(line, sizeof(line), f))
		if (strstr(line, "virt_fault"))
			printf("  maps: %s", line);
	fclose(f);
}

static void touch(const volatile char *p, char *out)
{
	int i;

	for (i = 0; i < 5; i++)
		out[i] = p[i];
	out[5] = '\0';
}

int main(void)
{
	char first[6], second[6];
	long before, after1, after2;
	const char *p;
	int fd;

	fd = open("/dev/virt_fault", O_RDONLY);
	check(fd >= 0, "open /dev/virt_fault");
	if (fd < 0)
		return 1;
	p = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0);
	check(p != MAP_FAILED, "mmap one page");
	if (p == MAP_FAILED)
		return 1;
	show_mapping();

	before = faults();
	printf("  faults after mmap: %ld\n", before);
	check(before == 0, "mmap alone does not fault");

	tracing(1);
	touch(p, first);
	tracing(0);
	after1 = faults();
	printf("  first read: %s faults=%ld\n", first, after1);
	check(after1 == 1, "first access calls the fault handler once");
	check(strcmp(first, "fault") == 0, "page holds the driver's bytes");

	touch(p, second);
	after2 = faults();
	printf("  second read: %s faults=%ld\n", second, after2);
	check(after2 == 1, "second access hits the installed PTE");

	munmap((void *)p, 4096);
	close(fd);
	printf("PASS: %d  FAIL: %d\n", pass_count, fail_count);
	return fail_count ? 1 : 0;
}
