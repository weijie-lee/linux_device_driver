# counter

## 代码知识点

counter 子系统描述计数器和信号。`counter_device` 里有 `counter_count`，`count_read` 返回计数值。`devm_counter_add()` 注册。

本计数器一条信号 `virt_sig`，读数是驱动里的整数。sysfs 在 counter 总线下。

## 代码架构

```
counter_count name = virt_count
signal name = virt_sig
count_read
devm_counter_add
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch66
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch66_counter_virt_counter.ko
insmod ok
+ lsmod | grep virt_counter
virt_counter           16384  0 
counter                20480  1 virt_counter
+ find /sys /dev /proc -name '*virt_count*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_counter
/sys/bus/platform/devices/virt_counter
/sys/bus/platform/drivers/virt_counter
/sys/bus/platform/drivers/virt_counter/virt_counter
/sys/module/virt_counter
/sys/module/virt_counter/drivers/platform:virt_counter
/sys/module/counter/holders/virt_counter
+ rmmod virt_counter
rmmod ok
[PASS] ch66_counter
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch66_counter`
- 能找到 `virt_count`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch66
```
