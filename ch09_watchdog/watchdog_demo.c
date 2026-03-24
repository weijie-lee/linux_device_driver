/*
 * watchdog_demo.c — Ch09: Watchdog 看门狗驱动
 *
 * 【知识点】
 * 看门狗定时器（Watchdog Timer）是系统可靠性的重要保障机制。
 * 当软件出现死锁或挂起时，看门狗超时会触发系统复位。
 * 用户态守护进程需要定期"喂狗"（写入 /dev/watchdog），
 * 否则系统将在超时后复位。
 *
 * 【Linux Watchdog 框架核心接口】
 *   watchdog_device：描述看门狗设备（超时时间、最小/最大超时等）
 *   watchdog_ops：start/stop/ping/set_timeout/get_timeleft 回调
 *   watchdog_register_device：注册到内核，自动创建 /dev/watchdog
 *
 * 【关键特性】
 *   nowayout：模块加载后无法停止（防止意外卸载导致系统无保护）
 *   WDIOF_KEEPALIVEPING：支持 ping 操作（写入任意字节即喂狗）
 *   WDIOC_SETTIMEOUT：ioctl 动态修改超时时间
 *
 * 【本示例的模拟方案】
 * 使用内核 hrtimer 模拟看门狗定时器，超时后打印警告（不真正复位）。
 * 通过标准 /dev/watchdog 接口喂狗。
 *
 * 【验证方法】
 * sudo insmod watchdog_demo.ko timeout=5
 * exec 3>/dev/watchdog                 # 打开看门狗（开始计时）
 * echo "V" >&3                         # 喂狗（重置计时器）
 * sleep 6                              # 等待超时（dmesg 会看到警告）
 * echo "V" >/dev/watchdog              # 喂狗并关闭（写入 'V' 表示正常关闭）
 * sudo rmmod watchdog_demo
 */

#include <linux/module.h>
#include <linux/watchdog.h>     /* watchdog_device, watchdog_ops */
#include <linux/hrtimer.h>      /* hrtimer，模拟看门狗定时器 */
#include <linux/ktime.h>
#include <linux/moduleparam.h>

#define DRIVER_NAME     "virt_wdt"
#define WDT_DEFAULT_TIMEOUT 30  /* 默认超时时间（秒） */
#define WDT_MIN_TIMEOUT     1
#define WDT_MAX_TIMEOUT     300

/* 模块参数：加载时可指定超时时间 */
static int timeout = WDT_DEFAULT_TIMEOUT;
module_param(timeout, int, 0644);
MODULE_PARM_DESC(timeout, "Watchdog timeout in seconds (default=30)");

/* nowayout：设为 1 后，一旦打开 /dev/watchdog 就无法停止 */
static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0644);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default="
		 __MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

/* ============================================================
 * 私有数据
 * ============================================================ */
struct virt_wdt_priv {
	struct hrtimer  timer;          /* 模拟看门狗定时器 */
	bool            timer_running;  /* 定时器是否在运行 */
};

static struct virt_wdt_priv wdt_priv;

/* ============================================================
 * hrtimer 回调：模拟看门狗超时
 * ============================================================ */
static enum hrtimer_restart virt_wdt_timer_fn(struct hrtimer *timer)
{
	/* 真实硬件看门狗超时后会触发系统复位
	 * 本示例只打印警告，不真正复位 */
	pr_crit("%s: WATCHDOG TIMEOUT! System would reset in real hardware!\n",
		DRIVER_NAME);
	pr_crit("%s: Feed the watchdog by writing to /dev/watchdog\n",
		DRIVER_NAME);

	/* 返回 HRTIMER_NORESTART 表示不自动重启定时器 */
	return HRTIMER_NORESTART;
}

/* ============================================================
 * Watchdog 操作回调
 * ============================================================ */

/*
 * virt_wdt_start — 启动看门狗
 * 当用户打开 /dev/watchdog 时由内核调用
 */
static int virt_wdt_start(struct watchdog_device *wdd)
{
	struct virt_wdt_priv *priv = watchdog_get_drvdata(wdd);

	/* 启动 hrtimer，超时时间为 wdd->timeout 秒 */
	hrtimer_start(&priv->timer,
		      ktime_set(wdd->timeout, 0),
		      HRTIMER_MODE_REL);
	priv->timer_running = true;

	pr_info("%s: started, timeout=%u seconds\n", DRIVER_NAME, wdd->timeout);
	return 0;
}

/*
 * virt_wdt_stop — 停止看门狗
 * 当用户向 /dev/watchdog 写入 'V' 并关闭时由内核调用
 * （如果 nowayout=1，此函数不会被调用）
 */
static int virt_wdt_stop(struct watchdog_device *wdd)
{
	struct virt_wdt_priv *priv = watchdog_get_drvdata(wdd);

	hrtimer_cancel(&priv->timer);
	priv->timer_running = false;

	pr_info("%s: stopped\n", DRIVER_NAME);
	return 0;
}

