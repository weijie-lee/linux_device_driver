# 把 regmap、时钟和中断放进同一次 probe

前面几章各自注册一个虚拟设备。真实的 SoC 驱动在一次 `probe` 里把它们接上：

1. `devm_regmap_init` 读写 ID、CTRL、TEMP、THRESHOLD。
2. `devm_clk_hw_register_fixed_rate` 提供 24 MHz 时钟，`devm_clk_hw_register_clkdev` 把它登记为连接名 `sensor`，驱动再用 `clk_get(dev, "sensor")` 和 `clk_prepare_enable()`。这是消费者侧的查找。频率读进 `clock_hz` 之后立刻 `clk_put`。时钟的 owner 是本模块，一直拿着引用的话 `rmmod` 会返回 EAGAIN，`exit` 没机会运行。
3. 与 `learn/03_irq` 相同的 irqdomain。写 `temperature` 后在硬中断里触发，threaded handler 里 `regmap_read`。上半部只返回 `IRQ_WAKE_THREAD`。

`virt-sensor.dts` 是同一块硬件在设备树上的写法（`reg`、`interrupts`、`clocks`）。本客户机没有 `CONFIG_OF`，所以绑定仍然靠平台设备名。

```bash
echo 37 > /sys/devices/platform/virt_sensor/temperature
cat /sys/devices/platform/virt_sensor/temperature    # 37
cat /sys/devices/platform/virt_sensor/clock_hz       # 24000000
cat /sys/devices/platform/virt_sensor/irq_count      # 至少 1
cat /sys/devices/platform/virt_sensor/last_temp      # handler 读到的 37
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter learn04
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/learn_04_soc_sensor_virt_sensor.ko
insmod ok
+ echo 37 > /sys/devices/platform/virt_sensor/temperature
+ cat /sys/devices/platform/virt_sensor/temperature /sys/devices/platform/virt_sensor/clock_hz /sys/devices/platform/virt_sensor/irq_count /sys/devices/platform/virt_sensor/last_temp
37
24000000
1
37
+ rmmod virt_sensor
rmmod ok
[PASS] learn04_sensor
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] learn04_sensor`
- 温度是 `37`，`clock_hz` 是 `24000000`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter learn04
```
