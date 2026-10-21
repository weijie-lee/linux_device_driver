#!/usr/bin/env python3
"""Fill each chapter README with the guest transcript from a chapter lab run."""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
LOG = ROOT / "qemu_test/output/qemu_serial.log"

LAB = """## 在镜像里复现

模块按 `5.15.0-173-generic` 编译，只在 QEMU 客户机里 `insmod`。容器里的 `uname -r` 是宿主机内核，不要在容器里加载这些 `.ko`。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter {cmd}
```

`chapter` 会编译并打包 initramfs，启动客户机，执行下面这些命令，并把回显打到终端。下面这份回显来自镜像里的一次实际运行。

```
{tx}
```

实验成功要同时满足：

- 这条 `docker run` 的退出码是 0
- 回显里有 `[PASS] {label}`
- {success}

想把 `qemu_test/output/qemu_serial.log` 留在本机时，把仓库挂进容器：

```bash
docker run --rm -v "$PWD":/linux_device_driver ldd-lab chapter {cmd}
```
"""


def transcripts(text):
    found = {}
    for m in re.finditer(
        r"======== (\S+) ========\n(.*?)(?=\n======== |\n={10,}|\n TEST RESULTS|\Z)",
        text,
        re.S,
    ):
        found[m.group(1)] = m.group(2).strip()
    return found


def lab_section(cmd, label, success, tx):
    if not tx:
        raise SystemExit(f"missing transcript for {label}")
    return LAB.format(cmd=cmd, label=label, success=success, tx=tx)


def replace_whole(path, body):
    path.write_text(body.rstrip() + "\n", encoding="utf-8")


def splice_before(path, marker, block):
    text = path.read_text(encoding="utf-8")
    if "## 在镜像里复现" in text:
        text = re.sub(r"\n## 在镜像里复现\n.*", "\n", text, flags=re.S)
    i = text.find(marker)
    if i < 0:
        raise SystemExit(f"{path} missing {marker}")
    text = text[:i] + block.rstrip() + "\n\n" + text[i:]
    path.write_text(text, encoding="utf-8")


def replace_span(path, start, end, block):
    text = path.read_text(encoding="utf-8")
    i = text.find(start)
    j = text.find(end)
    if i < 0 or j < 0 or j < i:
        raise SystemExit(f"{path} span {start} .. {end}")
    text = text[:i] + block.rstrip() + "\n\n" + text[j:]
    path.write_text(text, encoding="utf-8")


def page(title, knowledge, arch, cmd, label, success, tx, note=""):
    extra = f"\n{note.strip()}\n" if note else ""
    return f"""# {title}

## 代码知识点

{knowledge.strip()}

## 代码架构

{arch.strip()}
{extra}
{lab_section(cmd, label, success, tx)}
"""


# cmd, label, dir, title, success, knowledge, arch, note
VIRT = []


def add(cmd, label, directory, title, success, knowledge, arch, note=""):
    VIRT.append((cmd, label, directory, title, success, knowledge, arch, note))


add(
    "ch04", "ch04_second", "ch04_timer", "内核定时器：/dev/second",
    "读到的秒数大于等于 1，并且 `rmmod second` 打印 `rmmod ok`。",
    """`struct timer_list` 挂在内核定时器轮上。到期后在软中断里调用回调。`timer_setup()` 绑定回调，`add_timer()` 开始计时，周期定时器在回调里 `mod_timer(jiffies + HZ)`。回调不能睡眠。秒数用 `atomic_t` 累加，因为回调和 `read` 可能在不同 CPU 上。

`/dev/second` 的 `read` 用 `put_user()` 把这个整数交给用户空间，所以它是 4 字节二进制，不是 ASCII。`open` 时把计数清零并 `add_timer`。`release` 必须 `del_timer_sync()`，等回调结束再返回，避免卸载后回调还在跑。""",
    """```
insmod second.ko
  alloc_chrdev_region → cdev_add → class_create → device_create("/dev/second")
open
  timer_setup + add_timer，counter = 0
定时器回调（每秒）
  atomic_inc + mod_timer
read
  put_user(counter)
release
  del_timer_sync
```

本驱动没有打开计数。定时器还活着时再 `open` 一次会把同一个 `timer_list` 重复挂上链表。实验只打开一次，等两秒，从同一个文件描述符里读。""",
)

add(
    "ch05", "ch05_misc", "ch05_misc", "misc 杂项设备",
    "`head` 读回 `miscdata`，并且 `/dev/misc_demo` 存在。",
    """杂项设备共用主设备号 10。`misc_register()` 一次做完次设备号、`cdev` 和 `/dev` 节点，不用自己 `alloc_chrdev_region` 和 `class_create`。适合只有一个节点的小设备。

本模块的缓冲区在 `misc_demo_priv` 里，`mutex` 盖住读写。`write` 覆盖整段缓冲并记下长度，`read` 按 `*ppos` 往后读，读完返回 0。""",
    """```
misc_register(miscdevice)
  .name = misc_demo → /dev/misc_demo
  .fops = open/read/write/ioctl
write → 拷进内核缓冲
read  → copy_to_user，直到 data_len
rmmod → misc_deregister
```""",
)

add(
    "ch06", "ch06_platform", "ch06_platform", "platform 总线",
    "`/dev/virt_plat` 读回 `hi`，并且 sysfs 里能看到 `virt_plat`。",
    """platform 用来描述不能自己枚举的片上设备。匹配靠名字：`platform_device` 和 `platform_driver` 的 `.name` 都是 `virt_plat` 时，总线调用 `probe`。没有设备树时，模块自己 `platform_device_alloc()` 再 `platform_device_add()`。

`probe` 里 `misc_register()` 做出 `/dev/virt_plat`。私有数据用 `platform_set_drvdata` 挂在设备上。`remove` 要按申请的反序放掉。""",
    """```
module_init
  platform_device_alloc("virt_plat")
  platform_driver_register
    probe
      misc_register → /dev/virt_plat
      platform_set_drvdata
read/write
  container_of 从 miscdevice 找回 virt_plat_priv
remove
  misc_deregister
```""",
)

add(
    "ch07", "ch07_input", "ch07_input", "input 子系统",
    "`/dev/input` 里出现 `event`，并且 dmesg 有 `reported key 30`。",
    """输入设备不自己发明记录格式。驱动填 `input_dev` 的事件位图，用 `input_report_key()` 加上 `input_sync()` 上报。用户空间读 `/dev/input/eventN`，每条是 `struct input_event`。

本模块注册一块虚拟键盘，再在 `/proc/virt_kbd` 上收一个键码。写入 `30` 就是上报键 30 的按下和松开。""",
    """```
input_allocate_device
  set_bit(EV_KEY)
  set_bit(KEY_A) 等
input_register_device → /dev/input/eventN
/proc/virt_kbd 的 write
  input_report_key(press) + input_sync
  input_report_key(release) + input_sync
```""",
)

