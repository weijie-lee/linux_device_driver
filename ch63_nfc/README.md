# NFC

## 代码知识点

NFC 控制器实现 `nfc_ops`：`dev_up`、`dev_down`、`start_poll`、`stop_poll`、`activate_target`、`deactivate_target`、`im_transceive`。`nfc_allocate_device()` 和 `nfc_register_device()` 注册。

这些回调都要在 ops 里，缺一个就会在注册时失败。本控制器不发起射频。

## 代码架构

```
nfc_ops（up/down/poll/activate/transceive）
nfc_allocate_device
nfc_register_device
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch63
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch63_nfc_virt_nfc.ko
insmod ok
+ lsmod | grep virt_nfc
virt_nfc               16384  0 
nfc                   126976  1 virt_nfc
+ find /sys /dev /proc -name '*virt_nfc*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_nfc
/sys/bus/platform/devices/virt_nfc
/sys/bus/platform/drivers/virt_nfc
/sys/bus/platform/drivers/virt_nfc/virt_nfc
/sys/module/nfc/holders/virt_nfc
/sys/module/virt_nfc
/sys/module/virt_nfc/drivers/platform:virt_nfc
+ rmmod virt_nfc
rmmod ok
[PASS] ch63_nfc
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch63_nfc`
- 能找到 `virt_nfc`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch63
```
