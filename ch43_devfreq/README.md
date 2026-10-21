# devfreq

## 代码知识点

devfreq 按负载调设备频率。`devfreq_dev_profile` 给出频率表和 `target` 回调，`devfreq_add_device()` 注册。改频前通常要有 OPP 表，`dev_pm_opp_add()` 把频率放进去。

本设备有一张很小的频率表。`target` 记下被选中的频率。OPP 的单独一章是 `ch49_opp`。

## 代码架构

```
dev_pm_opp_add
devfreq_dev_profile.target
devfreq_add_device
/sys/class/devfreq/
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch43
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch43_devfreq_virt_devfreq.ko
insmod ok
+ lsmod | grep virt_devfreq
virt_devfreq           16384  0 
+ find /sys /dev /proc -name '*virt_devfreq*' 2>/dev/null | head -n 30
/sys/kernel/debug/opp/platform-virt_devfreq
/sys/class/devfreq/virt_devfreq
/sys/devices/platform/virt_devfreq
/sys/devices/platform/virt_devfreq/devfreq/virt_devfreq
/sys/bus/platform/devices/virt_devfreq
/sys/bus/platform/drivers/virt_devfreq
/sys/bus/platform/drivers/virt_devfreq/virt_devfreq
/sys/module/virt_devfreq
/sys/module/virt_devfreq/drivers/platform:virt_devfreq
+ rmmod virt_devfreq
rmmod ok
[PASS] ch43_devfreq
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch43_devfreq`
- 能找到 `virt_devfreq`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch43
```