add(
    "ch08", "ch08_regmap", "ch08_regmap", "regmap",
    "`/dev/regmap_demo` 存在，并且 dmesg 里有 `initialized`。",
    """regmap 把寄存器读写收成 `regmap_read` / `regmap_write` / `regmap_update_bits`。底层可以是 I2C、SPI 或 MMIO。本模块没有总线，用 `regmap_init()` 配一对自定义 `reg_read` / `reg_write`，寄存器是一块内存数组，寄存器 N 的初值是 `N * 0x10`。

debugfs 在 `/sys/kernel/debug/regmap/regmap_demo/registers`。用户程序走 `/dev/regmap_demo` 的 ioctl。""",
    """```
kzalloc 寄存器数组
regmap_init(自定义 bus)
misc_register → /dev/regmap_demo
ioctl
  regmap_read / regmap_write
rmmod
  regmap_exit + misc_deregister
```""",
)

add(
    "ch09", "ch09_watchdog", "ch09_watchdog", "看门狗",
    "`/dev/watchdog` 存在，dmesg 有 `registered`。不要在这个实验里打开这个设备节点。",
    """看门狗框架是 `struct watchdog_device` 加 `watchdog_ops`。`start` 开始倒计时，`ping` 喂狗，`set_timeout` 改超时，`stop` 停止。`watchdog_register_device()` 创建 `/dev/watchdogN`。

打开这个节点就会 start。本客户机里如果只打开、不喂狗，超时后驱动会走复位路径。实验只确认节点和注册日志，不 `open`。""",
    """```
watchdog_register_device
  ops: start / stop / ping / set_timeout
  /dev/watchdog0
rmmod
  watchdog_unregister_device
```""",
)

add(
    "ch10", "ch10_rtc", "ch10_rtc", "RTC",
    "`/sys/class/rtc/rtc*/name` 里有 `virt_rtc`，并且 `/dev/rtc0` 存在。",
    """RTC 框架用 `rtc_device_register()`（或 `devm_rtc_allocate_device` + `rtc_register_device`）把驱动挂到 `/dev/rtcN`。用户空间用 `RTC_RD_TIME` / `RTC_SET_TIME` 这类 ioctl。驱动实现 `rtc_class_ops` 的 `read_time` 和 `set_time`。

本模块没有电池。时间是启动时刻加上 `time_offset`。闹钟用内核定时器，到期打印 `ALARM triggered`。""",
    """```
platform 设备 virt_rtc
  probe → rtc_device_register
  /sys/class/rtc/rtcN/name = virt_rtc
  /dev/rtcN
read_time = 当前时间 + offset
```""",
)

add(
    "ch11", "ch11_pwm", "ch11_pwm", "PWM",
    "`/sys/class/pwm` 里有 `pwmchip`，dmesg 有 `PWM channels`。",
    """PWM 控制器实现 `struct pwm_ops`，用 `pwmchip_add()` 注册。用户空间在 `/sys/class/pwm/pwmchipN/` 里 `export` 通道，再写 `period` 和 `duty_cycle`。

本芯片报两路通道。`apply` 把周期和占空比记在驱动里并打印。没有引脚波形。""",
    """```
pwmchip_add
  npwm = 2
  ops: request / free / apply / get_state
/sys/class/pwm/pwmchipN/
```""",
)

add(
    "ch12", "ch12_dma", "ch12_dma", "kmalloc 和 vmalloc",
    "dmesg 里有 `kmalloc virtual addr`。这一章不调用 DMA API。",
    """`kmalloc` 来自直接映射的物理页，`__pa()` / `__va()` 可以互相换算。`vmalloc` 是虚拟地址连续、物理页可以不连续，对它用 `__pa()` 没有「一整块物理地址」的含义。模块在 `init` 里各分配一次，打印地址，然后立刻释放。

真正的 `dma_alloc_coherent`、`dma_map_single` 和 scatterlist 在 `learn/06_dma`。""",
    """```
init
  kmalloc → __pa → __va → kfree
  vmalloc → vfree
没有 dma_map，没有设备节点
```""",
    "DMA 映射要看 [learn/06_dma](../learn/06_dma)。",
)

add(
    "ch13", "ch13_snull", "ch13_net_virtual", "虚拟网卡 snull",
    "`ip link` 里同时有 `sn0` 和 `sn1`。",
    """网络设备是 `struct net_device`。`alloc_netdev()` 分配，`register_netdev()` 注册。发包走 `ndo_start_xmit`。本驱动做两块网卡 `sn0` 和 `sn1`，把从一块收到的 skb 交换到另一块，用来看协议栈而不需要网线。

`netif_start_queue` / `netif_stop_queue` 控制发送队列。私有数据跟在 `net_device` 后面，用 `netdev_priv()` 取出。""",
    """```
alloc_netdev("sn%d") × 2
register_netdev
  ndo_open / ndo_stop / ndo_start_xmit
sn0 的包交给 sn1，反过来一样
```""",
)

add(
    "ch14", "ch14_eth_mac", "ch14_net_mac_phy", "MAC 和 PHY",
    "`ip link` 里有 `veth0_mac`。",
    """以太网驱动通常分成 MAC 和 PHY。MAC 负责 `net_device` 和收发，PHY 负责链路速率和双工，中间用 MDIO。本模块把两步收成一块虚拟网卡 `veth0_mac`：随机本地 MAC、`IFF_NOARP`、`NETIF_F_LOOPBACK`，并注册 NAPI。

`ethtool_ops` 提供驱动名和链路信息。打开接口时 `netif_napi_add` 已经在注册前做完。""",
    """```
alloc_etherdev
  name = veth0_mac
  netdev_ops + ethtool_ops
  netif_napi_add
register_netdev
```""",
)

add(
    "ch15", "ch15_i2c", "ch15_i2c", "I2C 控制器和从设备",
    "`/dev/i2c_virt` 存在，dmesg 有 `registered as i2c-`，写入之后有 `wrote`。",
    """I2C 分主控制器和从设备。控制器实现 `i2c_algorithm` 的 `master_xfer`，用 `i2c_add_adapter()` 注册，出现 `i2c-N`。从设备是 `i2c_driver`，`probe` 在地址匹配之后调用。

没有设备树时，从设备模块用 `i2c_new_client_device()` 在虚拟总线上放一个地址 `0x50` 的客户端。`/dev/i2c_virt` 的写缓冲第一字节是寄存器地址，后面是数据，对应 `i2c_master_send()`。""",
    """```
i2c_master.ko
  i2c_add_adapter
i2c_slave.ko
  i2c_new_client_device(addr 0x50)
  probe → /dev/i2c_virt
write
  buf[0] = 寄存器，buf[1..] = 数据
  i2c_master_send
```

先加载 master，再加载 slave。卸载顺序相反。""",
)

