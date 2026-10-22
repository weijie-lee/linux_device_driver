# QEMU 驱动测试基础设施

本目录提供了一套完整的脚本，用于在 QEMU x86_64 虚拟机中对所有驱动模块进行自动化运行时测试。整个流程无需真实硬件，可在任何支持 QEMU 的 Linux 主机上复现。

## 目录结构

```
qemu_test/
├── qemu_test_all.sh   # 一键入口：编译 + 构建 rootfs + 运行 QEMU 测试
├── build_rootfs.sh    # 构建最小化 busybox initramfs
├── run_qemu.sh        # 启动 QEMU 并收集测试结果
├── run_chapter.sh     # 只跑一章，并打印客户机里的命令和回显
├── lab.sh             # 客户机里的单章实验（由 init.sh 在 ldd_chapter= 时调用）
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

## 单章复现

每一章 README 的「在镜像里复现」给出了要执行的命令、客户机回显和成功标准。镜像入口是：

```bash
docker build -t ldd-lab .
docker run --rm ldd-lab chapter ch21
```

`chapter` 调用 `run_chapter.sh`。它打包 initramfs，再用 `run_qemu.sh --chapter` 把 `ldd_chapter=` 写进内核命令行。客户机里的 `init.sh` 看到这个参数后改跑 `lab.sh`，逐条打印命令和回显，最后输出 `[PASS]` 或 `[FAIL]`。`ch00` 没有模块，只在镜像里核对 `driver_template.c`。

章节名用 `ch01`、`ch21`、`learn01`、`hardcode`，或 `all`。

改了命令或回显之后，用 `docker run --rm ldd-lab chapter all > /tmp/all.log` 取一份日志，再运行 `python3 qemu_test/refresh_transcripts.py /tmp/all.log`。它只刷新各章 README 里的回显代码块，知识点和成功判据是手写的。

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
| `--timeout <secs>` | 等待测试完成的超时时间 | `600` 秒 |
| `--chapter <name>` | 只跑 `lab.sh` 中的一章，并打印命令和回显 | 全量 `init.sh` |
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

有两套检查，都在 5.15.0-173-generic 客户机里运行：

| 命令 | 检查什么 | 项数 |
|------|---------|------|
| `docker run --rm ldd-lab test`（`init.sh`） | 每个模块 `insmod`，再看设备节点、sysfs 或网卡；`rmmod` 失败不算失败。`learn/` 的 7 个实验、`hardcode/`、`kernel/01` 和 `kernel/02` 做功能检查 | 80 |
| `docker run --rm ldd-lab chapter all`（`lab.sh`） | 每章打印命令和回显。`rmmod` 失败、期望字样缺失、设备节点不是字符设备或块设备，都算失败。`kernel01` 要求 trace 整行一致，`kernel02` 要求 ftrace 顺序一致 | 78 |

每一章的命令、回显和成功判据写在该章 README 的「在镜像里复现」里。`ch02`、`ch03`、`ch05`、`ch06`、`ch08` 还会运行各自 `tests/` 里的用户态程序，要求 `FAIL: 0`。`ch21`–`ch68` 多数只确认模块加载后 probe 建出了自己的节点并且能卸载，没有逐个驱动去做读写。

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

增大超时时间（默认 600 秒）：

```bash
bash qemu_test/qemu_test_all.sh --timeout 900
```

**Q: 调试某个驱动**

使用交互模式进入 QEMU 串口控制台：

```bash
bash qemu_test/run_qemu.sh --interactive
```

**Q: 在 CI/CD 环境中运行**

直接用镜像，不需要在 runner 上装 QEMU 和 5.15 头文件。仓库里的 `.github/workflows/docker-validation.yml` 就是这样做的：

```yaml
- name: Build Docker image
  run: docker build -t ldd-lab-ci .

- name: Run the smoke suite in the container
  run: docker run --rm ldd-lab-ci test

- name: Run every chapter lab from the READMEs
  run: docker run --rm ldd-lab-ci chapter all
```

## 已知测试结果

CI（`.github/workflows/docker-validation.yml`）在 `ubuntu-22.04` 上构建镜像，依次运行 `test` 和 `chapter all`，失败时上传 `qemu_serial.log`。

| 项目 | 版本 |
|------|------|
| 客户机内核 | Ubuntu 5.15.0-173-generic |
| QEMU | 6.2.0（镜像内） |
| busybox | 静态链接（镜像内） |
| `test` | PASS 80 / FAIL 0 |
| `chapter all` | PASS 78 / FAIL 0 |
