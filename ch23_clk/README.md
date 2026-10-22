# 时钟

## 代码知识点

时钟提供者注册 `struct clk_hw`。固定时钟用 `clk_hw_register_fixed_rate()`，名字 `virt_fixed_24m`，频率 24000000 Hz。消费者以后用 `clk_get` 和 `clk_prepare_enable`。

本模块只做提供者，不自己 `clk_get`。消费者那条路在 `learn/04_soc_sensor`。

## 代码架构

```
probe
  devm_clk_hw_register_fixed_rate("virt_fixed_24m", 24000000)
dmesg: virt_fixed_24m = 24 MHz
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch23
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch23_clk_virt_clk.ko
insmod ok
+ lsmod | grep virt_clk
virt_clk               16384  0 
+ find /sys /dev /proc -name '*virt_fixed_24m*' 2>/dev/null | head -n 30
/sys/kernel/debug/clk/virt_fixed_24m
+ dmesg | grep virt_fixed_24m
[   10.571065] virt_clk virt_clk: virt_fixed_24m = 24 MHz
+ rmmod virt_clk
rmmod ok
[PASS] ch23_clk
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch23_clk`
- dmesg 有 `virt_fixed_24m = 24 MHz`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch23
```