add(
    "ch16", "ch16_spi", "ch16_spi", "SPI 控制器和从设备",
    "`/dev/spi_virt` 存在，并且读回的前两个字节是 `sp`。",
    """SPI 控制器是 `spi_controller`，从设备是 `spi_device`。一次传输是 `spi_transfer`，装进 `spi_message`，由 `spi_sync()` 提交。SPI 是全双工：发出去的每个字节同时收进一个字节。

虚拟控制器把 MOSI 环回到 MISO。`/dev/spi_virt` 的 `write` 把用户缓冲放进 `tx_buf`，环回结果留在 `rx_buf`，随后 `read` 把 `rx_buf` 交回去。""",
    """```
spi_master.ko
  spi_register_controller
spi_slave.ko
  spi_new_device → probe → /dev/spi_virt
write
  spi_message + spi_sync
read
  返回环回的 rx_buf
```""",
)

add(
    "ch17", "ch17_block", "ch17_block", "内存块设备",
    "`/dev/vmem_disk0` 或 `/sys/block` 里能看到 `vmem_disk`。",
    """块设备按扇区访问。5.15 用 `blk_mq_alloc_disk()` 分配 `gendisk`，`set_capacity()` 设定扇区数，`add_disk()` 公布出去。请求在 `blk_mq_ops.queue_rq` 里完成，驱动把 bio 的页拷进自己的 `vmalloc` 后备存储。

节点名是 `vmem_disk0`。它不是硬盘，卸载前不要把它当根文件系统。""",
    """```
register_blkdev
vmalloc 后备存储
blk_mq_alloc_disk
set_capacity
add_disk → /dev/vmem_disk0
queue_rq：bio ↔ 后备存储
```""",
)

add(
    "ch18", "ch18_mmc", "ch18_mmc", "MMC 主机",
    "`/sys/class/mmc_host` 里出现 `mmc`。",
    """MMC/SD 主机实现 `mmc_host_ops`，`mmc_alloc_host()` 分配，`mmc_add_host()` 让核心开始探测卡。探测是异步的，所以插入模块后要等一下才看得到 `mmc_host`。

本主机的卡是一块内存。容量在加载日志里。卡检测完成后可能出现 `/dev/mmcblk0`，主机目录 `/sys/class/mmc_host/mmcN` 一定先出现。""",
    """```
platform 设备 mmc_virt
mmc_alloc_host
mmc_add_host
  核心异步探测
/sys/class/mmc_host/mmcN
```""",
)

add(
    "ch19", "ch19_pci", "ch19_pci_bus", "PCI 驱动",
    "两个模块的 dmesg 都有 `PCI driver registered`，并且都能 `rmmod`。",
    """PCI 驱动是 `struct pci_driver`。`id_table` 里是厂商号和设备号，`pci_register_driver()` 把它挂到 PCI 总线上。总线上出现匹配设备时才调用 `probe`。`probe` 里典型顺序是 `pci_enable_device`、`pci_request_regions`、`pci_iomap`。

本客户机没有厂商 `0x1234`、设备 `0x5678` 的 PCI 设备，所以 `probe` 不会跑。实验证明的是驱动注册成功，不是 BAR 已经映射。`pci_advanced` 在匹配设备上还会申请 MSI 和 DMA 缓冲，并做一个字符设备。""",
    """```
pci_basic.ko
  pci_register_driver
  id_table: 1234:5678
  无匹配设备 → 只有 registered，没有 Probing
pci_advanced.ko
  同样先注册
  匹配之后才会 iomap / 字符设备
```

`ch07_pci_bus` 是这份代码的副本，不参与 `make all`。""",
)

add(
    "ch20", "ch20_usb", "ch20_usb_bus", "USB 驱动",
    "dmesg 有 `registered`，`/sys/bus/usb/drivers/` 下有 `usb_basic` 和 `usb_advanced`。",
    """USB 设备驱动是 `struct usb_driver`，`usb_register()` 按 `id_table` 匹配接口。`probe` 收到 `usb_interface`，从端点描述符里找 bulk in/out。数据面用 URB：填 `usb_fill_bulk_urb`，再 `usb_submit_urb`。

本客户机没有匹配的 USB 设备，`probe` 不会跑。实验确认驱动挂上了 USB 总线。`usb_advanced` 在真正匹配时还会做字符设备和 URB。""",
    """```
usb_basic.ko
  usb_register
  /sys/bus/usb/drivers/usb_basic
usb_advanced.ko
  /sys/bus/usb/drivers/usb_advanced
无匹配设备 → 没有 Probing 行
```

`ch08_usb_bus` 是这份代码的副本，不参与 `make all`。""",
)


def virt(cmd, label, directory, title, success, knowledge, arch):
    add(cmd, label, directory, title, success, knowledge, arch)


virt("ch21", "ch21_gpio", "ch21_gpio", "GPIO 控制器",
     "`find` 的结果里有 `virt_gpio`，`lsmod` 能看到模块。",
     """GPIO 控制器是 `struct gpio_chip`。要实现 `get`、`set`、`direction_input`、`direction_output`，再 `gpiochip_add_data()`。`base = -1` 表示编号由 gpiolib 分配。

8 根线的方向在 `out[]`，电平在 `val[]`。没有引脚，值只活在这张表里。`/sys/class/gpio/gpiochipN` 是这颗控制器。""",
     """```
platform_device_alloc("virt_gpio")
platform_driver_register
  probe
    gpiochip_add_data
      ngpio = 8，base = -1
remove
  gpiochip_remove
```""")

virt("ch22", "ch22_pinctrl", "ch22_pinctrl", "pinctrl",
     "sysfs 或 debugfs 里能找到 `virt_pinctrl`。",
     """引脚控制器告诉 pinctrl 核心有哪些引脚、哪些组。`pinctrl_pin_desc` 列出 `virt_gpio0`、`virt_gpio1`、`virt_i2c_scl`、`virt_i2c_sda`。一组叫 `virt_all`，包含这四根。`pinctrl_register_and_init()` 加上 `pinctrl_enable()` 完成注册。

这一章只登记引脚，不改复用功能。GPIO 的电平仍看 `ch21_gpio`。""",
     """```
pinctrl_pin_desc[4]
pinctrl_ops：组名 virt_all
devm_pinctrl_register_and_init
pinctrl_enable
```""")

virt("ch23", "ch23_clk", "ch23_clk", "时钟",
     "dmesg 有 `virt_fixed_24m = 24 MHz`。",
     """时钟提供者注册 `struct clk_hw`。固定时钟用 `clk_hw_register_fixed_rate()`，名字 `virt_fixed_24m`，频率 24000000 Hz。消费者以后用 `clk_get` 和 `clk_prepare_enable`。

本模块只做提供者，不自己 `clk_get`。消费者那条路在 `learn/04_soc_sensor`。""",
     """```
probe
  devm_clk_hw_register_fixed_rate("virt_fixed_24m", 24000000)
dmesg: virt_fixed_24m = 24 MHz
```""")

