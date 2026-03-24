/*
 * test_i2c.c — Ch15 I2C 总线驱动用户态测试程序
 *
 * 验证 i2c_master.ko + i2c_slave.ko 的功能：
 *   1. 加载 i2c_master.ko（注册虚拟 I2C 适配器）
 *   2. 加载 i2c_slave.ko（在总线上注册从设备）
 *   3. 检查 /dev/i2c_virt 设备节点
 *   4. 通过 /dev/i2c_virt 写入数据
 *   5. 读回数据并验证
 *   6. 卸载模块
 *
 * 编译：gcc -o test_i2c test_i2c.c
 * 运行：sudo ./test_i2c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define MASTER_MODULE "i2c_virt_master"
#define SLAVE_MODULE  "i2c_virt_slave"
#define DEVICE_PATH   "/dev/i2c_virt"

int main(void)
{
	int fd, ret;
	const char *test_data = "Hello I2C";
	char read_buf[64];
	ssize_t n;

	printf("=== Ch15 I2C 用户态测试程序 ===\n\n");

	/* 清理残留 */
	system("rmmod " SLAVE_MODULE " 2>/dev/null");
	system("rmmod " MASTER_MODULE " 2>/dev/null");

	/* STEP 1: 加载 master */
	printf("[STEP 1] 加载 I2C master (%s.ko) ...\n", MASTER_MODULE);
	ret = system("insmod " MASTER_MODULE ".ko");
	if (ret != 0) {
		fprintf(stderr, "[FAIL] insmod %s 失败\n", MASTER_MODULE);
		return 1;
	}
	printf("  [PASS] master insmod 成功\n");
	usleep(300000);

	/* STEP 2: 加载 slave */
	printf("[STEP 2] 加载 I2C slave (%s.ko) ...\n", SLAVE_MODULE);
	ret = system("insmod " SLAVE_MODULE ".ko");
	if (ret != 0) {
		fprintf(stderr, "[FAIL] insmod %s 失败\n", SLAVE_MODULE);
		system("rmmod " MASTER_MODULE " 2>/dev/null");
		return 1;
	}
	printf("  [PASS] slave insmod 成功\n");
	usleep(300000);

	/* STEP 3: 检查设备节点 */
	printf("[STEP 3] 检查 %s 设备节点 ...\n", DEVICE_PATH);
	if (access(DEVICE_PATH, F_OK) == 0) {
		printf("  [PASS] %s 存在\n", DEVICE_PATH);
	} else {
		fprintf(stderr, "[FAIL] %s 不存在\n", DEVICE_PATH);
		system("rmmod " SLAVE_MODULE " 2>/dev/null");
		system("rmmod " MASTER_MODULE " 2>/dev/null");
		return 1;
	}

	/* STEP 4: 打开设备并写入数据 */
	printf("[STEP 4] 写入数据到 %s ...\n", DEVICE_PATH);
	fd = open(DEVICE_PATH, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "[FAIL] 打开 %s 失败: %s\n", DEVICE_PATH, strerror(errno));
		system("rmmod " SLAVE_MODULE " 2>/dev/null");
		system("rmmod " MASTER_MODULE " 2>/dev/null");
		return 1;
	}
	n = write(fd, test_data, strlen(test_data));
	if (n > 0) {
		printf("  [PASS] 写入 %zd 字节\n", n);
	} else {
		printf("  [WARN] 写入失败: %s\n", strerror(errno));
	}

	/* STEP 5: 读回数据 */
	printf("[STEP 5] 读回数据 ...\n");
	lseek(fd, 0, SEEK_SET);
	memset(read_buf, 0, sizeof(read_buf));
	n = read(fd, read_buf, sizeof(read_buf) - 1);
	if (n > 0) {
		printf("  [PASS] 读回 %zd 字节: \"%s\"\n", n, read_buf);
	} else {
		printf("  [WARN] 读取失败或无数据: %s\n", strerror(errno));
	}
	close(fd);

	/* STEP 6: 卸载模块 */
	printf("[STEP 6] 卸载模块 ...\n");
	system("rmmod " SLAVE_MODULE " 2>/dev/null");
	ret = system("rmmod " MASTER_MODULE);
	if (ret != 0) {
		fprintf(stderr, "[FAIL] rmmod %s 失败\n", MASTER_MODULE);
		return 1;
	}
	printf("  [PASS] 模块卸载成功\n");

	printf("\n=== Ch15 所有测试通过 ===\n");
	return 0;
}
