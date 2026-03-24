/*
 * pwm_demo.c — Ch11: PWM 脉宽调制驱动
 *
 * 【知识点】
 * PWM（Pulse Width Modulation）广泛用于 LED 调光、电机控制、
 * 蜂鸣器等场景。Linux PWM 框架通过 pwm_chip 抽象硬件 PWM 控制器，
 * 用户空间通过 sysfs 接口配置 period（周期）和 duty_cycle（占空比）。
 *
 * 【核心接口】
 *   pwm_chip：PWM 控制器，包含多个 PWM 通道
 *   pwm_ops.request/free：通道申请/释放
 *   pwm_ops.config：配置 period 和 duty_cycle（单位：纳秒）
 *   pwm_ops.enable/disable：使能/禁用 PWM 输出
 *
 * 【sysfs 接口】
 *   /sys/class/pwm/pwmchipX/export          写入通道号，导出 PWM 通道
 *   /sys/class/pwm/pwmchipX/pwmY/period     设置周期（纳秒）
 *   /sys/class/pwm/pwmchipX/pwmY/duty_cycle 设置占空比（纳秒）
 *   /sys/class/pwm/pwmchipX/pwmY/enable     使能（写入 1）
 *
 * 【本示例的模拟方案】
 * 注册一个虚拟 PWM 控制器（2个通道），config/enable 回调只打印
 * 参数到 dmesg，不操作真实硬件。
 *
 * 【验证方法】
 * sudo insmod pwm_demo.ko
 * ls /sys/class/pwm/                        # 查看注册的 pwmchip
 * CHIP=$(ls /sys/class/pwm/ | grep pwmchip | tail -1)
 * echo 0 > /sys/class/pwm/$CHIP/export      # 导出通道 0
 * echo 1000000 > /sys/class/pwm/$CHIP/pwm0/period      # 1ms 周期
 * echo 500000  > /sys/class/pwm/$CHIP/pwm0/duty_cycle  # 50% 占空比
 * echo 1 > /sys/class/pwm/$CHIP/pwm0/enable            # 使能
 * dmesg | tail -5                           # 查看驱动输出
 * sudo rmmod pwm_demo
 */

#include <linux/module.h>
#include <linux/pwm.h>          /* pwm_chip, pwm_ops, pwmchip_add */
#include <linux/platform_device.h>
#include <linux/slab.h>

#define DRIVER_NAME     "virt_pwm"
#define PWM_CHANNELS    2       /* 虚拟 PWM 控制器有 2 个通道 */

/* ============================================================
 * 每个 PWM 通道的状态
 * ============================================================ */
struct virt_pwm_channel {
	unsigned int    period_ns;      /* 当前周期（纳秒） */
	unsigned int    duty_cycle_ns;  /* 当前占空比（纳秒） */
	bool            enabled;
};

/* ============================================================
 * PWM 控制器私有数据
 * ============================================================ */
struct virt_pwm_chip {
	struct pwm_chip         chip;
	struct virt_pwm_channel channels[PWM_CHANNELS];
};

/* 从 pwm_chip 获取私有数据（container_of 用法） */
static inline struct virt_pwm_chip *to_virt_pwm(struct pwm_chip *chip)
{
	return container_of(chip, struct virt_pwm_chip, chip);
}

/* ============================================================
 * PWM 操作回调
 * ============================================================ */

static int virt_pwm_request(struct pwm_chip *chip, struct pwm_device *pwm)
{
	dev_info(chip->dev, "channel %d requested\n", pwm->hwpwm);
	return 0;
}

static void virt_pwm_free(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct virt_pwm_chip *vchip = to_virt_pwm(chip);
	vchip->channels[pwm->hwpwm].enabled = false;
	dev_info(chip->dev, "channel %d freed\n", pwm->hwpwm);
}

/*
 * virt_pwm_apply — 配置并应用 PWM 状态（内核 5.x 新 API）
 * 替代旧版的 config + enable/disable 分离接口
 *
 * 真实驱动中，这里会写寄存器设置 PWM 周期和占空比
 */