virt("ch24", "ch24_regulator", "ch24_regulator", "regulator",
     "`microvolts` 为 `1800000`，名字是 `virt_vdd_1v8`。",
     """稳压器实现 `regulator_ops`，用 `devm_regulator_register()` 注册。本电源只有一档电压 1.8 V。`enable` / `disable` 改私有的 `enabled` 标志，`get_voltage` 固定返回 1800000 微伏。

约束在 `regulator_init_data` 里，允许改开关，不允许改电压。sysfs 在 `/sys/class/regulator/regulatorN/`。""",
     """```
regulator_desc name = virt_vdd_1v8
  fixed_uV = 1800000
  ops: enable / disable / is_enabled / get_voltage
devm_regulator_register
```""")

virt("ch25", "ch25_reset", "ch25_reset", "reset 控制器",
     "能找到 `virt_reset` 这个 platform 设备。",
     """复位控制器是 `reset_controller_dev`。`ops.reset` 对某一路执行复位，`ops.assert` / `deassert` 分开拉住和放开。消费者用 `devm_reset_control_get()` 拿到 `reset_control`。

本模块登记一路软件复位线，状态记在驱动里。没有外设会被真正清掉。""",
     """```
reset_controller_dev
  nr_resets = 1
  ops: reset / assert / deassert
devm_reset_controller_register
```""")

virt("ch26", "ch26_nvmem", "ch26_nvmem", "NVMEM",
     "能找到名字 `virt-nvmem`。",
     """NVMEM 是给 EEPROM、eFuse 这类存储用的。提供者填 `nvmem_config`：`size`、`reg_read`、`reg_write`，再 `devm_nvmem_register()`。消费者用 `nvmem_cell_read`。

本设备 256 字节，字长 1。读写就是 `memcpy` 到驱动里的数组。""",
     """```
nvmem_config
  name = virt-nvmem
  size = 256
  reg_read / reg_write
devm_nvmem_register
```""")

virt("ch27", "ch27_hwmon", "ch27_hwmon", "hwmon",
     "`temp1_input` 是 `25000`（毫摄氏度，也就是 25.0°C）。",
     """hwmon 用 `hwmon_chip_info` 描述有哪些传感器。`is_visible` 决定 sysfs 文件出不出现，`read` 给出数值。温度通道的 `temp1_input` 单位是毫摄氏度。

`devm_hwmon_device_register_with_info()` 注册。本传感器只有一路温度，固定 25000。""",
     """```
HWMON_CHANNEL_INFO(temp, HWMON_T_INPUT)
read → *val = 25000
devm_hwmon_device_register_with_info("virt_hwmon")
/sys/class/hwmon/hwmonN/temp1_input
```""")

virt("ch28", "ch28_leds", "ch28_leds", "LED",
     "`/sys/class/leds/virt:status/max_brightness` 是 `255`。",
     """LED 类设备是 `led_classdev`。`brightness_set` 在用户写入 `brightness` 时被调用。名字 `virt:status` 会出现在 `/sys/class/leds/virt:status/`。`max_brightness` 是 255。

本回调只把亮度记在 `cdev->brightness`，没有 GPIO 去点灯。""",
     """```
led_classdev
  name = virt:status
  max_brightness = 255
  brightness_set
devm_led_classdev_register
```""")

virt("ch29", "ch29_thermal", "ch29_thermal", "热区",
     "`thermal_zone*/type` 里有 `virt_thermal`。",
     """热区用 `thermal_zone_device_register()` 注册。`ops.get_temp` 报告温度。5.15 上，如果 `ops->critical` 是空的，注册函数会往这个函数指针里写默认处理。ops 表因此不能放在 `const` 的只读段里，并且要自己提供 `critical`。

本热区的温度回调返回一个固定值。不要把 ops 写成 `const`。""",
     """```
thermal_zone_device_ops
  get_temp
  critical   （必须有，ops 不能是 const）
thermal_zone_device_register("virt_thermal")
/sys/class/thermal/thermal_zoneN/type
```""")

virt("ch30", "ch30_power", "ch30_power", "电源供应",
     "`/sys/class/power_supply/virt-battery/capacity` 是 `77`。",
     """电池和适配器走 power supply。`power_supply_desc` 列出属性，`get_property` 按 `POWER_SUPPLY_PROP_*` 填值。`devm_power_supply_register()` 在 `/sys/class/power_supply/` 下建目录。

本电池容量固定 77，电压 3800000 微伏，状态是放电，并且在位。""",
     """```
power_supply_desc name = virt-battery
get_property
  CAPACITY = 77
  VOLTAGE_NOW = 3800000
devm_power_supply_register
```""")

virt("ch31", "ch31_backlight", "ch31_backlight", "背光",
     "能找到 `virt_backlight`。",
     """背光设备是 `backlight_device`。`backlight_ops.update_status` 在亮度改变时被调用，`get_brightness` 返回当前档。`devm_backlight_device_register()` 注册。

sysfs 在 `/sys/class/backlight/`。本设备只记录亮度，不推屏幕。""",
     """```
backlight_ops
  update_status / get_brightness
devm_backlight_device_register
/sys/class/backlight/...
```""")

virt("ch32", "ch32_phy", "ch32_phy", "通用 PHY",
     "能找到 `virt_phy`。",
     """这里的 PHY 是 `drivers/phy` 的通用物理层（USB、PCIe、SATA 那类），不是 `ch14` 的以太网 PHY。提供者实现 `phy_ops` 的 `init` / `power_on` / `power_off`，用 `devm_phy_create()` 和 `devm_of_phy_provider_register()` 交出去。

没有设备树时，创建动作本身就说明控制器登记成功。以太网 MDIO PHY 在 `ch50_mdio`。""",
     """```
devm_phy_create
phy_ops: init / power_on / power_off
phy_provider
```""")

virt("ch33", "ch33_pps", "ch33_pps", "PPS",
     "能找到 PPS 设备。",
     """PPS 把一个外部脉冲当成时间源。驱动 `pps_register_source()` 得到 `pps_device`，每个脉冲调用 `pps_event()`。用户空间打开 `/dev/ppsN`，用 `PPS_FETCH` 取最后一次时间戳。

本模块注册源，不接真实引脚。节点出现就说明源已经挂上。""",
     """```
pps_register_source
/dev/ppsN 或 /sys/class/pps
```""")

