# 把 regmap、时钟和中断放进同一次 probe

前面几章各自注册一个虚拟设备。真实的 SoC 驱动在一次 `probe` 里把它们接上：

1. `devm_regmap_init` 读写 ID、CTRL、TEMP、THRESHOLD。
2. `devm_clk_hw_register_fixed_rate` 提供 24 MHz 时钟，`devm_clk_hw_register_clkdev` 把它登记为连接名 `sensor`，驱动再用 `devm_clk_get(dev, "sensor")` 和 `clk_prepare_enable()`。这是消费者侧的查找，不是直接拿 `clk_hw`。
3. 与 `learn/03_irq` 相同的 irqdomain。写 `temperature` 后在硬中断里触发，threaded handler 里 `regmap_read`。上半部只返回 `IRQ_WAKE_THREAD`。

`virt-sensor.dts` 是同一块硬件在设备树上的写法（`reg`、`interrupts`、`clocks`）。本客户机没有 `CONFIG_OF`，所以绑定仍然靠平台设备名。

```bash
echo 37 > /sys/devices/platform/virt_sensor/temperature
cat /sys/devices/platform/virt_sensor/temperature    # 37
cat /sys/devices/platform/virt_sensor/clock_hz       # 24000000
cat /sys/devices/platform/virt_sensor/irq_count      # 至少 1
cat /sys/devices/platform/virt_sensor/last_temp      # handler 读到的 37
```
