# mailbox

## 代码知识点

mailbox 是一块处理器向另一块发短消息的控制器。`mbox_controller` 加 `mbox_chan_ops.send_data`，`devm_mbox_controller_register()` 注册。对端用 `mbox_send_message()`。

本通道的 `send_data` 接受指针并返回 0，不把消息送到第二块 CPU。

## 代码架构

```
mbox_controller
  ops.send_data
devm_mbox_controller_register
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch47
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch47_mailbox_virt_mbox.ko
insmod ok
+ lsmod | grep virt_mbox
virt_mbox              16384  0 
+ find /sys /dev /proc -name '*virt_mbox*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_mbox
/sys/bus/platform/devices/virt_mbox
/sys/bus/platform/drivers/virt_mbox
/sys/bus/platform/drivers/virt_mbox/virt_mbox
/sys/module/virt_mbox
/sys/module/virt_mbox/drivers/platform:virt_mbox
+ rmmod virt_mbox
rmmod ok
[PASS] ch47_mailbox
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch47_mailbox`
- 能找到 `virt_mbox`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch47
```
