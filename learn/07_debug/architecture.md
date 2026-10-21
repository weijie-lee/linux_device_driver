# 调试入口

```mermaid
graph TD
    t["sysfs trigger"] --> p["virt_debug_ping"]
    p --> info["pr_info 总是进 dmesg"]
    p --> dbg["pr_debug 要 dyndbg +p"]
    p --> ft["ftrace function tracer"]
```
