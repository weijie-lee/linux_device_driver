# 调试：dyndbg、ftrace，以及怎么读 oops

## dyndbg 和 ftrace

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

## 读 oops

一次 oops 先看这几项：出错指令在 `RIP`，访问的地址在 `CR2`，`Call Trace` 是从出错点回到系统调用或中断的路径。

`ch29_thermal` 曾在 QEMU 里缺页：`thermal_zone_device_register()` 在 `ops->critical` 为空时写入这个函数指针，而 `virt_ops` 被写成了 `const`，放在只读的 `.rodata`。写入只读页就会 oops。修法是提供 `virt_critical()`，并去掉 `const`。不要在教学模块里主动触发这个写入。

## kgdb

`run_qemu.sh` 的普通测试不加调试桩。要单步时，在 QEMU 参数里加上 `-s -S`（gdb 在 `tcp::1234`，`-S` 表示上电后先停住），串口改回终端。宿主机：

```
gdb vmlinux
target remote :1234
```

客户机内核是 Ubuntu 的 `vmlinuz`，没有带调试信息的 vmlinux 时，断点只能落到模块的 `add-symbol-file`。先在客户机里看 `/sys/module/virt_debug/sections/.text` 的地址，再把 `virt_debug.ko` 配这个地址加载进 gdb。
