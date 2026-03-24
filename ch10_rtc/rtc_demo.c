/*
 * rtc_demo.c — Ch10: RTC 实时时钟驱动
 *
 * 【知识点】
 * RTC（Real-Time Clock）驱动通过 rtc_device 框架接入内核，
 * 提供标准的 /dev/rtcX 接口。用户空间通过 ioctl 读写时间、
 * 设置闹钟，内核通过 rtc_ops 回调调用驱动实现。
 *
 * 【核心接口】
 *   rtc_device_register：注册 RTC 设备，创建 /dev/rtc0
 *   rtc_ops.read_time：读取当前时间（struct rtc_time）
 *   rtc_ops.set_time：设置时间
 *   rtc_ops.read_alarm：读取闹钟时间
 *   rtc_ops.set_alarm：设置闹钟
 *
 * 【本示例的模拟方案】
 * 用 ktime_get_real_ts64() 获取系统时间作为 RTC 时间，
 * 支持时间读写（写入后在内存中保存偏移量）。
 * 闹钟通过 hrtimer 模拟，超时后触发 RTC 中断。
 *
 * 【验证方法】
 * sudo insmod rtc_demo.ko
 * hwclock -r -f /dev/rtc1          # 读取 RTC 时间
 * hwclock -w -f /dev/rtc1          # 将系统时间写入 RTC
 * sudo rmmod rtc_demo
 */

#include <linux/module.h>
#include <linux/rtc.h>          /* rtc_device, rtc_ops, rtc_time */
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>  /* ktime_get_real_ts64 */

#define DRIVER_NAME "virt_rtc"

/* ============================================================
 * 私有数据
 * ============================================================ */
struct virt_rtc_priv {
	struct rtc_device   *rtc;
	struct platform_device *pdev;

	/* 时间偏移：set_time 后保存与系统时间的差值（秒） */
	s64                  time_offset;

	/* 闹钟 */
	struct hrtimer       alarm_timer;
	struct rtc_wkalrm    alarm;
	bool                 alarm_enabled;
};

static struct virt_rtc_priv *g_priv;

/* ============================================================
 * RTC 操作回调
 * ============================================================ */

/*
 * virt_rtc_read_time — 读取当前时间
 * 返回系统时间 + 用户设置的偏移量
 */
static int virt_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct virt_rtc_priv *priv = dev_get_drvdata(dev);
	struct timespec64 ts;

	ktime_get_real_ts64(&ts);
	ts.tv_sec += priv->time_offset;  /* 应用偏移量 */

	/* 将 Unix 时间戳转换为 struct rtc_time（年月日时分秒） */
	rtc_time64_to_tm(ts.tv_sec, tm);
	return 0;
}

/*
 * virt_rtc_set_time — 设置时间
 * 计算新时间与当前系统时间的差值，保存为偏移量
 */
static int virt_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct virt_rtc_priv *priv = dev_get_drvdata(dev);
	struct timespec64 ts;
	time64_t new_time;

	/* 将 struct rtc_time 转换回 Unix 时间戳 */
	new_time = rtc_tm_to_time64(tm);

	ktime_get_real_ts64(&ts);
	priv->time_offset = new_time - ts.tv_sec;

	pr_info("%s: time set, offset=%lld seconds\n",
		DRIVER_NAME, (long long)priv->time_offset);
	return 0;
}

/* 闹钟定时器回调 */
static enum hrtimer_restart virt_rtc_alarm_fn(struct hrtimer *timer)
{
	struct virt_rtc_priv *priv =
		container_of(timer, struct virt_rtc_priv, alarm_timer);

	pr_info("%s: ALARM triggered!\n", DRIVER_NAME);

	/* 触发 RTC 闹钟中断，通知等待的进程 */
	rtc_update_irq(priv->rtc, 1, RTC_AF | RTC_IRQF);
	priv->alarm_enabled = false;

	return HRTIMER_NORESTART;
}

static int virt_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct virt_rtc_priv *priv = dev_get_drvdata(dev);
	*alrm = priv->alarm;
	return 0;
}

