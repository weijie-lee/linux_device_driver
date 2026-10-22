# 硬件自旋锁

## 代码知识点

硬件自旋锁给多块处理器抢同一把锁。银行是 `hwspinlock_device`，`ops.trylock` 必须立刻返回，成功返回非 0。`hwspin_lock_register()` 登记这一排锁。

本锁用一个软件标志模拟。`trylock` 在已被占用时返回失败，`unlock` 放开。

## 代码架构

```
hwspinlock_ops
  trylock / unlock / relax
hwspin_lock_register
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch46
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch46_hwspinlock_virt_hwspin.ko
insmod ok
+ lsmod | grep virt_hwspin
virt_hwspin            16384  0 
+ find /sys /dev /proc -name '*virt_hwspin*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_hwspin
/sys/bus/platform/devices/virt_hwspin
/sys/bus/platform/drivers/virt_hwspin
/sys/bus/platform/drivers/virt_hwspin/virt_hwspin
/sys/module/virt_hwspin
/sys/module/virt_hwspin/drivers/platform:virt_hwspin
+ rmmod virt_hwspin
rmmod ok
[PASS] ch46_hwspinlock
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch46_hwspinlock`
- 能找到 `virt_hwspin`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch46
```
