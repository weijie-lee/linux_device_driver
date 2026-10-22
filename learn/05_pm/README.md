# runtime PM 和系统睡眠

## 代码知识点

`dev_pm_ops` 上有两对回调。

**系统睡眠** `suspend` / `resume` 把 `ctrl` 清成 0，唤醒时写回。生产环境里它们由 `pm_suspend()` 调用。本实验不让客户机进入 mem 睡眠，`sleep` 属性调用的是驱动结构体里的同一对函数指针。

**runtime PM** 在 probe 里 `pm_runtime_set_active()` 然后 `pm_runtime_enable()`。写 `idle` 调用 `pm_runtime_idle()`。usage 为 0 时内核进入 `runtime_suspend`。`/sys/devices/platform/virt_pm/power/control` 写成 `on` 会 `pm_runtime_forbid()` 并 resume。

```bash
cat /sys/devices/platform/virt_pm/ctrl                 # 3
echo suspend > /sys/devices/platform/virt_pm/sleep
cat /sys/devices/platform/virt_pm/ctrl                 # 0
echo resume > /sys/devices/platform/virt_pm/sleep
echo 1 > /sys/devices/platform/virt_pm/idle
cat /sys/devices/platform/virt_pm/power/runtime_status # suspended
echo on > /sys/devices/platform/virt_pm/power/control
cat /sys/devices/platform/virt_pm/power/runtime_status # active
```

`runtime_marker` 由 runtime 回调更新，用来确认走的是驱动自己的函数，而不只是 PM 核心改了状态字。

## 代码架构

```
probe
  device_create_file          sleep、idle、sleeping 等属性
  pm_runtime_set_active → pm_runtime_enable
echo suspend > sleep          virt_suspend：保存 ctrl，清成 0
echo resume  > sleep          virt_resume：写回 ctrl
echo 1 > idle                 pm_runtime_idle → virt_runtime_suspend，runtime 记为 suspended
echo on > power/control       pm_runtime_forbid → virt_runtime_resume，runtime 记为 active
remove
  pm_runtime_disable
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter learn05
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/learn_05_pm_virt_pm.ko
insmod ok
+ echo suspend > /sys/devices/platform/virt_pm/sleep
+ cat /sys/devices/platform/virt_pm/ctrl /sys/devices/platform/virt_pm/sleeping
0
1
+ echo resume > /sys/devices/platform/virt_pm/sleep
+ cat /sys/devices/platform/virt_pm/ctrl
3
+ echo 1 > /sys/devices/platform/virt_pm/idle
sh: write error: Resource temporarily unavailable
+ cat /sys/devices/platform/virt_pm/power/runtime_status /sys/devices/platform/virt_pm/runtime_marker
suspended
suspended
+ echo on > /sys/devices/platform/virt_pm/power/control
+ cat /sys/devices/platform/virt_pm/power/runtime_status /sys/devices/platform/virt_pm/runtime_marker
active
active
+ rmmod virt_pm
rmmod ok
[PASS] learn05_pm
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] learn05_pm`
- suspend 之后 ctrl 是 `0`，resume 之后是 `3`。写 `idle` 时 shell 可能打印 `Resource temporarily unavailable`，随后 `runtime_status` 仍是 `suspended`，`control=on` 之后是 `active`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter learn05
```