virt("ch34", "ch34_ptp", "ch34_ptp", "PTP 时钟",
     "能找到 PTP 时钟设备。",
     """PTP 硬件时钟是 `ptp_clock_info`。`gettime64` / `settime64` / `adjfine` 让网络时间协议调整这块钟。`ptp_clock_register()` 创建 `/dev/ptpN`。

本钟的时间存在驱动变量里，`adjfine` 接受调整但不改真实晶振。""",
     """```
ptp_clock_info
  gettime64 / settime64 / adjfine
ptp_clock_register → /dev/ptpN
```""")

virt("ch35", "ch35_tty", "ch35_tty", "TTY",
     "设备节点 `/dev/ttyVIRT` 存在。",
     """TTY 驱动用 `tty_alloc_driver()` 分配，填 `tty_operations` 的 `open`、`close`、`write`、`write_room`，再 `tty_register_driver()`。`tty_port` 管打开计数和缓冲。

名字是 `ttyVIRT`，类型是串口，9600 8N1。`write` 接受字节并直接返回长度，没有对端。""",
     """```
tty_alloc_driver(1)
  driver_name = virt_tty
  name = ttyVIRT
tty_set_operations
tty_register_device → /dev/ttyVIRT
```""")

virt("ch36", "ch36_fb", "ch36_fb", "framebuffer",
     "`cat` 读到 `virtfb`。fbcon 占着模块时先把 `vtcon1/bind` 和 `vtcon0/bind` 写成 0，再 `rmmod virt_fb` 得到 `rmmod ok`。",
     """fbdev 分配 `fb_info`，填 `fb_var_screeninfo` 和 `fb_fix_screeninfo`，`register_framebuffer()` 得到 `/dev/fbN`。显存是 `vzalloc` 的一块 320×240、16 位的缓冲。

`fb_fillrect` 用 `cfb_fillrect`。它会读 `pseudo_palette`。这个指针必须指向可写的调色板，不能是 NULL。`fb_check_var` 只接受这一种分辨率。""",
     """```
framebuffer_alloc
screen_base = vzalloc(320*240*2)
fix.id = virtfb
pseudo_palette = 16 色
register_framebuffer → /dev/fbN
```""")

virt("ch37", "ch37_mfd", "ch37_mfd", "MFD",
     "能找到 `virt-mfd` 或它的子设备。",
     """MFD 是一个父设备拆成多个平台子设备。父驱动在 `probe` 里填 `mfd_cell` 数组，`devm_mfd_add_devices()` 为每个 cell 生成一个 platform 设备。子驱动按 cell 的名字匹配。

本父设备添加名为 `virt-mfd-cell` 的单元。没有第二块子驱动去 probe 它。实验看的是 cell 已经挂到 platform 总线上。""",
     """```
mfd_cell name = virt-mfd-cell
devm_mfd_add_devices
  → platform 设备 virt-mfd-cell
```""")

virt("ch38", "ch38_extcon", "ch38_extcon", "extcon",
     "能找到 `virt_extcon`。",
     """extcon 报告线缆状态，例如 USB 或耳机是否插入。`devm_extcon_dev_allocate()` 列出支持的线缆类型，`devm_extcon_dev_register()` 注册。状态变化用 `extcon_set_state_sync()`。

本模块注册一种线缆并设为连接。sysfs 在 `/sys/class/extcon/`。""",
     """```
devm_extcon_dev_allocate
devm_extcon_dev_register
extcon_set_state_sync
```""")

virt("ch39", "ch39_scsi", "ch39_scsi", "SCSI 主机",
     "能找到 `virt_scsi`。",
     """SCSI 主机是 `Scsi_Host`。`scsi_host_alloc()` 分配，`scsi_add_host()` 公布，`scsi_scan_host()` 扫描目标。命令在 `queuecommand` 里完成，驱动填 `scsi_cmnd` 的结果并调用 `scsi_done`。

本主机没有磁盘。`queuecommand` 把命令完成掉，用来看主机注册，而不是挂载文件系统。""",
     """```
scsi_host_alloc
scsi_add_host
scsi_scan_host
queuecommand → scsi_done
```""")

virt("ch40", "ch40_crypto", "ch40_crypto", "crypto",
     "`/proc/crypto` 里有 `virt-null`。",
     """对称算法用 `skcipher_alg` 描述，`crypto_register_skcipher()` 注册。算法名 `virt-null`，驱动名 `virt-null-generic`。`encrypt` 和 `decrypt` 都通过 `skcipher_walk` 把源拷到目的，所以它是恒等变换。

`/proc/crypto` 能看到名字和优先级。用户空间一般经 AF_ALG 或内核里的 `crypto_alloc_skcipher()` 使用它。""",
     """```
skcipher_alg
  cra_name = virt-null
  setkey / encrypt / decrypt
crypto_register_skcipher
/proc/crypto
```""")

virt("ch41", "ch41_virtio", "ch41_virtio", "virtio 驱动",
     "能找到 `virt_virtio` 驱动。",
     """virtio 驱动是 `struct virtio_driver`。`id_table` 给出设备号，`register_virtio_driver()` 挂到 virtio 总线。匹配的 virtio 设备出现时才 `probe`。传输（PCI、MMIO）由 virtio 核心处理，驱动只谈 virtqueue。

本表匹配设备号 `0xffff`。QEMU 命令行没有加这种 virtio 设备时，`probe` 不运行。实验确认驱动已注册。""",
     """```
virtio_device_id { 0xffff, VIRTIO_DEV_ANY_ID }
register_virtio_driver
  有匹配设备才 probe
/sys/bus/virtio/drivers/virt_virtio
```""")

virt("ch42", "ch42_remoteproc", "ch42_remoteproc", "remoteproc",
     "能找到 `virt-rproc`。",
     """remoteproc 管一块远程处理器的生命周期。`rproc_alloc()` 配上 `rproc_ops` 的 `start` 和 `stop`，`rproc_add()` 注册。sysfs 在 `/sys/class/remoteproc/remoteprocN/`，`state` 可以读，写入 `start` 会调到 `ops.start`。

本处理器没有固件。`start` 只改状态。""",
     """```
rproc_alloc(name virt-rproc)
  ops.start / ops.stop
rproc_add
/sys/class/remoteproc/
```""")

virt("ch43", "ch43_devfreq", "ch43_devfreq", "devfreq",
     "能找到 `virt_devfreq`。",
     """devfreq 按负载调设备频率。`devfreq_dev_profile` 给出频率表和 `target` 回调，`devfreq_add_device()` 注册。改频前通常要有 OPP 表，`dev_pm_opp_add()` 把频率放进去。

本设备有一张很小的频率表。`target` 记下被选中的频率。OPP 的单独一章是 `ch49_opp`。""",
     """```
dev_pm_opp_add
devfreq_dev_profile.target
devfreq_add_device
/sys/class/devfreq/
```""")

