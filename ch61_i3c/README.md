# I3C

## 代码知识点

I3C 主控制器实现 `i3c_master_controller_ops`：`bus_init`、`do_daa`（动态地址分配）、`send_ccc`、兼容的 I2C 传输。`i3c_master_register()` 注册。`i3c_master_set_info()` 必须在注册前填上主机自己的动态地址能力。

本主机不扫描真实设备。注册成功就是实验目标。

## 代码架构

```
i3c_master_controller
  ops.bus_init / do_daa / send_ccc
i3c_master_set_info
i3c_master_register
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch61
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch61_i3c_virt_i3c.ko
insmod ok
+ lsmod | grep virt_i3c
virt_i3c               16384  0 
i3c                    40960  1 virt_i3c
+ find /sys /dev /proc -name '*virt_i3c*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_i3c
/sys/bus/platform/devices/virt_i3c
/sys/bus/platform/drivers/virt_i3c
/sys/bus/platform/drivers/virt_i3c/virt_i3c
/sys/module/virt_i3c
/sys/module/virt_i3c/drivers/platform:virt_i3c
/sys/module/i3c/holders/virt_i3c
+ rmmod virt_i3c
rmmod ok
[PASS] ch61_i3c
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch61_i3c`
- 能找到 `virt_i3c`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch61
```
