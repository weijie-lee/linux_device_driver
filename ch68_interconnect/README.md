# interconnect

## 代码知识点

interconnect 描述片上总线的带宽请求。提供者实现 `icc_provider`，`set` 在聚合带宽变化时被调用。节点用 `icc_node_create()` 建，`icc_link_create()` 连边，`icc_provider_register()` 注册。

本提供者两节点一条链路。`set` 记下带宽，不改时钟。

## 代码架构

```
icc_provider
  set / aggregate / xlate
icc_node_create × 2
icc_link_create
icc_provider_register
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch68
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/ch68_interconnect_virt_icc.ko
insmod ok
+ lsmod | grep virt_icc
virt_icc               16384  0 
+ find /sys /dev /proc -name '*virt_icc*' 2>/dev/null | head -n 30
/sys/devices/platform/virt_icc
/sys/bus/platform/devices/virt_icc
/sys/bus/platform/drivers/virt_icc
/sys/bus/platform/drivers/virt_icc/virt_icc
/sys/module/virt_icc
/sys/module/virt_icc/drivers/platform:virt_icc
+ rmmod virt_icc
rmmod ok
[PASS] ch68_interconnect
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch68_interconnect`
- 能找到 `virt_icc`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch68
```
