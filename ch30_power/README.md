# 电源供应

## 代码知识点

电池和适配器走 power supply。`power_supply_desc` 列出属性，`get_property` 按 `POWER_SUPPLY_PROP_*` 填值。`devm_power_supply_register()` 在 `/sys/class/power_supply/` 下建目录。

本电池容量固定 77，电压 3800000 微伏，状态是放电，并且在位。

## 代码架构

```
power_supply_desc name = virt-battery
get_property
  CAPACITY = 77
  VOLTAGE_NOW = 3800000
devm_power_supply_register
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch30
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch30_power_virt_psy.ko
insmod ok
+ lsmod | grep virt_psy
virt_psy               16384  0 
+ find /sys /dev /proc -name '*virt-battery*' 2>/dev/null | head -n 30
/sys/class/power_supply/virt-battery
/sys/devices/platform/virt_psy/power_supply/virt-battery
+ cat /sys/class/power_supply/virt-battery/capacity
77
+ rmmod virt_psy
rmmod ok
[PASS] ch30_power
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch30_power`
- `/sys/class/power_supply/virt-battery/capacity` 是 `77`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch30
```
