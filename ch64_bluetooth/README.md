# 蓝牙 HCI

## 代码知识点

蓝牙控制器从 HCI 驱动挂上。`hci_alloc_dev()` 分配，`hci_register_dev()` 注册。`send` 把 HCI 命令或 ACL 数据交给传输。

本传输把包接受下来并计一次发送，不建立空口链路。

## 代码架构

```
hci_alloc_dev
hci_register_dev
send → 接受 skb
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch64
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch64_bluetooth_virt_bt.ko
insmod ok
+ lsmod | grep virt_bt
virt_bt                16384  0 
bluetooth             675840  2 virt_bt
+ find /sys /dev /proc -name '*virt_bt*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_bt
/sys/bus/platform/devices/virt_bt
/sys/bus/platform/drivers/virt_bt
/sys/bus/platform/drivers/virt_bt/virt_bt
/sys/module/virt_bt
/sys/module/virt_bt/drivers/platform:virt_bt
/sys/module/bluetooth/holders/virt_bt
+ rmmod virt_bt
rmmod ok
[PASS] ch64_bluetooth
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch64_bluetooth`
- 能找到 `virt_bt`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch64
```