/*
 * virt_wdt_ping — 喂狗（重置超时计时器）
 * 当用户写入 /dev/watchdog 时由内核调用
 */
static int virt_wdt_ping(struct watchdog_device *wdd)
{
	struct virt_wdt_priv *priv = watchdog_get_drvdata(wdd);

	/* 重置定时器：先取消，再重新启动 */
	hrtimer_cancel(&priv->timer);
	hrtimer_start(&priv->timer,
		      ktime_set(wdd->timeout, 0),
		      HRTIMER_MODE_REL);

	pr_info("%s: ping (timeout reset to %u seconds)\n",
		DRIVER_NAME, wdd->timeout);
	return 0;
}

/*
 * virt_wdt_set_timeout — 动态修改超时时间
 * 通过 ioctl(fd, WDIOC_SETTIMEOUT, &new_timeout) 调用
 */
static int virt_wdt_set_timeout(struct watchdog_device *wdd, unsigned int t)
{
	wdd->timeout = t;

	/* 如果定时器正在运行，重新启动以应用新超时 */
	if (watchdog_active(wdd)) {
		struct virt_wdt_priv *priv = watchdog_get_drvdata(wdd);
		hrtimer_cancel(&priv->timer);
		hrtimer_start(&priv->timer, ktime_set(t, 0), HRTIMER_MODE_REL);
	}

	pr_info("%s: timeout changed to %u seconds\n", DRIVER_NAME, t);
	return 0;
}

/*
 * virt_wdt_get_timeleft — 获取剩余超时时间
 * 通过 ioctl(fd, WDIOC_GETTIMELEFT, &timeleft) 调用
 */
static unsigned int virt_wdt_get_timeleft(struct watchdog_device *wdd)
{
	struct virt_wdt_priv *priv = watchdog_get_drvdata(wdd);
	ktime_t remaining;
	s64 secs;

	if (!priv->timer_running)
		return 0;

	remaining = hrtimer_get_remaining(&priv->timer);
	secs = ktime_to_ns(remaining) / NSEC_PER_SEC;
	return (secs > 0) ? (unsigned int)secs : 0;
}

/* ============================================================
 * Watchdog 设备注册
 * ============================================================ */
static const struct watchdog_ops virt_wdt_ops = {
	.owner          = THIS_MODULE,
	.start          = virt_wdt_start,
	.stop           = virt_wdt_stop,
	.ping           = virt_wdt_ping,
	.set_timeout    = virt_wdt_set_timeout,
	.get_timeleft   = virt_wdt_get_timeleft,
};

static struct watchdog_device virt_wdt_dev = {
	.info = &(const struct watchdog_info) {
		.options    = WDIOF_SETTIMEOUT |    /* 支持动态设置超时 */
			      WDIOF_KEEPALIVEPING | /* 支持 ping */
			      WDIOF_MAGICCLOSE,     /* 写入 'V' 才能正常关闭 */
		.firmware_version = 1,
		.identity   = "Virtual Watchdog",
	},
	.ops            = &virt_wdt_ops,
	.timeout        = WDT_DEFAULT_TIMEOUT,
	.min_timeout    = WDT_MIN_TIMEOUT,
	.max_timeout    = WDT_MAX_TIMEOUT,
};

/* ============================================================
 * 模块初始化与退出
 * ============================================================ */
static int __init watchdog_demo_init(void)
{
	int ret;

	/* 初始化 hrtimer */
	hrtimer_init(&wdt_priv.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	wdt_priv.timer.function = virt_wdt_timer_fn;
	wdt_priv.timer_running  = false;

	/* 设置超时时间（来自模块参数） */
	virt_wdt_dev.timeout = timeout;
	watchdog_set_drvdata(&virt_wdt_dev, &wdt_priv);

	/* 设置 nowayout 标志 */
	watchdog_set_nowayout(&virt_wdt_dev, nowayout);

	/*
	 * watchdog_register_device：注册看门狗设备
	 * 自动创建 /dev/watchdog 和 /dev/watchdog0
	 */
	ret = watchdog_register_device(&virt_wdt_dev);
	if (ret) {
		pr_err("%s: watchdog_register_device failed: %d\n",
		       DRIVER_NAME, ret);
		return ret;
	}

	pr_info("%s: registered, timeout=%d seconds, nowayout=%d\n",
		DRIVER_NAME, timeout, nowayout);
	return 0;
}

static void __exit watchdog_demo_exit(void)
{
	watchdog_unregister_device(&virt_wdt_dev);
	hrtimer_cancel(&wdt_priv.timer);
	pr_info("%s: unregistered\n", DRIVER_NAME);
}

module_init(watchdog_demo_init);
module_exit(watchdog_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("weijie-lee");
MODULE_DESCRIPTION("Ch09: Watchdog timer driver demo");