virt("ch44", "ch44_dmaengine", "ch44_dmaengine", "dmaengine",
     "能找到 `virt_dmaeng`。",
     """dmaengine 是通用 DMA 通道。控制器实现 `dma_device`，通道是 `virt_dma_chan`。`dma_async_device_register()` 之后，别的驱动 `dma_request_chan()`。提交描述符、`issue_pending` 开始搬移。

本引擎的 `issue_pending` 不碰硬件。它和 `ch12` 的 kmalloc、`learn/06` 的 DMA API 不是同一层。""",
     """```
dma_device + virt_dma_chan
dma_async_device_register
issue_pending
```""")

virt("ch45", "ch45_rfkill", "ch45_rfkill", "rfkill",
     "能找到 `virt_rfkill`。",
     """rfkill 是无线开关。`rfkill_alloc()` 指定类型（例如 WLAN），`rfkill_register()` 出现在 `/sys/class/rfkill/`。用户或平台策略调用 `ops.set_block`。

本开关只记一个布尔值。""",
     """```
rfkill_alloc
ops.set_block
rfkill_register
/sys/class/rfkill/
```""")

virt("ch46", "ch46_hwspinlock", "ch46_hwspinlock", "硬件自旋锁",
     "能找到 `virt_hwspin`。",
     """硬件自旋锁给多块处理器抢同一把锁。银行是 `hwspinlock_device`，`ops.trylock` 必须立刻返回，成功返回非 0。`hwspin_lock_register()` 登记这一排锁。

本锁用一个软件标志模拟。`trylock` 在已被占用时返回失败，`unlock` 放开。""",
     """```
hwspinlock_ops
  trylock / unlock / relax
hwspin_lock_register
```""")

virt("ch47", "ch47_mailbox", "ch47_mailbox", "mailbox",
     "能找到 `virt_mbox`。",
     """mailbox 是一块处理器向另一块发短消息的控制器。`mbox_controller` 加 `mbox_chan_ops.send_data`，`devm_mbox_controller_register()` 注册。对端用 `mbox_send_message()`。

本通道的 `send_data` 接受指针并返回 0，不把消息送到第二块 CPU。""",
     """```
mbox_controller
  ops.send_data
devm_mbox_controller_register
```""")

virt("ch48", "ch48_powercap", "ch48_powercap", "powercap",
     "能找到 `virt_pcap`。",
     """powercap 给功耗封顶做一类 sysfs。`powercap_register_control_type()` 注册控制类型，再 `powercap_register_zone()` 加区域。区域可以有能耗计数和功率上限。

本区域名字带 `virt_pcap`。读到的是驱动里的计数，不是实测功率。""",
     """```
powercap_register_control_type
powercap_register_zone("virt_pcap")
/sys/class/powercap/
```""")

virt("ch49", "ch49_opp", "ch49_opp", "OPP",
     "能找到 `virt_opp`。",
     """OPP 是一组「频率、电压」工作点。`dev_pm_opp_add()` 往设备的表里加一项。devfreq 和 cpufreq 从这张表里挑点。

本模块给自己的 platform 设备加一个工作点，证明表能建起来。和 `ch43_devfreq` 配在一起看。""",
     """```
platform 设备 virt_opp
dev_pm_opp_add(频率, 电压)
```""")

virt("ch50", "ch50_mdio", "ch50_mdio", "MDIO",
     "能找到 `virt-mdio`。",
     """MDIO 是 MAC 访问以太网 PHY 寄存器的总线。`mii_bus` 填 `read` 和 `write`，`mdiobus_register()` 注册。PHY 驱动随后可以挂到某个地址。

本总线的读写打到一块假寄存器。它和 `ch32_phy` 的通用 PHY 框架不是同一个子系统。""",
     """```
mii_bus name = virt-mdio
  read / write
mdiobus_register
```""")

virt("ch51", "ch51_iio", "ch51_iio", "IIO",
     "`in_voltage0_raw` 是 `1234`。",
     """IIO 用 `iio_dev` 描述一块传感器。通道是 `iio_chan_spec`，这里是一路索引为 0 的电压，带 `RAW` 和 `SCALE`。`read_raw` 在 `IIO_CHAN_INFO_RAW` 时返回 1234。

`devm_iio_device_register()` 之后，sysfs 在 `/sys/bus/iio/devices/iio:deviceN/`。直接模式 `INDIO_DIRECT_MODE` 表示读一次通道就返回，不用缓冲。""",
     """```
iio_chan_spec
  type = IIO_VOLTAGE，channel = 0
read_raw
  RAW → 1234
  SCALE → 0 + 1000 微
devm_iio_device_alloc
devm_iio_device_register
```""")

virt("ch52", "ch52_alsa", "ch52_alsa", "ALSA",
     "`/proc/asound/cards` 里有 `virt`。",
     """ALSA 声卡是 `snd_card`。`snd_card_new()` 创建，`snd_pcm_new()` 加一块 PCM，`snd_pcm_set_ops()` 填 open、hw_params、prepare、trigger。`snd_card_register()` 之后 `/proc/asound/cards` 能看到它。

硬件参数固定 48000 Hz、双声道、S16_LE。`trigger` 不推真实 DAC。""",
     """```
snd_card_new(..., "virt")
snd_pcm_new("virt PCM")
snd_pcm_set_ops(playback 和 capture)
snd_card_register
/proc/asound/cards
```""")

virt("ch53", "ch53_hid", "ch53_hid", "HID",
     "能找到 `virt_hid`。",
     """HID 驱动解析报告描述符，把输入报告变成 input 事件。低层传输（USB、I2C）向 HID 核心提供 `hid_ll_driver`。`hid_allocate_device()`、`hid_add_device()` 把设备放进 HID 总线。

本设备带一份很小的报告描述符，`raw_request` 返回空报告。它说明 HID 核心接受了这块设备。""",
     """```
hid_allocate_device
hid_ll_driver: parse / start / stop / raw_request
hid_add_device
```""")

virt("ch54", "ch54_mtd", "ch54_mtd", "MTD",
     "`/sys/class/mtd/mtd*/name` 是 `virt-mtd`。",
     """MTD 是原始闪存或 RAM 的线性映射，不带文件系统。`mtd_info` 填 `size`、`erasesize`、`_read`、`_write`、`_erase`，`mtd_device_register()` 得到 `/dev/mtdN`。

本设备 64 KiB，擦除块 4 KiB，初值全是 `0xff`。擦除把区间写成 `0xff`，读写是 `memcpy`。""",
     """```
mtd_info
  name = virt-mtd
  size = 64 KiB，erasesize = 4096
  _read / _write / _erase
mtd_device_register
```""")

