# GPIO 控制器

## 代码知识点

GPIO 控制器是 `struct gpio_chip`。要实现 `get`、`set`、`direction_input`、`direction_output`，再 `gpiochip_add_data()`。`base = -1` 表示编号由 gpiolib 分配。

8 根线的方向在 `out[]`，电平在 `val[]`。没有引脚，值只活在这张表里。`/sys/class/gpio/gpiochipN` 是这颗控制器。

## 代码架构

```
platform_device_alloc("virt_gpio")
platform_driver_register
  probe
    gpiochip_add_data
      ngpio = 8，base = -1
remove
  gpiochip_remove
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch21
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch21_gpio_virt_gpio.ko
insmod ok
+ lsmod | grep virt_gpio
virt_gpio              16384  0 
+ find /sys /dev /proc -name '*virt_gpio*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_gpio
/sys/bus/platform/devices/virt_gpio
/sys/bus/platform/drivers/virt_gpio
/sys/bus/platform/drivers/virt_gpio/virt_gpio
/sys/module/virt_gpio
/sys/module/virt_gpio/drivers/platform:virt_gpio
+ rmmod virt_gpio
rmmod ok
[PASS] ch21_gpio
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch21_gpio`
- `find` 的结果里有 `virt_gpio`，`lsmod` 能看到模块。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch21
```
