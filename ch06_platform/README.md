# platform 总线

## 代码知识点

platform 用来描述不能自己枚举的片上设备。匹配靠名字：`platform_device` 和 `platform_driver` 的 `.name` 都是 `virt_plat` 时，总线调用 `probe`。没有设备树时，模块自己 `platform_device_alloc()` 再 `platform_device_add()`。

`probe` 里 `misc_register()` 做出 `/dev/virt_plat`。私有数据用 `platform_set_drvdata` 挂在设备上。`remove` 要按申请的反序放掉。

## 代码架构

```
module_init
  platform_device_alloc("virt_plat")
  platform_driver_register
    probe
      misc_register → /dev/virt_plat
      platform_set_drvdata
read/write
  container_of 从 miscdevice 找回 virt_plat_priv
remove
  misc_deregister
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch06
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch06_platform_platform_demo.ko
insmod ok
+ ls /dev/virt_plat /sys/bus/platform/devices
/dev/virt_plat

/sys/bus/platform/devices:
Fixed MDIO bus.0
PNP0103:00
QEMU0002:00
alarmtimer.0.auto
i8042
kgdboc
pcspkr
platform-framebuffer.0
reg-dummy
serial8250
virt_plat
+ test -c /dev/virt_plat -o -b /dev/virt_plat
node ok
+ echo hi > /dev/virt_plat
+ head -c 2 /dev/virt_plat
hi
+ /bin/test_platform
=== Ch06 Platform Device Driver Test ===

[TC01] open/close
  [PASS] open /dev/virt_plat
[TC02] write
  [PASS] write returns correct byte count
[TC03] read
  [PASS] read returns same byte count
  [PASS] read data matches written data

=== Test Summary ===
PASS: 4  FAIL: 0
+ rmmod platform_demo
rmmod ok
[PASS] ch06_platform
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch06_platform`
- `node ok`，`/dev/virt_plat` 读回 `hi`，sysfs 里能看到 `virt_plat`
- 用户态程序 `test_platform` 的结尾是 `PASS: 4  FAIL: 0`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch06
```
