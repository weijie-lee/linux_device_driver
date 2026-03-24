/*
 * test_regmap.c — Ch08 Regmap 驱动用户态测试程序
 *
 * 测试项目：
 *   TC01: ioctl REGMAP_IOC_READ 读取初始寄存器值
 *   TC02: ioctl REGMAP_IOC_WRITE 写入寄存器
 *   TC03: 读回验证写入值
 *   TC04: ioctl REGMAP_IOC_UPDBITS 位操作
 *   TC05: 验证位操作结果
 *
 * 编译：gcc -o test_regmap test_regmap.c
 * 运行：sudo ./test_regmap
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define DEVICE      "/dev/regmap_demo"
#define REGMAP_MAGIC    'R'

struct regmap_ioc_arg {
	unsigned int reg;
	unsigned int val;
};
struct regmap_ioc_updbits {
	unsigned int reg;
	unsigned int mask;
	unsigned int val;
};

#define REGMAP_IOC_READ     _IOWR(REGMAP_MAGIC, 0, struct regmap_ioc_arg)
#define REGMAP_IOC_WRITE    _IOW(REGMAP_MAGIC,  1, struct regmap_ioc_arg)
#define REGMAP_IOC_UPDBITS  _IOW(REGMAP_MAGIC,  2, struct regmap_ioc_updbits)

static int pass_count = 0, fail_count = 0;
#define ASSERT(cond, msg) do { \
	if (cond) { printf("  [PASS] %s\n", msg); pass_count++; } \
	else { printf("  [FAIL] %s\n", msg); fail_count++; } \
} while (0)

int main(void)
{
	int fd;
	struct regmap_ioc_arg rw;
	struct regmap_ioc_updbits upd;

	printf("=== Ch08 Regmap Driver Test ===\n\n");

	fd = open(DEVICE, O_RDWR);
	if (fd < 0) { perror("open"); return 1; }

	/* TC01: 读取初始值（寄存器 5 的初始值应为 5*0x10 = 0x50） */
	printf("[TC01] Read initial register value\n");
	rw.reg = 5; rw.val = 0;
	ASSERT(ioctl(fd, REGMAP_IOC_READ, &rw) == 0, "ioctl READ returns 0");
	printf("  reg[5] = 0x%x (expected 0x50)\n", rw.val);
	ASSERT(rw.val == 0x50, "reg[5] initial value is 0x50");

	/* TC02: 写入寄存器 */
	printf("[TC02] Write register\n");
	rw.reg = 10; rw.val = 0xDEADBEEF;
	ASSERT(ioctl(fd, REGMAP_IOC_WRITE, &rw) == 0, "ioctl WRITE returns 0");

	/* TC03: 读回验证 */
	printf("[TC03] Read back written value\n");
	rw.reg = 10; rw.val = 0;
	ioctl(fd, REGMAP_IOC_READ, &rw);
	printf("  reg[10] = 0x%x (expected 0xDEADBEEF)\n", rw.val);
	ASSERT(rw.val == 0xDEADBEEF, "read back matches written value");

	/* TC04: 位操作 — 将 reg[10] 的低8位清零，高8位设为 0xAB */
	printf("[TC04] Update bits (mask=0xFF0000FF, val=0xAB000000)\n");
	upd.reg = 10; upd.mask = 0xFF0000FF; upd.val = 0xAB000000;
	ASSERT(ioctl(fd, REGMAP_IOC_UPDBITS, &upd) == 0, "ioctl UPDBITS returns 0");

	/* TC05: 验证位操作结果
	 * 原值：0xDEADBEEF
	 * mask：0xFF0000FF → 修改 bit[31:24] 和 bit[7:0]
	 * val： 0xAB000000 → bit[31:24]=0xAB, bit[7:0]=0x00
	 * 期望：0xABADBE00
	 */
	printf("[TC05] Verify update_bits result\n");
	rw.reg = 10; rw.val = 0;
	ioctl(fd, REGMAP_IOC_READ, &rw);
	printf("  reg[10] = 0x%x (expected 0xABADBE00)\n", rw.val);
	ASSERT(rw.val == 0xABADBE00, "update_bits result is correct");

	close(fd);

	printf("\n=== Test Summary ===\n");
	printf("PASS: %d  FAIL: %d\n", pass_count, fail_count);
	return (fail_count == 0) ? 0 : 1;
}
