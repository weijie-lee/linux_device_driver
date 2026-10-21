# reset 控制器

## 代码知识点

复位控制器是 `reset_controller_dev`。`ops.reset` 对某一路执行复位，`ops.assert` / `deassert` 分开拉住和放开。消费者用 `devm_reset_control_get()` 拿到 `reset_control`。

本模块登记一路软件复位线，状态记在驱动里。没有外设会被真正清掉。

## 代码架构

```
reset_controller_dev
  nr_resets = 1
  ops: reset / assert / deassert
devm_reset_controller_register
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch25
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch25_reset_virt_reset.ko
insmod ok
+ lsmod | grep virt_reset
virt_reset             16384  0 
+ find /sys /dev /proc -name '*virt_reset*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_reset
/sys/bus/platform/devices/virt_reset
/sys/bus/platform/drivers/virt_reset
/sys/bus/platform/drivers/virt_reset/virt_reset
/sys/module/virt_reset
/sys/module/virt_reset/drivers/platform:virt_reset
+ rmmod virt_reset
rmmod ok
[PASS] ch25_reset
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch25_reset`
- 能找到 `virt_reset` 这个 platform 设备。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch25
```
