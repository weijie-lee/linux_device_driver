# pinctrl

## 代码知识点

引脚控制器告诉 pinctrl 核心有哪些引脚、哪些组。`pinctrl_pin_desc` 列出 `virt_gpio0`、`virt_gpio1`、`virt_i2c_scl`、`virt_i2c_sda`。一组叫 `virt_all`，包含这四根。`pinctrl_register_and_init()` 加上 `pinctrl_enable()` 完成注册。

这一章只登记引脚，不改复用功能。GPIO 的电平仍看 `ch21_gpio`。

## 代码架构

```
pinctrl_pin_desc[4]
pinctrl_ops：组名 virt_all
devm_pinctrl_register_and_init
pinctrl_enable
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch22
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch22_pinctrl_virt_pinctrl.ko
insmod ok
+ lsmod | grep virt_pinctrl
virt_pinctrl           16384  0 
+ find /sys /dev /proc -name '*virt_pinctrl*' 2>/dev/null | head -n 30
/sys/kernel/debug/pinctrl/virt_pinctrl
/sys/devices/platform/virt_pinctrl
/sys/bus/platform/devices/virt_pinctrl
/sys/bus/platform/drivers/virt_pinctrl
/sys/bus/platform/drivers/virt_pinctrl/virt_pinctrl
/sys/module/virt_pinctrl
/sys/module/virt_pinctrl/drivers/platform:virt_pinctrl
+ rmmod virt_pinctrl
rmmod ok
[PASS] ch22_pinctrl
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch22_pinctrl`
- sysfs 或 debugfs 里能找到 `virt_pinctrl`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch22
```
