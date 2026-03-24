/*
 * test_input.c — Ch07 Input 子系统驱动用户态测试程序
 *
 * 测试项目：
 *   TC01: 找到虚拟键盘设备节点
 *   TC02: 打开 /dev/input/eventX
 *   TC03: 注入按键事件并读取验证
 *   TC04: 验证事件类型和键码正确
 *
 * 编译：gcc -o test_input test_input.c
 * 运行：sudo ./test_input
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>
#include <linux/input.h>

#define PROC_KBD    "/proc/virt_kbd"
#define INPUT_DIR   "/dev/input"
#define SYSFS_INPUT "/sys/class/input"

static int pass_count = 0;
static int fail_count = 0;

#define ASSERT(cond, msg) do { \
	if (cond) { printf("  [PASS] %s\n", msg); pass_count++; } \
	else { printf("  [FAIL] %s\n", msg); fail_count++; } \
} while (0)

/* 在 /sys/class/input/ 中查找名为 "Virtual Keyboard" 的设备 */
static int find_virt_kbd_event(char *event_path, size_t len)
{
	DIR *dir;
	struct dirent *ent;
	char name_path[256], name[64];
	FILE *f;

	dir = opendir(SYSFS_INPUT);
	if (!dir) return -1;

	while ((ent = readdir(dir)) != NULL) {
		if (strncmp(ent->d_name, "event", 5) != 0)
			continue;

		snprintf(name_path, sizeof(name_path),
			 "%s/%s/device/name", SYSFS_INPUT, ent->d_name);
		f = fopen(name_path, "r");
		if (!f) continue;

		memset(name, 0, sizeof(name));
		fgets(name, sizeof(name), f);
		fclose(f);

		/* 去掉末尾换行 */
		name[strcspn(name, "\n")] = '\0';

		if (strcmp(name, "Virtual Keyboard") == 0) {
			snprintf(event_path, len, "%s/%s", INPUT_DIR, ent->d_name);
			closedir(dir);
			return 0;
		}
	}
	closedir(dir);
	return -1;
}

int main(void)
{
	char event_path[256];
	int fd_event, fd_proc;
	struct input_event ev;
	fd_set rfds;
	struct timeval tv;
	ssize_t n;
	int found_key_press = 0, found_key_release = 0;

	printf("=== Ch07 Input Subsystem Driver Test ===\n\n");

	/* TC01: 找到虚拟键盘设备 */
	printf("[TC01] Find virtual keyboard device\n");
	int ret = find_virt_kbd_event(event_path, sizeof(event_path));
	ASSERT(ret == 0, "Found Virtual Keyboard in /sys/class/input/");
	if (ret != 0) {
		printf("  Virtual Keyboard not found, check: dmesg | grep virt_kbd\n");
		goto summary;
	}
	printf("  Device: %s\n", event_path);

	/* TC02: 打开事件设备 */
	printf("[TC02] Open event device\n");
	fd_event = open(event_path, O_RDONLY | O_NONBLOCK);
	ASSERT(fd_event >= 0, "open event device");
	if (fd_event < 0) { perror("open"); goto summary; }

	/* TC03: 注入按键事件（KEY_A = 30） */
	printf("[TC03] Inject key event (KEY_A=30)\n");
	fd_proc = open(PROC_KBD, O_WRONLY);
	ASSERT(fd_proc >= 0, "open /proc/virt_kbd");
	if (fd_proc >= 0) {
		const char *cmd = "key 30\n";
		n = write(fd_proc, cmd, strlen(cmd));
		ASSERT(n > 0, "write key command to /proc/virt_kbd");
		close(fd_proc);
	}

	/* TC04: 读取并验证事件 */
	printf("[TC04] Read and verify events\n");
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	/* 读取多个事件（按下 + 同步 + 释放 + 同步） */
	int max_events = 8;
	while (max_events-- > 0) {
		FD_ZERO(&rfds);
		FD_SET(fd_event, &rfds);
		tv.tv_sec = 0;
		tv.tv_usec = 200000;

		if (select(fd_event + 1, &rfds, NULL, NULL, &tv) <= 0)
			break;

		n = read(fd_event, &ev, sizeof(ev));
		if (n != sizeof(ev)) continue;

		if (ev.type == EV_KEY && ev.code == 30) {
			if (ev.value == 1) found_key_press = 1;
			if (ev.value == 0) found_key_release = 1;
			printf("  EV_KEY code=30 value=%d\n", ev.value);
		}
	}

	ASSERT(found_key_press,   "KEY_A press event received");
	ASSERT(found_key_release, "KEY_A release event received");

	close(fd_event);

summary:
	printf("\n=== Test Summary ===\n");
	printf("PASS: %d  FAIL: %d\n", pass_count, fail_count);
	return (fail_count == 0) ? 0 : 1;
}