static int virt_pwm_apply(struct pwm_chip *chip, struct pwm_device *pwm,
			  const struct pwm_state *state)
{
	struct virt_pwm_chip *vchip = to_virt_pwm(chip);
	struct virt_pwm_channel *ch = &vchip->channels[pwm->hwpwm];

	ch->period_ns    = (unsigned int)state->period;
	ch->duty_cycle_ns = (unsigned int)state->duty_cycle;
	ch->enabled      = state->enabled;

	dev_info(chip->dev,
		 "channel %d: period=%uns duty_cycle=%uns polarity=%s enabled=%d\n",
		 pwm->hwpwm,
		 ch->period_ns,
		 ch->duty_cycle_ns,
		 (state->polarity == PWM_POLARITY_NORMAL) ? "normal" : "inversed",
		 ch->enabled);

	/* 计算并打印占空比百分比（方便验证） */
	if (ch->period_ns > 0) {
		unsigned int duty_pct = ch->duty_cycle_ns * 100 / ch->period_ns;
		dev_info(chip->dev, "channel %d: duty_cycle=%u%%\n",
			 pwm->hwpwm, duty_pct);
	}

	return 0;
}

/*
 * virt_pwm_get_state — 读取当前 PWM 状态
 */
static void virt_pwm_get_state(struct pwm_chip *chip, struct pwm_device *pwm,
				struct pwm_state *state)
{
	struct virt_pwm_chip *vchip = to_virt_pwm(chip);
	struct virt_pwm_channel *ch = &vchip->channels[pwm->hwpwm];

	state->period     = ch->period_ns;
	state->duty_cycle = ch->duty_cycle_ns;
	state->polarity   = PWM_POLARITY_NORMAL;
	state->enabled    = ch->enabled;
}

static const struct pwm_ops virt_pwm_ops = {
	.request   = virt_pwm_request,
	.free      = virt_pwm_free,
	.apply     = virt_pwm_apply,
	.get_state = virt_pwm_get_state,
	.owner     = THIS_MODULE,
};

/* ============================================================
 * Platform Driver probe/remove
 * ============================================================ */
static int virt_pwm_probe(struct platform_device *pdev)
{
	struct virt_pwm_chip *vchip;
	int ret;

	vchip = devm_kzalloc(&pdev->dev, sizeof(*vchip), GFP_KERNEL);
	if (!vchip)
		return -ENOMEM;

	/* 初始化 pwm_chip */
	vchip->chip.dev  = &pdev->dev;
	vchip->chip.ops  = &virt_pwm_ops;
	vchip->chip.npwm = PWM_CHANNELS;  /* 通道数 */

	/*
	 * pwmchip_add：注册 PWM 控制器
	 * 注册后在 /sys/class/pwm/ 下创建 pwmchipX 目录
	 * 用户通过 echo N > export 导出具体通道
	 */
	ret = pwmchip_add(&vchip->chip);
	if (ret) {
		dev_err(&pdev->dev, "pwmchip_add failed: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, vchip);

	dev_info(&pdev->dev, "probed, %d PWM channels registered\n",
		 PWM_CHANNELS);
	dev_info(&pdev->dev, "sysfs: /sys/class/pwm/pwmchip%d/\n",
		 vchip->chip.base);
	return 0;
}

static int virt_pwm_remove(struct platform_device *pdev)
{
	struct virt_pwm_chip *vchip = platform_get_drvdata(pdev);
	pwmchip_remove(&vchip->chip);
	dev_info(&pdev->dev, "removed\n");
	return 0;
}

static struct platform_driver virt_pwm_driver = {
	.probe  = virt_pwm_probe,
	.remove = virt_pwm_remove,
	.driver = { .name = DRIVER_NAME, .owner = THIS_MODULE },
};

static struct platform_device *virt_pwm_pdev;

static int __init pwm_demo_init(void)
{
	int ret;

	virt_pwm_pdev = platform_device_alloc(DRIVER_NAME, -1);
	if (!virt_pwm_pdev)
		return -ENOMEM;

	ret = platform_device_add(virt_pwm_pdev);
	if (ret) { platform_device_put(virt_pwm_pdev); return ret; }

	ret = platform_driver_register(&virt_pwm_driver);
	if (ret) { platform_device_unregister(virt_pwm_pdev); return ret; }

	pr_info("%s: module loaded\n", DRIVER_NAME);
	return 0;
}

static void __exit pwm_demo_exit(void)
{
	platform_driver_unregister(&virt_pwm_driver);
	platform_device_unregister(virt_pwm_pdev);
	pr_info("%s: module unloaded\n", DRIVER_NAME);
}

module_init(pwm_demo_init);
module_exit(pwm_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("weijie-lee");
MODULE_DESCRIPTION("Ch11: PWM driver demo - virtual PWM chip with 2 channels");
