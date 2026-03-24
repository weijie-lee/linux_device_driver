# spi_driver — 虚拟 SPI master/slave 驱动示例

本模块演示完整的 Linux SPI 驱动框架，包含 **SPI master 控制器驱动**和 **SPI slave 外设驱动**两个层次。全程基于软件模拟（loopback），无需任何 SPI 硬件即可在标准 Linux 主机上运行验证。

## 知识点

- ✅ `spi_alloc_master()` / `spi_register_master()` — 注册 SPI 控制器
- ✅ `transfer_one_message()` — 实现控制器的核心传输逻辑
- ✅ `spi_message` / `spi_transfer` — SPI 传输描述符的构造与迭代
- ✅ `spi_driver` probe/remove — SPI 外设驱动的生命周期
- ✅ `spi_sync()` — 同步（阻塞）方式发起 SPI 传输
- ✅ `spi_new_device()` — 无设备树时手动实例化 SPI 设备
- ✅ 通过字符设备将 SPI 总线暴露给用户空间

## 架构图

```
用户空间
  write("/dev/spi_virt", data)
        │
        ▼
  spi_slave.c (spi_driver)
    spi_virt_write()
    ├── 构造 spi_message + spi_transfer
    └── spi_sync() ──────────────────────────────┐
                                                  ▼
                                    spi_master.c (spi_master)
                                    spi_virt_transfer_one_message()
                                    ├── loopback: rx_buf = tx_buf
                                    └── spi_finalize_current_message()
        ▲
        │
  read("/dev/spi_virt") ← rx_buf 中读回回显数据
```

## 代码结构

### spi_master.c — 控制器驱动

| 函数 | 作用 |
|------|------|
| `spi_virt_transfer_one_message()` | 核心传输函数，遍历 spi_transfer 链表，将 tx_buf 回显到 rx_buf |
| `spi_virt_master_probe()` | 分配并注册 spi_master，设置总线号、CS 数量、速率上限 |
| `spi_virt_master_init()` | 自建 platform_device 作为控制器父设备，无需设备树 |

**关键数据结构：**

```c
master->bus_num        = 0;          // 注册为 spi0
master->num_chipselect = 4;          // 支持 4 个片选
master->mode_bits      = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH;
master->transfer_one_message = spi_virt_transfer_one_message;
```

### spi_slave.c — 外设驱动

| 函数 | 作用 |
|------|------|
| `spi_virt_slave_probe()` | 分配私有数据，注册字符设备 `/dev/spi_virt` |
| `spi_virt_write()` | 接收用户数据，构造 spi_transfer，调用 `spi_sync()` 发起传输 |
| `spi_virt_read()` | 将 rx_buf 中的回显数据返回给用户空间 |
| `spi_virt_slave_init()` | 注册 spi_driver，并通过 `spi_new_device()` 手动实例化设备 |

**spi_transfer 构造方式：**

```c
struct spi_transfer xfer = {
    .tx_buf   = slave->tx_buf,   // 发送缓冲区
    .rx_buf   = slave->rx_buf,   // 接收缓冲区（全双工）
    .len      = count,
    .speed_hz = 1000000,         // 1 MHz
};
spi_message_init(&msg);
spi_message_add_tail(&xfer, &msg);
ret = spi_sync(slave->spi, &msg);
```

## 编译

```bash
cd spi_driver
make
```

## 加载顺序（必须先加载 master）

```bash
# 1. 加载 SPI master 控制器
sudo insmod spi_master.ko

# 2. 验证 master 注册成功
dmesg | grep spi_virt_master
# 预期输出: virtual SPI master registered as spi0 (loopback mode)

# 3. 查看 sysfs 中的 SPI 总线
ls /sys/bus/spi/devices/

# 4. 加载 SPI slave 外设驱动
sudo insmod spi_slave.ko

# 5. 验证 slave probe 成功
dmesg | grep spi_virt_slave
# 预期输出: SPI slave probed on bus 0 cs 0, /dev/spi_virt created
```

## 验证实验

### 实验一：loopback 回显测试

```bash
# 写入数据，master 会将其回显到 rx_buf
echo -n "Hello SPI" > /dev/spi_virt

# 读回 rx_buf 中的数据（应与写入内容相同）
cat /dev/spi_virt
# 预期输出: Hello SPI
```

### 实验二：二进制数据验证

```bash
# 写入已知字节序列
printf '\x01\x02\x03\x04\x05' > /dev/spi_virt

# 用 xxd 查看回显
cat /dev/spi_virt | xxd | head
# 预期: 00000000: 0102 0304 05aa aaaa ...  (前5字节为写入值，其余为0xAA填充)
```

### 实验三：查看 sysfs 传输统计

```bash
# 查看 SPI 设备属性
cat /sys/bus/spi/devices/spi0.0/modalias
# 预期: spi:spi-virt-dev

# 查看内核日志中的传输记录
dmesg | grep "spi_virt_slave: wrote"
```

### 实验四：卸载顺序验证

```bash
# 必须先卸载 slave，再卸载 master
sudo rmmod spi_slave
sudo rmmod spi_master

# 验证资源已释放
dmesg | tail -4
ls /dev/spi_virt 2>&1   # 应报 No such file or directory
```

## SPI 工作模式说明

| 模式 | CPOL | CPHA | 时钟空闲 | 采样边沿 |
|------|------|------|---------|---------|
| MODE_0 | 0 | 0 | 低电平 | 上升沿 |
| MODE_1 | 0 | 1 | 低电平 | 下降沿 |
| MODE_2 | 1 | 0 | 高电平 | 下降沿 |
| MODE_3 | 1 | 1 | 高电平 | 上升沿 |

本驱动默认使用 **MODE_0**，可在 `spi_board_info.mode` 中修改。

## 核心要点

| 要点 | 说明 |
|------|------|
| **master 先于 slave 加载** | slave 的 `init` 调用 `spi_busnum_to_master(0)` 查找 master，若 master 未注册则返回 `-ENODEV` |
| **全双工传输** | `spi_transfer` 同时指定 `tx_buf` 和 `rx_buf`，一次传输完成发送和接收 |
| **spi_sync vs spi_async** | `spi_sync` 阻塞等待传输完成；`spi_async` 注册回调后立即返回，适合高性能场景 |
| **spi_new_device** | 无设备树时的设备实例化方式，等价于 DT 中的 `compatible` 匹配 |
| **loopback 验证** | master 的 `transfer_one_message` 将 tx_buf 直接复制到 rx_buf，无需真实 SPI 线路 |

## 参考

- 《Linux设备驱动开发详解》第15章（SPI子系统）
- `Documentation/spi/spi-summary.rst`
- `drivers/spi/spi-loopback-test.c`（内核自带 SPI loopback 测试框架）
