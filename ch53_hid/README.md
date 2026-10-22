# HID

## 代码知识点

HID 驱动解析报告描述符，把输入报告变成 input 事件。低层传输（USB、I2C）向 HID 核心提供 `hid_ll_driver`。`hid_allocate_device()`、`hid_add_device()` 把设备放进 HID 总线。

本设备带一份很小的报告描述符，`raw_request` 返回空报告。它说明 HID 核心接受了这块设备。

## 代码架构

```
hid_allocate_device
hid_ll_driver: parse / start / stop / raw_request
hid_add_device
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch53
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch53_hid_virt_hid.ko
insmod ok
+ lsmod | grep virt_hid
virt_hid               16384  0 
hid                   151552  1 virt_hid
+ find /sys /dev /proc -name '*virt_hid*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_hid
/sys/bus/platform/devices/virt_hid
/sys/bus/platform/drivers/virt_hid
/sys/bus/platform/drivers/virt_hid/virt_hid
/sys/module/hid/holders/virt_hid
/sys/module/virt_hid
/sys/module/virt_hid/drivers/platform:virt_hid
+ rmmod virt_hid
rmmod ok
[PASS] ch53_hid
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch53_hid`
- 能找到 `virt_hid`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch53
```
