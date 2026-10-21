# extcon

## 代码知识点

extcon 报告线缆状态，例如 USB 或耳机是否插入。`devm_extcon_dev_allocate()` 列出支持的线缆类型，`devm_extcon_dev_register()` 注册。状态变化用 `extcon_set_state_sync()`。

本模块注册一种线缆并设为连接。sysfs 在 `/sys/class/extcon/`。

## 代码架构

```
devm_extcon_dev_allocate
devm_extcon_dev_register
extcon_set_state_sync
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch38
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch38_extcon_virt_extcon.ko
insmod ok
+ lsmod | grep virt_extcon
virt_extcon            16384  0 
+ find /sys /dev /proc -name '*virt_extcon*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_extcon
/sys/bus/platform/devices/virt_extcon
/sys/bus/platform/drivers/virt_extcon
/sys/bus/platform/drivers/virt_extcon/virt_extcon
/sys/module/virt_extcon
/sys/module/virt_extcon/drivers/platform:virt_extcon
+ rmmod virt_extcon
rmmod ok
[PASS] ch38_extcon
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch38_extcon`
- 能找到 `virt_extcon`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch38
```
