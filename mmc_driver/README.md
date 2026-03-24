# mmc_driver — 虚拟 MMC/eMMC 主控驱动示例

本模块实现一个完整的 Linux MMC 主控驱动，将一块 `vmalloc` 分配的内存（默认 64 MiB）模拟为 eMMC 存储卡。加载后内核会自动完成卡枚举、创建 `/dev/mmcblk0` 块设备，可直接格式化、挂载，无需任何 SD 卡或 eMMC 硬件。

## 知识点

- ✅ `mmc_alloc_host()` / `mmc_add_host()` / `mmc_remove_host()` — MMC 主控生命周期
- ✅ `struct mmc_host_ops.request()` — MMC 命令分发核心回调
- ✅ MMC 命令集 — CMD0/CMD1/CMD2/CMD3/CMD7/CMD9/CMD13/CMD16/CMD17/CMD18/CMD24/CMD25
- ✅ `mmc_request` / `mmc_command` / `mmc_data` 数据结构
- ✅ Scatter-Gather I/O — `sg_virt()` 访问 DMA 缓冲区
- ✅ CID/CSD 寄存器格式 — 卡身份和容量描述
- ✅ `mmc_host.caps` — 声明 4-bit 总线宽度和高速模式

## MMC 卡枚举流程

```
mmc_add_host()
      │
      ▼
MMC 核心发起枚举序列：
  CMD0  → GO_IDLE_STATE（复位）
  CMD1  → SEND_OP_COND（获取 OCR，确认电压范围）
  CMD2  → ALL_SEND_CID（读取卡 ID）
  CMD3  → SET_RELATIVE_ADDR（分配 RCA=1）
  CMD9  → SEND_CSD（读取容量/时序参数）
  CMD7  → SELECT_CARD（进入 Transfer 状态）
  CMD16 → SET_BLOCKLEN（设置块大小为 512B）
      │
      ▼
mmc_card 创建 → mmc_block 创建 → /dev/mmcblk0 出现
```

## 代码结构

### 关键数据结构

```c
struct mmc_virt_host {
    struct mmc_host *mmc;       // MMC 核心分配的主控结构
    void            *card_data; // vmalloc 的 64 MiB 内存（模拟 eMMC）
    size_t           card_size; // 64 * 1024 * 1024 字节
    u16              rca;       // 相对卡地址（CMD3 分配）
};
```

### 命令处理逻辑

| 命令 | 操作码 | 驱动行为 |
|------|--------|---------|
| GO_IDLE_STATE | CMD0 | 无操作，返回成功 |
| SEND_OP_COND | CMD1 | 返回 OCR=0x80FF8000（就绪，3.3V，扇区模式） |
| ALL_SEND_CID | CMD2 | 返回预设 CID（制造商 ID + 产品名） |
| SET_RELATIVE_ADDR | CMD3 | 分配 RCA=1，返回 RCA<<16 |
| SEND_CSD | CMD9 | 返回预设 CSD（描述 64 MiB 容量） |
| SELECT_CARD | CMD7 | 返回 Transfer 状态 |
| READ_SINGLE/MULTIPLE | CMD17/18 | `memcpy(sg_buf ← card_data[arg*512])` |
| WRITE_BLOCK/MULTIPLE | CMD24/25 | `memcpy(card_data[arg*512] ← sg_buf)` |
| SEND_STATUS | CMD13 | 返回 Transfer 状态 |

### Scatter-Gather I/O

```c
// 读操作：从内存卡复制到 SG 缓冲区
for_each_sg(data->sg, sg, data->sg_len, i) {
    memcpy(sg_virt(sg), card_data + offset, sg->length);
    offset += sg->length;
}
```

## 编译

```bash
cd mmc_driver
make
```

## 加载与验证

```bash
# 1. 加载模块
sudo insmod mmc_virt.ko

# 2. 观察卡枚举过程
dmesg | grep -E "(mmc_virt|mmcblk)"
# 预期:
#   mmc_virt: virtual MMC host registered, 64 MiB backing store
#   mmc0: new MMC card at address 0001
#   mmcblk0: mmc0:0001 VRTMC 64.0 MiB

# 3. 确认块设备已创建
lsblk | grep mmcblk
# 预期: mmcblk0    179:0    0   64M  0 disk
```

## 验证实验

### 实验一：格式化并挂载虚拟 eMMC

