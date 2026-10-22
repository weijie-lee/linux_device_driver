# regulator

## 代码知识点

稳压器实现 `regulator_ops`，用 `devm_regulator_register()` 注册。本电源只有一档电压 1.8 V。`enable` / `disable` 改私有的 `enabled` 标志，`get_voltage` 固定返回 1800000 微伏。

约束在 `regulator_init_data` 里，允许改开关，不允许改电压。sysfs 在 `/sys/class/regulator/regulatorN/`。

## 代码架构

```
regulator_desc name = virt_vdd_1v8
  fixed_uV = 1800000
  ops: enable / disable / is_enabled / get_voltage
devm_regulator_register
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch24
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch24_regulator_virt_regulator.ko
insmod ok
+ lsmod | grep virt_regulator
virt_regulator         16384  0 
+ find /sys /dev /proc -name '*virt_vdd_1v8*' 2>/dev/null | head -n 30
/sys/kernel/debug/regulator/virt_regulator-virt_vdd_1v8
+ cat /sys/class/regulator/regulator*/microvolts
1800000
+ rmmod virt_regulator
rmmod ok
[PASS] ch24_regulator
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch24_regulator`
- `microvolts` 为 `1800000`，名字是 `virt_vdd_1v8`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch24
```
