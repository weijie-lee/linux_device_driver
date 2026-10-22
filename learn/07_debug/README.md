# 调试：dyndbg、ftrace，以及怎么读 oops

## 代码知识点

### dyndbg 和 ftrace

`virt_debug_ping()` 标了 `noinline`，里面有一条 `pr_debug` 和一条 `pr_info`。`CONFIG_DYNAMIC_DEBUG` 打开时，`pr_debug` 默认不打印。

```bash
mount -t debugfs none /sys/kernel/debug
mount -t tracefs none /sys/kernel/tracing
echo 1 > /sys/devices/platform/virt_debug/trigger
dmesg | grep ping          # 只有 ping info

echo 'module virt_debug +p' > /sys/kernel/debug/dynamic_debug/control
echo 1 > /sys/devices/platform/virt_debug/trigger
dmesg | grep 'ping debug'

echo 0 > /sys/kernel/tracing/tracing_on
echo nop > /sys/kernel/tracing/current_tracer
echo function > /sys/kernel/tracing/current_tracer
echo virt_debug_ping > /sys/kernel/tracing/set_ftrace_filter
echo 1 > /sys/kernel/tracing/tracing_on
echo 1 > /sys/devices/platform/virt_debug/trigger
echo 0 > /sys/kernel/tracing/tracing_on
grep virt_debug_ping /sys/kernel/tracing/trace
echo nop > /sys/kernel/tracing/current_tracer
```

跟踪结束要把 tracer 设回 `nop`。

### 读 oops

一次 oops 先看这几项：出错指令在 `RIP`，访问的地址在 `CR2`，`Call Trace` 是从出错点回到系统调用或中断的路径。

`ch29_thermal` 曾在 QEMU 里缺页：`thermal_zone_device_register()` 在 `ops->critical` 为空时写入这个函数指针，而 `virt_ops` 被写成了 `const`，放在只读的 `.rodata`。写入只读页就会 oops。修法是提供 `virt_critical()`，并去掉 `const`。不要在教学模块里主动触发这个写入。

### kgdb

`run_qemu.sh` 的普通测试不加调试桩。要单步时，在 QEMU 参数里加上 `-s -S`（gdb 在 `tcp::1234`，`-S` 表示上电后先停住），串口改回终端。宿主机：

```
gdb vmlinux
target remote :1234
```

客户机内核是 Ubuntu 的 `vmlinuz`，没有带调试信息的 vmlinux 时，断点只能落到模块的 `add-symbol-file`。先在客户机里看 `/sys/module/virt_debug/sections/.text` 的地址，再把 `virt_debug.ko` 配这个地址加载进 gdb。

## 代码架构

```
probe
  device_create_file(trigger)
echo 1 > trigger
  trigger_store → virt_debug_ping()      noinline，ftrace 能按符号名过滤
    pr_debug  "ping debug"               CONFIG_DYNAMIC_DEBUG：默认关，写 dynamic_debug/control 才开
    pr_info   "ping info"                始终打印
ftrace
  current_tracer = function → set_ftrace_filter = virt_debug_ping → tracing_on
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter learn07
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/learn_07_debug_virt_debug.ko
insmod ok
+ echo 1 > /sys/devices/platform/virt_debug/trigger
+ dmesg | grep virt_debug
[   86.254455] virt_debug: ping info
+ echo 'module virt_debug +p' > /sys/kernel/debug/dynamic_debug/control
+ echo 1 > /sys/devices/platform/virt_debug/trigger
+ dmesg | grep 'ping debug'
[   86.279881] virt_debug: ping debug
+ echo 0 > /sys/kernel/tracing/tracing_on
+ echo nop > /sys/kernel/tracing/current_tracer
+ echo function > /sys/kernel/tracing/current_tracer
+ echo virt_debug_ping > /sys/kernel/tracing/set_ftrace_filter
+ echo > /sys/kernel/tracing/trace
+ echo 1 > /sys/kernel/tracing/tracing_on
+ echo 1 > /sys/devices/platform/virt_debug/trigger
+ echo 0 > /sys/kernel/tracing/tracing_on
+ grep virt_debug_ping /sys/kernel/tracing/trace
              sh-1       [001] .....    92.021538: virt_debug_ping <-trigger_store
+ rmmod virt_debug
rmmod ok
[PASS] learn07_debug
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] learn07_debug`
- 打开 dyndbg 之前只有 `ping info`，之后有 `ping debug`，ftrace 里有 `virt_debug_ping`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter learn07
```