```bash
# 格式化为 ext4
sudo mkfs.ext4 /dev/mmcblk0
# 预期: Creating filesystem with 65536 1k blocks ...

# 挂载
sudo mkdir -p /mnt/virt_emmc
sudo mount /dev/mmcblk0 /mnt/virt_emmc

# 验证挂载成功
df -h /mnt/virt_emmc
# 预期: /dev/mmcblk0  ... 59M  ... /mnt/virt_emmc
```

### 实验二：文件读写验证

```bash
# 写入文件
echo "Hello eMMC!" | sudo tee /mnt/virt_emmc/test.txt

# 读回验证
cat /mnt/virt_emmc/test.txt
# 预期: Hello eMMC!

# 写入大文件测试吞吐量
sudo dd if=/dev/zero of=/mnt/virt_emmc/bigfile bs=1M count=32
# 预期: 32 MiB 写入成功，速率约 500-2000 MB/s（内存速度）
```

### 实验三：分区表操作

```bash
# 卸载后分区
sudo umount /mnt/virt_emmc
sudo fdisk /dev/mmcblk0
# 创建两个分区：p1=32MiB, p2=剩余

# 查看分区
lsblk /dev/mmcblk0
# 预期:
#   mmcblk0      179:0  0  64M  0 disk
#   ├─mmcblk0p1  179:1  0  32M  0 part
#   └─mmcblk0p2  179:2  0  32M  0 part

# 格式化分区
sudo mkfs.vfat /dev/mmcblk0p1
sudo mkfs.ext4 /dev/mmcblk0p2
```

### 实验四：查看 MMC 卡信息

```bash
# 查看 CID（卡身份信息）
cat /sys/class/mmc_host/mmc0/mmc0:0001/cid
# 预期: 01564d005652544d43010000 00000000

# 查看 CSD（容量/时序参数）
cat /sys/class/mmc_host/mmc0/mmc0:0001/csd

# 查看卡名称
cat /sys/class/mmc_host/mmc0/mmc0:0001/name
# 预期: VRTMC

# 查看卡容量
cat /sys/class/mmc_host/mmc0/mmc0:0001/preferred_erase_size
```

### 实验五：卸载模块

```bash
# 先卸载文件系统
sudo umount /mnt/virt_emmc 2>/dev/null || true

# 卸载模块
sudo rmmod mmc_virt

# 验证设备已消失
lsblk | grep mmcblk
# 预期：无输出
```

## CID 寄存器格式

| 字段 | 位范围 | 本驱动值 | 说明 |
|------|--------|---------|------|
| MID | [127:120] | 0x01 | 制造商 ID（Panasonic） |
| OID | [119:104] | "VM" | OEM/应用 ID |
| PNM | [103:64] | "VRTMC" | 产品名称（5字节） |
| PRV | [63:56] | 0x01 | 产品版本 |
| PSN | [55:24] | 0x000000 | 序列号 |
| MDT | [19:8] | 0x000 | 制造日期 |

## MMC 驱动核心要点

| 要点 | 说明 |
|------|------|
| **mmc_request_done** | `request()` 回调处理完成后必须调用此函数通知 MMC 核心，否则队列死锁 |
| **Scatter-Gather** | 数据缓冲区以 SG 链表形式传入，必须用 `sg_virt()` 或 `sg_copy_*` 访问，不能直接解引用 |
| **OCR 寄存器** | `SEND_OP_COND` 响应中 bit31=0 表示卡就绪，bit30=1 表示扇区寻址（>2GB 卡必须） |
| **RCA** | 相对卡地址，CMD7 的 arg 高 16 位必须匹配，否则卡不响应 |
| **mmc_host.caps** | `MMC_CAP_4_BIT_DATA` 允许 4-bit 总线；`MMC_CAP_MMC_HIGHSPEED` 允许 52MHz |
| **vmalloc vs kmalloc** | 64 MiB 超过 kmalloc 上限，必须用 `vmalloc`；SG 访问时用 `sg_virt()` 获取虚拟地址 |

## 参考

- 《Linux设备驱动开发详解》第18章（存储设备驱动）
- `Documentation/driver-api/mmc/mmc-dev-attrs.rst`
- `drivers/mmc/host/sdhci.c`（标准 SDHCI 主控驱动参考）
- `drivers/mmc/host/mmc_spi.c`（SPI 接口 MMC 驱动参考）
- JEDEC eMMC 规范 JESD84-B51
