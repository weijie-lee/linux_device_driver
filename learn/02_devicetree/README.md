# 设备树资源

## 代码知识点

SoC 上的平台设备没有 PCI 那种枚举。板级描述写在设备树里，内核把它变成 `struct resource`，probe 再用 `platform_get_resource` 取出来。

`virt-reg.dts` 里这个节点：

```
compatible = "ldd,virt-reg";
reg = <0x0 0x10000000 0x0 0x1000>;
interrupts = <19>;
ldd,threshold = <40>;
```

对应驱动里的三件事：

| 设备树 | 内核里 |
| --- | --- |
| `compatible` | `of_device_id` / `of_match_table`，没有 OF 时靠驱动名 `virt_dt` 绑定 |
| `reg` | `IORESOURCE_MEM`。本实验用一页 RAM 的物理地址代替 `0x10000000`，probe 里 `memremap` 后 `readl`/`writel` |
| `interrupts = <19>` | `IORESOURCE_IRQ` 的 start。这里只读编号，不 `request_irq`，避免占用客户机上真实的 19 号中断 |
| `ldd,threshold` | `of_property_read_u32`。本内核 `CONFIG_OF` 未打开，该调用走头文件里的桩并返回错误，驱动改用 `platform_data` 里的同一个值 40 |

打开 `CONFIG_OF` 的内核会在 probe 之前根据 dtb 填好 `dev.of_node` 和 resource，驱动的 probe 不用改。当前 QEMU 客户机是 x86 的 Ubuntu 通用内核，启动时不接收 `-dtb`。

```bash
dtc -I dts -O dtb -o /tmp/virt-reg.dtb virt-reg.dts
make
```

加载后：

```bash
cat /sys/devices/platform/virt_dt/threshold   # 40
cat /sys/devices/platform/virt_dt/id          # 4c444454
cat /sys/devices/platform/virt_dt/irq         # 19
```

`ch06_platform` 只演示 platform 总线的注册。资源布局以这一章为准。

## 代码架构

```
insmod virt_dt.ko
  platform_driver_register(virt_dt)
  platform_device_register_full      带 IORESOURCE_MEM 和 IORESOURCE_IRQ 两项 resource
    probe
      platform_get_resource(MEM)     → devm_memremap
      platform_get_resource(IRQ)     → irq = 19，只读取
      of_property_read_u32           没有 OF 时失败，改用 platform_data 里的 40
      writel / readl                 校验 id 和 threshold
      device_create_file             threshold、id、irq
rmmod virt_dt
  platform_device_unregister → platform_driver_unregister
```

## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter learn02
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。方括号里的时间戳、内存地址和 `lsmod` 里的数字每次都不同，对照命令和关键字即可。

```
+ insmod /lib/modules/driver_test/learn_02_devicetree_virt_dt.ko
insmod ok
+ cat /sys/devices/platform/virt_dt/threshold /sys/devices/platform/virt_dt/id /sys/devices/platform/virt_dt/irq
40
4c444454
19
+ rmmod virt_dt
rmmod ok
[PASS] learn02_devicetree
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] learn02_devicetree`
- 三行依次是 `40`、`4c444454`、`19`。

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter learn02
```
