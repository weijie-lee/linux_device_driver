# LED

## 代码知识点

LED 类设备是 `led_classdev`。`brightness_set` 在用户写入 `brightness` 时被调用。名字 `virt:status` 会出现在 `/sys/class/leds/virt:status/`。`max_brightness` 是 255。

本回调只把亮度记在 `cdev->brightness`，没有 GPIO 去点灯。

## 代码架构

```
led_classdev
  name = virt:status
  max_brightness = 255
  brightness_set
devm_led_classdev_register
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch28
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch28_leds_virt_led.ko
insmod ok
+ lsmod | grep virt_led
virt_led               16384  0 
+ find /sys /dev /proc -name '*virt:status*' 2>/dev/null | head -n 30
/sys/class/leds/virt:status
/sys/devices/platform/virt_led/leds/virt:status
+ cat /sys/class/leds/virt:status/max_brightness
255
+ rmmod virt_led
rmmod ok
[PASS] ch28_leds
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch28_leds`
- `/sys/class/leds/virt:status/max_brightness` 是 `255`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch28
```
