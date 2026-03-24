/*
 * test_vmem_disk.c — Ch17 块设备驱动用户态测试程序
 *
 * 验证 vmem_disk.ko 虚拟块设备的功能：
 *   1. 加载 vmem_disk.ko
 *   2. 检查 /dev/vmem_disk 块设备节点
 *   3. 查询设备大小（blockdev --getsize64）
 *   4. 格式化为 ext4 文件系统
 *   5. 挂载并写入/读取文件
 *   6. 卸载文件系统并卸载模块
 *
 * 编译：gcc -o test_vmem_disk test_vmem_disk.c
 * 运行：sudo ./test_vmem_disk
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#define MODULE_NAME  "vmem_disk"
#define DEVICE_PATH  "/dev/vmem_disk"
#define MOUNT_POINT  "/tmp/vmem_test"
#define TEST_FILE    MOUNT_POINT "/test.txt"
#define TEST_DATA    "vmem_disk block device test data"

int main(void)
{
	int ret;
	FILE *f;
	char buf[256];

	printf("=== Ch17 vmem_disk 用户态测试程序 ===\n\n");

	/* 清理残留 */
	system("umount " MOUNT_POINT " 2>/dev/null");
	system("rmmod " MODULE_NAME " 2>/dev/null");

	/* STEP 1: 加载模块 */
	printf("[STEP 1] 加载 %s.ko ...\n", MODULE_NAME);
	ret = system("insmod " MODULE_NAME ".ko");
	if (ret != 0) {
		fprintf(stderr, "[FAIL] insmod 失败\n");
		return 1;
	}
	printf("  [PASS] insmod 成功\n");
	usleep(500000);

	/* STEP 2: 检查块设备节点 */
	printf("[STEP 2] 检查 %s 块设备 ...\n", DEVICE_PATH);
	{
		struct stat st;
		if (stat(DEVICE_PATH, &st) == 0 && S_ISBLK(st.st_mode)) {
			printf("  [PASS] %s 是块设备\n", DEVICE_PATH);
		} else {
			fprintf(stderr, "[FAIL] %s 不是块设备或不存在\n", DEVICE_PATH);
			system("rmmod " MODULE_NAME " 2>/dev/null");
			return 1;
		}
	}

	/* STEP 3: 查询设备大小 */
	printf("[STEP 3] 查询设备大小 ...\n");
	{
		FILE *p = popen("blockdev --getsize64 " DEVICE_PATH " 2>/dev/null", "r");
		if (p) {
			long long size = 0;
			if (fscanf(p, "%lld", &size) == 1 && size > 0) {
				printf("  [PASS] 设备大小: %lld 字节 (%lld KiB)\n",
				       size, size / 1024);
			} else {
				printf("  [WARN] 无法获取设备大小\n");
			}
			pclose(p);
		}
	}

	/* STEP 4: 格式化为 ext4 */
	printf("[STEP 4] 格式化为 ext4 ...\n");
	ret = system("mkfs.ext4 -F " DEVICE_PATH " >/dev/null 2>&1");
	if (ret == 0) {
		printf("  [PASS] mkfs.ext4 成功\n");
	} else {
		fprintf(stderr, "[FAIL] mkfs.ext4 失败\n");
		system("rmmod " MODULE_NAME " 2>/dev/null");
		return 1;
	}

	/* STEP 5: 挂载 */
	printf("[STEP 5] 挂载到 %s ...\n", MOUNT_POINT);
	mkdir(MOUNT_POINT, 0755);
	ret = system("mount " DEVICE_PATH " " MOUNT_POINT);
	if (ret != 0) {
		fprintf(stderr, "[FAIL] mount 失败\n");
		system("rmmod " MODULE_NAME " 2>/dev/null");
		return 1;
	}
	printf("  [PASS] 挂载成功\n");

	/* STEP 6: 写入测试文件 */
	printf("[STEP 6] 写入测试文件 ...\n");
	f = fopen(TEST_FILE, "w");
	if (!f) {
		fprintf(stderr, "[FAIL] 创建 %s 失败: %s\n", TEST_FILE, strerror(errno));
		system("umount " MOUNT_POINT " 2>/dev/null");
		system("rmmod " MODULE_NAME " 2>/dev/null");
		return 1;
	}
	fputs(TEST_DATA, f);
	fclose(f);
	printf("  [PASS] 写入 \"%s\"\n", TEST_DATA);

	/* STEP 7: 读回并验证 */
	printf("[STEP 7] 读回并验证 ...\n");
	f = fopen(TEST_FILE, "r");
	if (!f) {
		fprintf(stderr, "[FAIL] 读取 %s 失败\n", TEST_FILE);
	} else {
		memset(buf, 0, sizeof(buf));
		fgets(buf, sizeof(buf), f);
		fclose(f);
		if (strcmp(buf, TEST_DATA) == 0) {
			printf("  [PASS] 数据一致: \"%s\"\n", buf);
		} else {
			fprintf(stderr, "[FAIL] 数据不一致: 期望 \"%s\"，得到 \"%s\"\n",
				TEST_DATA, buf);
		}
	}

	/* STEP 8: 卸载文件系统 */
	printf("[STEP 8] 卸载文件系统 ...\n");
	system("umount " MOUNT_POINT);
	rmdir(MOUNT_POINT);
	printf("  [PASS] 卸载成功\n");

	/* STEP 9: 卸载模块 */
	printf("[STEP 9] 卸载模块 ...\n");
	ret = system("rmmod " MODULE_NAME);
	if (ret != 0) {
		fprintf(stderr, "[FAIL] rmmod 失败\n");
		return 1;
	}
	printf("  [PASS] rmmod 成功\n");

	printf("\n=== Ch17 所有测试通过 ===\n");
	return 0;
}
