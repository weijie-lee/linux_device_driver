# MDIO

## 代码知识点

MDIO 是 MAC 访问以太网 PHY 寄存器的总线。`mii_bus` 填 `read` 和 `write`，`mdiobus_register()` 注册。PHY 驱动随后可以挂到某个地址。

本总线的读写打到一块假寄存器。它和 `ch32_phy` 的通用 PHY 框架不是同一个子系统。

## 代码架构

```
mii_bus name = virt-mdio
  read / write
mdiobus_register
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch50
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch50_mdio_virt_mdio.ko
insmod ok
+ lsmod | grep virt_mdio
virt_mdio              16384  0 
+ find /sys /dev /proc -name '*virt-mdio*' 2>/dev/null | head -n 30
/sys/class/mdio_bus/virt-mdio
/sys/devices/platform/virt_mdio/mdio_bus/virt-mdio
+ rmmod virt_mdio
rmmod ok
[PASS] ch50_mdio
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch50_mdio`
- 能找到 `virt-mdio`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch50
```