static int virt_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct virt_rtc_priv *priv = dev_get_drvdata(dev);
	struct timespec64 now;
	time64_t alarm_time, now_time;
	s64 delta_ns;

	/* 取消已有闹钟 */
	hrtimer_cancel(&priv->alarm_timer);
	priv->alarm = *alrm;

	if (!alrm->enabled) {
		priv->alarm_enabled = false;
		return 0;
	}

	/* 计算闹钟触发时间与当前时间的差值 */
	alarm_time = rtc_tm_to_time64(&alrm->time);
	ktime_get_real_ts64(&now);
	now_time = now.tv_sec + priv->time_offset;
	delta_ns = (alarm_time - now_time) * NSEC_PER_SEC;

	if (delta_ns <= 0) {
		pr_warn("%s: alarm time is in the past\n", DRIVER_NAME);
		return -EINVAL;
	}

	hrtimer_start(&priv->alarm_timer, ns_to_ktime(delta_ns), HRTIMER_MODE_REL);
	priv->alarm_enabled = true;

	pr_info("%s: alarm set, triggers in %lld seconds\n",
		DRIVER_NAME, delta_ns / NSEC_PER_SEC);
	return 0;
}

static const struct rtc_class_ops virt_rtc_ops = {
	.read_time  = virt_rtc_read_time,
	.set_time   = virt_rtc_set_time,
	.read_alarm = virt_rtc_read_alarm,
	.set_alarm  = virt_rtc_set_alarm,
};

/* ============================================================
 * Platform Driver probe/remove
 * ============================================================ */
static int virt_rtc_probe(struct platform_device *pdev)
{
	struct virt_rtc_priv *priv;
	int ret;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->pdev = pdev;
	priv->time_offset = 0;
	priv->alarm_enabled = false;

	/* 初始化闹钟定时器 */
	hrtimer_init(&priv->alarm_timer, CLOCK_REALTIME, HRTIMER_MODE_REL);
	priv->alarm_timer.function = virt_rtc_alarm_fn;

	/*
	 * rtc_allocate_device + rtc_register_device（内核 5.x 新 API）
	 * 或旧版 rtc_device_register（内核 4.x）
	 * 注册后自动创建 /dev/rtc1（如果 rtc0 已被系统 RTC 占用）
	 */
	priv->rtc = devm_rtc_allocate_device(&pdev->dev);
	if (IS_ERR(priv->rtc))
		return PTR_ERR(priv->rtc);

	priv->rtc->ops = &virt_rtc_ops;
	priv->rtc->range_min = RTC_TIMESTAMP_BEGIN_2000;
	priv->rtc->range_max = RTC_TIMESTAMP_END_2099;

	ret = devm_rtc_register_device(priv->rtc);
	if (ret)
		return ret;

	dev_set_drvdata(&pdev->dev, priv);
	g_priv = priv;

	dev_info(&pdev->dev, "probed, /dev/%s created\n",
		 dev_name(&priv->rtc->dev));
	return 0;
}

static int virt_rtc_remove(struct platform_device *pdev)
{
	struct virt_rtc_priv *priv = dev_get_drvdata(&pdev->dev);
	hrtimer_cancel(&priv->alarm_timer);
	dev_info(&pdev->dev, "removed\n");
	return 0;
}

static struct platform_driver virt_rtc_driver = {
	.probe  = virt_rtc_probe,
	.remove = virt_rtc_remove,
	.driver = { .name = DRIVER_NAME, .owner = THIS_MODULE },
};

static struct platform_device *virt_rtc_pdev;

static int __init rtc_demo_init(void)
{
	int ret;

	virt_rtc_pdev = platform_device_alloc(DRIVER_NAME, -1);
	if (!virt_rtc_pdev)
		return -ENOMEM;

	ret = platform_device_add(virt_rtc_pdev);
	if (ret) { platform_device_put(virt_rtc_pdev); return ret; }

	ret = platform_driver_register(&virt_rtc_driver);
	if (ret) { platform_device_unregister(virt_rtc_pdev); return ret; }

	pr_info("%s: module loaded\n", DRIVER_NAME);
	return 0;
}

static void __exit rtc_demo_exit(void)
{
	platform_driver_unregister(&virt_rtc_driver);
	platform_device_unregister(virt_rtc_pdev);
	pr_info("%s: module unloaded\n", DRIVER_NAME);
}

module_init(rtc_demo_init);
module_exit(rtc_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("weijie-lee");
MODULE_DESCRIPTION("Ch10: RTC real-time clock driver demo");
