# i2c_driver — 虚拟 I2C master/slave 驱动示例

本模块演示完整的 Linux I2C 驱动框架，包含 **I2C adapter（master）驱动**和 **I2C client（slave）驱动**两个层次。全程基于软件模拟，内置一个 256 字节的虚拟寄存器文件，无需任何 I2C 硬件即可在标准 Linux 主机上运行验证。

## 知识点

- ✅ `i2c_add_adapter()` / `i2c_del_adapter()` — 注册 I2C 总线适配器
- ✅ `struct i2c_algorithm.master_xfer()` — 实现 I2C 总线的核心传输逻辑
- ✅ `I2C_M_RD` 标志 — 区分读写事务
- ✅ `i2c_driver` probe/remove — I2C 外设驱动的生命周期
- ✅ `i2c_master_send()` / `i2c_master_recv()` — 高层传输辅助函数
- ✅ `i2c_smbus_read/write_byte_data()` — SMBus 寄存器访问
- ✅ `i2c_new_device()` — 无设备树时手动实例化 I2C 设备
- ✅ `i2c_check_functionality()` — 验证 adapter 能力

## 架构图

```
用户空间
  write("/dev/i2c_virt", "\x00\xAB")   ← reg=0x00, data=0xAB
        │
        ▼
  i2c_slave.c (i2c_driver / i2c_client)
    i2c_virt_write()
    └── i2c_master_send() ──────────────────────────┐
                                                     ▼
                                    i2c_master.c (i2c_adapter)
                                    i2c_virt_xfer()
                                    ├── WRITE: reg_file[0x00] = 0xAB
                                    └── READ:  return reg_file[reg_ptr]
        ▲
        │
  read("/dev/i2c_virt") ← i2c_master_recv() 读回寄存器值
```

## 代码结构

### i2c_master.c — adapter 驱动

| 函数 | 作用 |
|------|------|
| `i2c_virt_xfer()` | 核心传输函数：WRITE 写入寄存器文件，READ 读出寄存器文件 |
| `i2c_virt_func()` | 声明支持的功能集（`I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL`） |
| `i2c_virt_master_init()` | 分配并注册 adapter，预填充寄存器文件 |

**寄存器文件初始值：**

```c
reg_file[0x00] = 0x50;  // 模拟 "设备 ID"
reg_file[0x01] = 0x01;  // 模拟 "版本号"
reg_file[其余]  = 0xAB;  // 可识别的填充值
```

### i2c_slave.c — client 驱动

| 函数 | 作用 |
|------|------|
| `i2c_virt_slave_probe()` | 检查 adapter 功能，注册字符设备 `/dev/i2c_virt` |
| `i2c_virt_write()` | 接收用户数据，调用 `i2c_master_send()` 写入寄存器 |
| `i2c_virt_read()` | 先发送寄存器地址，再调用 `i2c_master_recv()` 读取数据 |
| `i2c_virt_ioctl()` | `I2C_VIRT_SET_REG` 命令设置下次读取的寄存器地址 |

**写操作协议（标准 I2C EEPROM 格式）：**

```
用户写入: [reg_addr, data0, data1, ...]
  buf[0] = 寄存器地址
  buf[1..n] = 数据字节
```

## 编译

```bash
cd i2c_driver
make
```

## 加载顺序（必须先加载 master）

```bash
# 1. 加载 I2C adapter
sudo insmod i2c_master.ko

# 2. 查看分配的总线号
dmesg | grep i2c_virt_master
# 预期: i2c_virt_master: registered as i2c-5, virtual slave at 0x50

# 3. 记录总线号（假设为 5）
BUS=5

# 4. 加载 I2C slave 驱动
sudo insmod i2c_slave.ko

# 5. 验证 probe 成功
dmesg | grep i2c_virt_slave
# 预期: I2C slave probed at addr 0x50 on adapter i2c-virt-master, /dev/i2c_virt created
```

## 验证实验

### 实验一：i2cdetect 扫描总线

```bash
# 安装 i2c-tools（如未安装）
sudo apt-get install -y i2c-tools

# 扫描虚拟总线（替换 5 为实际总线号）
sudo i2cdetect -y 5
# 预期输出：地址 0x50 处显示 "50"，其余为 "--"
#      0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
# 50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
# 50: 50 -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
```

### 实验二：i2cget/i2cset 直接访问寄存器

```bash
# 读取寄存器 0x00（设备 ID，预期值 0x50）
sudo i2cget -y 5 0x50 0x00
# 预期: 0x50

# 读取寄存器 0x01（版本号，预期值 0x01）
sudo i2cget -y 5 0x50 0x01
# 预期: 0x01

# 写入寄存器 0x10，值为 0xDE
sudo i2cset -y 5 0x50 0x10 0xDE

# 读回验证
sudo i2cget -y 5 0x50 0x10
# 预期: 0xde
```

### 实验三：通过 /dev/i2c_virt 字符设备访问

```bash
# 写入：reg=0x20，data=0x42
printf '\x20\x42' > /dev/i2c_virt

# 设置读取寄存器地址（ioctl 方式）
# 或者直接再写一个单字节（仅设置寄存器指针）
printf '\x20' > /dev/i2c_virt

# 读取 1 字节
dd if=/dev/i2c_virt bs=1 count=1 2>/dev/null | xxd
# 预期: 00000000: 42  .
```

### 实验四：SMBus 连续读（i2cdump）

```bash
# 转储整个寄存器文件（256 字节）
sudo i2cdump -y 5 0x50
# 预期：0x00=50, 0x01=01, 其余=AB（初始填充值）
```

### 实验五：卸载顺序验证

```bash
sudo rmmod i2c_slave
sudo rmmod i2c_master
dmesg | tail -4
ls /dev/i2c_virt 2>&1   # 应报 No such file or directory
```

## I2C 事务类型说明

| 类型 | 标志 | 描述 |
|------|------|------|
| WRITE | `flags=0` | master → slave，第一字节通常是寄存器地址 |
| READ | `I2C_M_RD` | slave → master，读取寄存器数据 |
| 写后读 | WRITE + repeated START + READ | 最常见的寄存器读取模式 |
| SMBus byte | `i2c_smbus_read/write_byte_data` | 单字节寄存器访问的高层封装 |

## 核心要点

| 要点 | 说明 |
|------|------|
| **adapter 先于 client 加载** | slave 的 `init` 通过名称查找 adapter，若未注册则返回 `-ENODEV` |
| **写后读模式** | 先发送寄存器地址（WRITE），再发起 READ，这是 I2C 传感器/EEPROM 的标准访问模式 |
| **i2c_check_functionality** | probe 时必须验证 adapter 支持所需功能，否则应返回 `-ENODEV` |
| **SMBus 兼容** | `I2C_FUNC_SMBUS_EMUL` 使 adapter 支持所有 SMBus 命令，`i2cget`/`i2cset` 工具依赖此功能 |
| **动态总线号** | `adapter.nr = -1` 让内核自动分配总线号，避免与系统现有 I2C 总线冲突 |

## 参考

- 《Linux设备驱动开发详解》第14章（I2C子系统）
- `Documentation/i2c/writing-clients.rst`
- `Documentation/i2c/instantiating-devices.rst`
- `drivers/i2c/i2c-stub.c`（内核自带 I2C stub 驱动，原理与本驱动相同）
