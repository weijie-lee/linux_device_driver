# powercap

## 代码知识点

powercap 给功耗封顶做一类 sysfs。`powercap_register_control_type()` 注册控制类型，再 `powercap_register_zone()` 加区域。区域可以有能耗计数和功率上限。

本区域名字带 `virt_pcap`。读到的是驱动里的计数，不是实测功率。

## 代码架构

```
powercap_register_control_type
powercap_register_zone("virt_pcap")
/sys/class/powercap/
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch48
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch48_powercap_virt_powercap.ko
insmod ok
+ lsmod | grep virt_powercap
virt_powercap          16384  0 
+ find /sys /dev /proc -name '*virt_pcap*' 2>/dev/null | head -n 30
/sys/class/powercap/virt_pcap
/sys/devices/virtual/powercap/virt_pcap
+ rmmod virt_powercap
rmmod ok
[PASS] ch48_powercap
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch48_powercap`
- 能找到 `virt_pcap`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch48
```
