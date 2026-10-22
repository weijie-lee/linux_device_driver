# 热区

## 代码知识点

热区用 `thermal_zone_device_register()` 注册。`ops.get_temp` 报告温度。5.15 上，如果 `ops->critical` 是空的，注册函数会往这个函数指针里写默认处理。ops 表因此不能放在 `const` 的只读段里，并且要自己提供 `critical`。

本热区的温度回调返回一个固定值。不要把 ops 写成 `const`。

## 代码架构

```
thermal_zone_device_ops
  get_temp
  critical   （必须有，ops 不能是 const）
thermal_zone_device_register("virt_thermal")
/sys/class/thermal/thermal_zoneN/type
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch29
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch29_thermal_virt_thermal.ko
insmod ok
+ lsmod | grep virt_thermal
virt_thermal           16384  0 
+ find /sys /dev /proc -name '*virt_thermal*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_thermal
/sys/bus/platform/devices/virt_thermal
/sys/bus/platform/drivers/virt_thermal
/sys/bus/platform/drivers/virt_thermal/virt_thermal
/sys/module/virt_thermal
/sys/module/virt_thermal/drivers/platform:virt_thermal
+ cat /sys/class/thermal/thermal_zone*/type
virt_thermal
+ rmmod virt_thermal
rmmod ok
[PASS] ch29_thermal
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch29_thermal`
- `thermal_zone*/type` 里有 `virt_thermal`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch29
```