virt("ch55", "ch55_uio", "ch55_uio", "UIO",
     "能找到 `virt_uio`。",
     """UIO 让用户空间映射设备内存并等中断。`uio_info` 填名字和 IRQ，`uio_register_device()` 创建 `/dev/uioN`。没有硬件中断时 `irq` 用 `UIO_IRQ_NONE`。

本设备没有映射真实 BAR。节点出现说明 UIO 核心接受了 `uio_info`。""",
     """```
uio_info
  name = virt_uio
  irq = UIO_IRQ_NONE
uio_register_device → /dev/uioN
```""")

virt("ch56", "ch56_w1", "ch56_w1", "1-Wire",
     "能找到 `virt_w1`。",
     """1-Wire 主机实现 `w1_bus_master` 的触碰位时序，`w1_add_master_device()` 注册。核心会搜索总线上的从设备家族号。

本主机的位操作是空的，总线上不会出现真实从设备。实验看主机设备已经加上。""",
     """```
w1_bus_master
w1_add_master_device
/sys/bus/w1/
```""")

virt("ch57", "ch57_can", "ch57_can", "CAN",
     "`ip link` 里有 `can`。",
     """CAN 控制器在 `alloc_candev()` 的 `net_device` 上填 `can_priv`：位定时常数、时钟、`do_set_bittiming`。`register_candev()` 注册。接口要先设比特率再 `ip link set up`。

本控制器时钟 8 MHz。`start_xmit` 丢掉 skb 并计一次发送。没有收发器。""",
     """```
alloc_candev
can_priv.clock.freq = 8000000
can_bittiming_const
register_candev
ip link 看到 can0
```""")

virt("ch58", "ch58_mux", "ch58_mux", "多路开关",
     "能找到 `virt_mux`。",
     """多路开关把几路信号选到一路上。`mux_chip_alloc()` 分配，`ops.set` 在选通改变时被调用，`devm_mux_chip_register()` 注册。

本芯片记当前通道号。没有真实模拟开关。""",
     """```
mux_chip
ops.set
devm_mux_chip_register
```""")

virt("ch59", "ch59_fpga", "ch59_fpga", "FPGA manager",
     "能找到 `virt-fpga`。",
     """FPGA manager 负责加载比特流。`fpga_manager_ops` 有 `write_init`、`write`、`write_complete`。`devm_fpga_mgr_register()` 把它放进 `/sys/class/fpga_manager/`。

本管理器接受写入并报告完成，不配置真实 FPGA。""",
     """```
fpga_manager_ops
  write_init / write / write_complete
devm_fpga_mgr_register("virt-fpga")
```""")

virt("ch60", "ch60_spmi", "ch60_spmi", "SPMI",
     "能找到 `virt_spmi`。",
     """SPMI 是电源管理芯片用的串行总线。控制器实现 `spmi_controller` 的 `cmd`、`read_cmd`、`write_cmd`，`spmi_controller_add()` 注册。从设备按地址挂在这条总线上。

本控制器的命令回调返回成功，没有 PMIC。""",
     """```
spmi_controller_alloc
ops: cmd / read_cmd / write_cmd
spmi_controller_add
```""")

virt("ch61", "ch61_i3c", "ch61_i3c", "I3C",
     "能找到 `virt_i3c`。",
     """I3C 主控制器实现 `i3c_master_controller_ops`：`bus_init`、`do_daa`（动态地址分配）、`send_ccc`、兼容的 I2C 传输。`i3c_master_register()` 注册。`i3c_master_set_info()` 必须在注册前填上主机自己的动态地址能力。

本主机不扫描真实设备。注册成功就是实验目标。""",
     """```
i3c_master_controller
  ops.bus_init / do_daa / send_ccc
i3c_master_set_info
i3c_master_register
```""")

virt("ch62", "ch62_gnss", "ch62_gnss", "GNSS",
     "能找到 `virt_gnss`。",
     """GNSS 框架把接收机暴露成字符设备，NMEA 句子从这里读。`gnss_allocate_device()` 分配，`gnss_register_device()` 注册。驱动在收到芯片数据时 `gnss_insert_raw()`。

本接收机的 `write` 接受命令，不输出真实定位。""",
     """```
gnss_allocate_device
gnss_ops: open / close / write_raw
gnss_register_device
```""")

virt("ch63", "ch63_nfc", "ch63_nfc", "NFC",
     "能找到 `virt_nfc`。",
     """NFC 控制器实现 `nfc_ops`：`dev_up`、`dev_down`、`start_poll`、`stop_poll`、`activate_target`、`deactivate_target`、`im_transceive`。`nfc_allocate_device()` 和 `nfc_register_device()` 注册。

这些回调都要在 ops 里，缺一个就会在注册时失败。本控制器不发起射频。""",
     """```
nfc_ops（up/down/poll/activate/transceive）
nfc_allocate_device
nfc_register_device
```""")

virt("ch64", "ch64_bluetooth", "ch64_bluetooth", "蓝牙 HCI",
     "能找到 `virt_bt`。",
     """蓝牙控制器从 HCI 驱动挂上。`hci_alloc_dev()` 分配，`hci_register_dev()` 注册。`send` 把 HCI 命令或 ACL 数据交给传输。

本传输把包接受下来并计一次发送，不建立空口链路。""",
     """```
hci_alloc_dev
hci_register_dev
send → 接受 skb
```""")

virt("ch65", "ch65_v4l2", "ch65_v4l2", "V4L2",
     "能找到 `virt_v4l2`。",
     """V4L2 设备是 `video_device`。`v4l2_device_register()` 登记驱动，`video_register_device()` 得到 `/dev/videoN`。`v4l2_file_operations` 至少要有 `unlocked_ioctl` 走到 `video_ioctl2`，`vidioc_querycap` 报告驱动名。

本设备报告能力，不采集帧。节点在 `/sys/class/video4linux/`。""",
     """```
v4l2_device_register
video_device
  fops + ioctl_ops.querycap
video_register_device → /dev/videoN
```""")

virt("ch66", "ch66_counter", "ch66_counter", "counter",
     "能找到 `virt_count`。",
     """counter 子系统描述计数器和信号。`counter_device` 里有 `counter_count`，`count_read` 返回计数值。`devm_counter_add()` 注册。

本计数器一条信号 `virt_sig`，读数是驱动里的整数。sysfs 在 counter 总线下。""",
     """```
counter_count name = virt_count
signal name = virt_sig
count_read
devm_counter_add
```""")

virt("ch67", "ch67_drm", "ch67_drm", "DRM",
     "能找到 `virt_drm`。",
     """DRM 设备从 `drm_dev_alloc()` 开始，准备 mode config 和最小的 KMS 对象后 `drm_dev_register()`。用户空间打开 `/dev/dri/cardN`。

本设备做最小注册，不设扫描输出时序。它和 `ch36_fb` 的 fbdev 是两套显示框架。""",
     """```
drm_dev_alloc
drm_mode_config_init
drm_dev_register → /dev/dri/cardN
```""")

