# 多路开关

## 代码知识点

多路开关把几路信号选到一路上。`mux_chip_alloc()` 分配，`ops.set` 在选通改变时被调用，`devm_mux_chip_register()` 注册。

本芯片记当前通道号。没有真实模拟开关。

## 代码架构

```
mux_chip
ops.set
devm_mux_chip_register
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch58
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch58_mux_virt_mux.ko
insmod ok
+ lsmod | grep virt_mux
virt_mux               16384  0 
mux_core               16384  1 virt_mux
+ find /sys /dev /proc -name '*virt_mux*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_mux
/sys/bus/platform/devices/virt_mux
/sys/bus/platform/drivers/virt_mux
/sys/bus/platform/drivers/virt_mux/virt_mux
/sys/module/mux_core/holders/virt_mux
/sys/module/virt_mux
/sys/module/virt_mux/drivers/platform:virt_mux
+ rmmod virt_mux
rmmod ok
[PASS] ch58_mux
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch58_mux`
- 能找到 `virt_mux`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch58
```
