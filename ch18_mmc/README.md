# MMC 主机

## 代码知识点

MMC/SD 主机实现 `mmc_host_ops`，`mmc_alloc_host()` 分配，`mmc_add_host()` 让核心开始探测卡。探测是异步的，所以插入模块后要等一下才看得到 `mmc_host`。

本主机的卡是一块内存。容量在加载日志里。卡检测完成后可能出现 `/dev/mmcblk0`，主机目录 `/sys/class/mmc_host/mmcN` 一定先出现。

## 代码架构

```
platform 设备 mmc_virt
mmc_alloc_host
mmc_add_host
  核心异步探测
/sys/class/mmc_host/mmcN
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch18
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch18_mmc_mmc_virt.ko
insmod ok
+ ls /sys/class/mmc_host /dev
/dev:
console
cpu_dma_latency
dma_heap
ecryptfs
full
fuse
hpet
hwrng
input
kmsg
loop-control
loop0
loop1
loop2
loop3
loop4
loop5
loop6
loop7
mapper
mcelog
mem
net
null
port
ppp
psaux
ptmx
pts
random
rfkill
rtc0
snapshot
snd
tty
tty0
tty1
tty10
tty11
tty12
tty13
tty14
tty15
tty16
tty17
tty18
tty19
tty2
tty20
tty21
tty22
tty23
tty24
tty25
tty26
tty27
tty28
tty29
tty3
tty30
tty31
tty32
tty33
tty34
tty35
tty36
tty37
tty38
tty39
tty4
tty40
tty41
tty42
tty43
tty44
tty45
tty46
tty47
tty48
tty49
tty5
tty50
tty51
tty52
tty53
tty54
tty55
tty56
tty57
tty58
tty59
tty6
tty60
tty61
tty62
tty63
tty7
tty8
tty9
ttyS0
ttyS1
ttyS10
ttyS11
ttyS12
ttyS13
ttyS14
ttyS15
ttyS16
ttyS17
ttyS18
ttyS19
ttyS2
ttyS20
ttyS21
ttyS22
ttyS23
ttyS24
ttyS25
ttyS26
ttyS27
ttyS28
ttyS29
ttyS3
ttyS30
ttyS31
ttyS4
ttyS5
ttyS6
ttyS7
ttyS8
ttyS9
ttyprintk
udmabuf
uinput
urandom
vcs
vcs1
vcsa
vcsa1
vcsu
vcsu1
vfio
vga_arbiter
zero

/sys/class/mmc_host:
mmc0
+ rmmod mmc_virt
rmmod ok
[PASS] ch18_mmc
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch18_mmc`
- `/sys/class/mmc_host` 里出现 `mmc`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch18
```