virt("ch68", "ch68_interconnect", "ch68_interconnect", "interconnect",
     "能找到 `virt_icc`。",
     """interconnect 描述片上总线的带宽请求。提供者实现 `icc_provider`，`set` 在聚合带宽变化时被调用。节点用 `icc_node_create()` 建，`icc_link_create()` 连边，`icc_provider_register()` 注册。

本提供者两节点一条链路。`set` 记下带宽，不改时钟。""",
     """```
icc_provider
  set / aggregate / xlate
icc_node_create × 2
icc_link_create
icc_provider_register
```""")


def main():
    log = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else LOG
    tx = transcripts(log.read_text(encoding="utf-8", errors="replace"))
    missing = [item[1] for item in VIRT if item[1] not in tx]
    if missing:
        raise SystemExit("missing transcripts: " + ", ".join(missing))

    for cmd, label, directory, title, success, knowledge, arch, *rest in VIRT:
        note = rest[0] if rest else ""
        body = page(title, knowledge, arch, cmd, label, success, tx[label], note)
        replace_whole(ROOT / directory / "README.md", body)

    # 早期三章保留已有知识点，换掉不能在镜像里复现的编译和实验段。
    for directory, cmd, label, success in (
        ("ch01_kernel_basics", "ch01", "ch01_kfifo",
         "dmesg 里有 `fifo test begin`、`fifo test end`、`buf:hello`，并且 `cat /proc/bytestream-fifo` 读回 `labfifo`。"),
        ("ch02_char_basic", "ch02", "ch02_globalmem",
         "`head -c 9 /dev/global_mem_0` 是 `test_data`。设备节点是 `global_mem_0` 到 `global_mem_3`，不是 `/dev/globalmem`。"),
        ("ch03_char_advanced", "ch03", "ch03_globalfifo",
         "`head -c 9 /dev/global_mem_0` 是 `fifo_test`。读完就停，不要用 `cat`，空 FIFO 上的 `read` 会睡着。"),
    ):
        block = lab_section(cmd, label, success, tx[label])
        replace_span(ROOT / directory / "README.md", "## 编译方法", "## 常见问题", block)

    ch00 = """## 在镜像里复现

这一章没有 `.ko`。`driver_template.c` 是一份字符设备骨架，不参与 `make all`。镜像里用下面的命令核对骨架还在，并且五步注册都写在文件里。

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch00
```

实验成功：退出码是 0，回显里有 `[PASS] ch00_framework`，并且能看到 `alloc_chrdev_region`、`cdev_init`、`cdev_add`、`class_create`、`device_create`。

可加载的第一块模块从 [ch01_kernel_basics](../ch01_kernel_basics) 开始。

"""
    splice_before(ROOT / "ch00_framework/README.md", "## 目录", ch00)

    duplicates = (
        ("ch04_regmap_framework", "ch08", "ch08_regmap", "../ch08_regmap",
         "regmap 把 `regmap_read` / `regmap_write` 从 I2C、SPI、MMIO 后面抽出来。自定义总线时用 `regmap_init()`，配 `reg_read` 和 `reg_write`。"),
        ("ch05_i2c_bus", "ch15", "ch15_i2c", "../ch15_i2c",
         "I2C 控制器实现 `i2c_algorithm.master_xfer` 并 `i2c_add_adapter()`。从设备是 `i2c_driver`，没有设备树时用 `i2c_new_client_device()` 放到总线上。"),
        ("ch07_pci_bus", "ch19", "ch19_pci", "../ch19_pci_bus",
         "PCI 驱动是 `pci_driver` 加 `id_table`。`pci_register_driver()` 之后，只有总线上出现匹配的厂商号和设备号才会 `probe`。"),
        ("ch08_usb_bus", "ch20", "ch20_usb", "../ch20_usb_bus",
         "USB 接口驱动是 `usb_driver`。`usb_register()` 按 `id_table` 匹配，`probe` 里从端点描述符找 bulk，再用 URB 收发。"),
    )
    for directory, cmd, label, link, knowledge in duplicates:
        body = f"""# {directory}

本目录不参与 `make all`，镜像也不会加载这里的文件。能跑的代码在 [{link}]({link})。

## 代码知识点

{knowledge}

## 代码架构

注册和卸载的顺序与 [{link}]({link}) 里的模块相同。本目录如果留有同名 `.c`，只作对照，不要在这里 `make` 之后拿去 `insmod`。

{lab_section(cmd, label, "回显与可运行那一章相同，并且退出码是 0。", tx[label])}
"""
        replace_whole(ROOT / directory / "README.md", body)

    learn = (
        ("learn/01_uapi/README.md", "learn01", "learn01_uapi",
         "`uapi_test` 打印 `uapi ok: all`。"),
        ("learn/02_devicetree/README.md", "learn02", "learn02_devicetree",
         "三行依次是 `40`、`4c444454`、`19`。"),
        ("learn/03_irq/README.md", "learn03", "learn03_irq",
         "`counts` 前五项都大于等于 1，第六项 `raise_errno` 是 0。"),
        ("learn/04_soc_sensor/README.md", "learn04", "learn04_sensor",
         "温度是 `37`，`clock_hz` 是 `24000000`。"),
        ("learn/05_pm/README.md", "learn05", "learn05_pm",
         "suspend 之后 ctrl 是 `0`，resume 之后是 `3`。写 `idle` 时 shell 可能打印 `Resource temporarily unavailable`，随后 `runtime_status` 仍是 `suspended`，`control=on` 之后是 `active`。"),
        ("learn/06_dma/README.md", "learn06", "learn06_dma",
         "`report` 同时包含 `coherent:ok`、`stream:ok`、`sg:ok`。"),
        ("learn/07_debug/README.md", "learn07", "learn07_debug",
         "打开 dyndbg 之前只有 `ping info`，之后有 `ping debug`，ftrace 里有 `virt_debug_ping`。"),
    )
    for path, cmd, label, success in learn:
        block = "\n" + lab_section(cmd, label, success, tx[label])
        text = (ROOT / path).read_text(encoding="utf-8")
        text = re.sub(r"\n## 在镜像里复现\n.*", "\n", text, flags=re.S)
        (ROOT / path).write_text(text.rstrip() + "\n" + block, encoding="utf-8")

    hard = ROOT / "hardcode/README.md"
    text = hard.read_text(encoding="utf-8")
    text = re.sub(r"\n## 在镜像里复现\n.*", "\n", text, flags=re.S)
    block = lab_section(
        "hardcode", "hardcode",
        "dmesg 里有 `hardcode samples ok` 和 `hardcode list key=`。",
        tx["hardcode"],
    )
    hard.write_text(text.rstrip() + "\n\n" + block, encoding="utf-8")
    print(f"updated {len(VIRT)} chapter pages, learn/, hardcode, ch00, duplicates")


if __name__ == "__main__":
    main()
