# rfkill

## 代码知识点

rfkill 是无线开关。`rfkill_alloc()` 指定类型（例如 WLAN），`rfkill_register()` 出现在 `/sys/class/rfkill/`。用户或平台策略调用 `ops.set_block`。

本开关只记一个布尔值。

## 代码架构

```
rfkill_alloc
ops.set_block
rfkill_register
/sys/class/rfkill/
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch45
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch45_rfkill_virt_rfkill.ko
insmod ok
+ lsmod | grep virt_rfkill
virt_rfkill            16384  0 
+ find /sys /dev /proc -name '*virt_rfkill*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_rfkill
/sys/bus/platform/devices/virt_rfkill
/sys/bus/platform/drivers/virt_rfkill
/sys/bus/platform/drivers/virt_rfkill/virt_rfkill
/sys/module/virt_rfkill
/sys/module/virt_rfkill/drivers/platform:virt_rfkill
+ rmmod virt_rfkill
rmmod ok
[PASS] ch45_rfkill
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch45_rfkill`
- 能找到 `virt_rfkill`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch45
```
