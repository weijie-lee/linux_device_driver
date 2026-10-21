# 设备树到 probe

```mermaid
graph LR
    dts["virt-reg.dts"] --> res["resource 数组"]
    res --> pdev["platform_device"]
    pdev --> probe["virt_dt_probe"]
    probe --> mmio["memremap 后的 readl/writel"]
```

有 `CONFIG_OF` 时，左边那一步由内核的 OF 代码完成。本客户机没有 OF，模块按 dts 里的编号自己填 resource。
