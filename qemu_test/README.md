# QEMU 驱动测试基础设施

本目录提供了一套完整的脚本，用于在 QEMU x86_64 虚拟机中对所有驱动模块进行自动化运行时测试。整个流程无需真实硬件，可在任何支持 QEMU 的 Linux 主机上复现。

## 目录结构

```
qemu_test/
├── qemu_test_all.sh   # 一键入口：编译 + 构建 rootfs + 运行 QEMU 测试
├── build_rootfs.sh    # 构建最小化 busybox initramfs
├── run_qemu.sh        # 启动 QEMU 并收集测试结果
├── init.sh            # QEMU 内部的 /init 测试脚本（busybox sh）
├── README.md          # 本文档
└── output/            # 构建产物（由脚本自动生成，不纳入版本控制）
    ├── vmlinuz              # 内核镜像（从 /boot 复制）
    ├── initramfs.cpio.gz    # 打包好的 initramfs
    ├── rootfs/              # rootfs 目录树
    ├── qemu_serial.log      # QEMU 串口输出日志
    ├── test_summary.txt     # 测试结果摘要
    └── build.log            # 驱动编译日志
```

## 快速开始

### 1. 安装依赖

```bash
# QEMU 和 busybox（静态链接版本）
sudo apt-get install -y \
    qemu-system-x86 \
    busybox-static \
    cpio gzip \
    build-essential \
    linux-headers-$(uname -r)
```

### 2. 一键运行全量测试

```bash
cd /path/to/linux_device_driver
bash qemu_test/qemu_test_all.sh
```

预期输出示例：

```
╔══════════════════════════════════════════════════════════╗
║       Linux 驱动模块 QEMU 全量测试套件                   ║
╚══════════════════════════════════════════════════════════╝
  开始时间: Mon Mar 24 10:00:00 UTC 2026
  输出目录: qemu_test/output

━━━ Phase 1/2: 构建 rootfs ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
[OK]    busybox 已安装到 rootfs: 22 个命令
[OK]    共复制 20 个 .ko 文件
[OK]    initramfs 打包完成: qemu_test/output/initramfs.cpio.gz (3.0M)

━━━ Phase 2/2: 运行 QEMU 测试 ━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  ✅ [PASS] ch01_kfifo insmod+rmmod
  ✅ [PASS] ch02_globalmem rw
  ...
  ✅ [PASS] ch18_mmc dev

  汇总：PASS=19  FAIL=0  TOTAL=19

  🎉 所有测试通过！
```

## 分步运行

### 仅构建 rootfs

```bash
bash qemu_test/build_rootfs.sh
```

常用选项：

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `--output-dir <dir>` | 指定输出目录 | `qemu_test/output` |
| `--kernel <path>` | 指定内核 vmlinuz 路径 | 自动检测 `/boot/vmlinuz-*` |
| `--skip-build` | 跳过驱动编译，直接使用已有 `.ko` | 否 |

### 仅运行 QEMU 测试

```bash
bash qemu_test/run_qemu.sh
```

常用选项：

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `--output-dir <dir>` | initramfs/vmlinuz 所在目录 | `qemu_test/output` |
| `--log <file>` | 串口输出日志路径 | `output/qemu_serial.log` |
| `--timeout <secs>` | 等待测试完成的超时时间 | `180` 秒 |
| `--kvm` | 启用 KVM 加速（需要 `/dev/kvm`） | 否 |
| `--interactive` | 交互模式（串口连接到终端，调试用） | 否 |

## 测试架构

### 工作原理

```
宿主机                              QEMU 虚拟机 (x86_64)
─────────────────────               ─────────────────────────────────
build_rootfs.sh                     内核启动
  ├── make all (编译 .ko)            │
  ├── 构建 busybox rootfs            └── /init (init.sh)
  ├── 复制 .ko → rootfs/                  ├── 挂载 proc/sysfs/devtmpfs
  └── 打包 initramfs.cpio.gz              ├── 逐章 insmod .ko
                                          ├── 验证设备节点/sysfs
run_qemu.sh                               ├── 输出 [PASS]/[FAIL]
  ├── 启动 qemu-system-x86_64             └── poweroff -f
  ├── 监控串口输出
  ├── 检测 QEMU_TEST_DONE 标记
  └── 解析并汇总结果
```

### QEMU 配置

