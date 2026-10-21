# 通用 PHY

## 代码知识点

这里的 PHY 是 `drivers/phy` 的通用物理层（USB、PCIe、SATA 那类），不是 `ch14` 的以太网 PHY。提供者实现 `phy_ops` 的 `init` / `power_on` / `power_off`，用 `devm_phy_create()` 和 `devm_of_phy_provider_register()` 交出去。

没有设备树时，创建动作本身就说明控制器登记成功。以太网 MDIO PHY 在 `ch50_mdio`。

## 代码架构

```
devm_phy_create
phy_ops: init / power_on / power_off
phy_provider
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch32
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
+ insmod /lib/modules/driver_test/ch32_phy_virt_phy.ko
insmod ok
+ lsmod | grep virt_phy
virt_phy               16384  0 
+ find /sys /dev /proc -name '*virt_phy*' 2>/dev/null | head -n 30
/sys/class/phy/phy-virt_phy.0
/sys/devices/platform/virt_phy
/sys/devices/platform/virt_phy/phy/phy-virt_phy.0
/sys/bus/platform/devices/virt_phy
/sys/bus/platform/drivers/virt_phy
/sys/bus/platform/drivers/virt_phy/virt_phy
/sys/module/virt_phy
/sys/module/virt_phy/drivers/platform:virt_phy
+ rmmod virt_phy
rmmod ok
[PASS] ch32_phy
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] ch32_phy`
- 能找到 `virt_phy`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter ch32
```