| 参数 | 值 | 说明 |
|------|----|------|
| 机器类型 | `q35` | 支持 PCIe 的现代 PC 机型 |
| 内存 | `512M` | 足够运行所有驱动模块 |
| CPU | `2 核` | 测试 SMP 相关驱动 |
| 串口 | `file:qemu_serial.log` | 无图形模式，输出到文件 |
| 内核参数 | `console=ttyS0 panic=5 quiet` | 串口控制台，崩溃后 5 秒重启 |

### init 测试脚本逻辑

`init.sh` 在 QEMU 内作为 PID 1 运行，对每个章节执行以下验证：

1. **insmod**：通过 `timeout 5 insmod` 加载模块（防止驱动挂起）
2. **功能验证**：检查设备节点（`/dev/xxx`）、sysfs 条目（`/sys/class/xxx`）或网络接口
3. **rmmod**：卸载模块，验证资源正确释放
4. **输出结果**：`[PASS] 描述` 或 `[FAIL] 描述: 错误信息`

## 测试覆盖范围

| 章节 | 驱动模块 | 验证内容 |
|------|---------|---------|
| Ch01 | `kfifo_demo_static` | insmod/rmmod 正常 |
| Ch02 | `globalmem` | `/dev/globalmem` 读写 |
| Ch03 | `globalfifo` | `/dev/globalfifo` 写入 |
| Ch04 | `second` | `/dev/second` 设备节点 |
| Ch05 | `misc_demo` | `/dev/misc_demo` 设备节点 |
| Ch06 | `platform_demo` | `/sys/bus/platform/devices/` sysfs 条目 |
| Ch07 | `input_demo` | `/dev/input/eventN` 输入设备 |
| Ch08 | `regmap_demo` | insmod/rmmod 正常 |
| Ch09 | `watchdog_demo` | `/dev/watchdog` 设备节点 |
| Ch10 | `rtc_demo` | `/dev/rtc` 设备节点 |
| Ch11 | `pwm_demo` | `/sys/class/pwm/pwmchipN` sysfs |
| Ch12 | `dma_demo` | insmod/rmmod 正常（DMA 内存分配） |
| Ch13 | `snull` | `sn0`/`sn1` 网络接口 |
| Ch14 | `eth_mac` | 虚拟以太网接口注册 |
| Ch15 | `i2c_virt_master` + `i2c_virt_slave` | `/dev/i2c_virt` 设备节点 |
| Ch16 | `spi_virt_master` + `spi_virt_slave` | `/dev/spi_virt` 设备节点 |
| Ch17 | `vmem_disk` | `/dev/vmem_disk` 块设备 |
| Ch18 | `mmc_virt` | `/dev/mmcblk0` MMC 卡枚举 |

## 常见问题

**Q: `busybox: command not found`**

```bash
sudo apt-get install busybox-static
```

**Q: 内核版本不匹配（模块加载失败 `Invalid module format`）**

`build_rootfs.sh` 会自动优先使用 `5.15.0-173-generic` 内核头文件编译模块，并将对应的 `vmlinuz` 复制到输出目录。如果宿主机没有该版本，需要安装：

```bash
sudo apt-get install linux-headers-5.15.0-173-generic \
                     linux-image-5.15.0-173-generic
```

**Q: QEMU 测试超时**

增大超时时间（默认 180 秒）：

```bash
bash qemu_test/qemu_test_all.sh --timeout 300
```

**Q: 调试某个驱动**

使用交互模式进入 QEMU 串口控制台：

```bash
bash qemu_test/run_qemu.sh --interactive
```

**Q: 在 CI/CD 环境中运行**

```yaml
# GitHub Actions 示例
- name: Install dependencies
  run: |
    sudo apt-get install -y qemu-system-x86 busybox-static cpio gzip \
      linux-headers-5.15.0-173-generic linux-image-5.15.0-173-generic

- name: Run QEMU tests
  run: bash qemu_test/qemu_test_all.sh --timeout 240
```

## 已知测试结果

在以下环境中验证通过：

| 项目 | 版本 |
|------|------|
| 宿主机内核 | Ubuntu 22.04 / 5.15.0-173-generic |
| QEMU | 6.2.0 |
| busybox | 1.30.1 (静态链接) |
| 测试结果 | PASS: 19 / FAIL: 0 |
