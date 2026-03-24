#!/usr/bin/env bash
# =============================================================================
# build_rootfs.sh — 构建用于 QEMU 驱动测试的最小化 initramfs
#
# 功能：
#   1. 检查依赖（busybox-static、cpio、gzip、qemu-system-x86_64）
#   2. 编译所有驱动模块（make all）
#   3. 构建最小化 rootfs 目录树（busybox + 必要设备节点）
#   4. 将所有 .ko 文件复制到 rootfs/lib/modules/driver_test/
#   5. 生成 init 测试脚本
#   6. 打包为 initramfs.cpio.gz
#
# 用法：
#   cd /path/to/linux_device_driver
#   bash qemu_test/build_rootfs.sh [--output-dir <dir>]
#
# 选项：
#   --output-dir <dir>   指定输出目录（默认：./qemu_test/output）
#   --kernel <path>      指定内核 vmlinuz 路径（默认自动检测）
#   --skip-build         跳过驱动编译，直接使用已有 .ko 文件
#   --help               显示帮助信息
#
# 依赖：
#   Ubuntu/Debian: sudo apt-get install busybox-static cpio gzip qemu-system-x86_64
#   内核头文件:    sudo apt-get install linux-headers-$(uname -r)
# =============================================================================

set -euo pipefail

# ── 颜色输出 ──────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
BLUE='\033[0;34m'; BOLD='\033[1m'; NC='\033[0m'
info()  { echo -e "${BLUE}[INFO]${NC}  $*"; }
ok()    { echo -e "${GREEN}[OK]${NC}    $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }
die()   { error "$*"; exit 1; }

# ── 默认参数 ──────────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
OUTPUT_DIR="${SCRIPT_DIR}/output"
KERNEL_PATH=""
SKIP_BUILD=0

# ── 解析命令行参数 ────────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --output-dir) OUTPUT_DIR="$2"; shift 2 ;;
        --kernel)     KERNEL_PATH="$2"; shift 2 ;;
        --skip-build) SKIP_BUILD=1; shift ;;
        --help|-h)
            sed -n '2,30p' "$0" | grep '^#' | sed 's/^# \?//'
            exit 0 ;;
        *) die "未知参数: $1，使用 --help 查看帮助" ;;
    esac
done

ROOTFS_DIR="${OUTPUT_DIR}/rootfs"
INITRAMFS="${OUTPUT_DIR}/initramfs.cpio.gz"

echo -e "${BOLD}============================================================${NC}"
echo -e "${BOLD} Linux 驱动 QEMU 测试环境构建脚本${NC}"
echo -e "${BOLD}============================================================${NC}"
echo "  仓库目录: ${REPO_DIR}"
echo "  输出目录: ${OUTPUT_DIR}"
echo ""

# ── Step 1: 检查依赖 ──────────────────────────────────────────────────────────
info "Step 1/5: 检查依赖..."

check_cmd() {
    if ! command -v "$1" &>/dev/null; then
        error "缺少命令: $1"
        echo "  安装方法: sudo apt-get install $2"
        return 1
    fi
    ok "$1 已安装"
}

DEPS_OK=1
check_cmd busybox  "busybox-static"  || DEPS_OK=0
check_cmd cpio     "cpio"            || DEPS_OK=0
check_cmd gzip     "gzip"            || DEPS_OK=0
check_cmd make     "build-essential" || DEPS_OK=0

if [[ $DEPS_OK -eq 0 ]]; then
    echo ""
    echo "请先安装缺失的依赖："
    echo "  sudo apt-get install -y busybox-static cpio gzip build-essential"
    echo "  sudo apt-get install -y linux-headers-\$(uname -r)"
    die "依赖检查失败"
fi

# 检查 busybox 是否为静态链接
if ! file "$(command -v busybox)" | grep -q "statically linked"; then
    warn "busybox 不是静态链接版本，在 QEMU initramfs 中可能无法运行"
    warn "建议安装: sudo apt-get install busybox-static"
fi

# ── Step 2: 编译驱动模块 ──────────────────────────────────────────────────────
if [[ $SKIP_BUILD -eq 0 ]]; then
    info "Step 2/5: 编译所有驱动模块..."
    cd "${REPO_DIR}"

    # 检查内核头文件
    KVER=$(uname -r)
    # 优先使用 5.15.0-173-generic（与 QEMU 内核匹配）
    for kver in "5.15.0-173-generic" "$KVER"; do
        if [[ -d "/lib/modules/${kver}/build" ]]; then
            KBUILD="/lib/modules/${kver}/build"
            KVER_BUILD="$kver"
            break
        fi
    done
    if [[ -z "${KBUILD:-}" ]]; then
        die "未找到内核头文件目录 /lib/modules/*/build，请安装: sudo apt-get install linux-headers-\$(uname -r)"
    fi
    ok "使用内核头文件: ${KBUILD} (${KVER_BUILD})"

    # 编译
    BUILD_LOG="${OUTPUT_DIR}/build.log"
    mkdir -p "${OUTPUT_DIR}"
    if make -C "${REPO_DIR}" KERNEL_DIR="${KBUILD}" all 2>&1 | tee "${BUILD_LOG}" | \
       grep -E "^(  (CC|LD|AR)|✅|❌|Error|error:)" | tail -30; then
        ok "驱动编译完成，日志: ${BUILD_LOG}"
    else
        warn "编译可能有警告，请检查 ${BUILD_LOG}"
    fi
else
    info "Step 2/5: 跳过编译（--skip-build）"
fi

# ── Step 3: 构建 rootfs 目录树 ────────────────────────────────────────────────
info "Step 3/5: 构建 rootfs 目录树..."

# 清理并重建
rm -rf "${ROOTFS_DIR}"
mkdir -p "${ROOTFS_DIR}"/{bin,sbin,dev,proc,sys,tmp,mnt,lib/modules/driver_test,etc/init.d}

# 复制 busybox（静态链接版本）
BUSYBOX_BIN=$(command -v busybox)
cp "${BUSYBOX_BIN}" "${ROOTFS_DIR}/bin/busybox"
chmod +x "${ROOTFS_DIR}/bin/busybox"

# 创建 busybox applet 符号链接
for applet in sh ash cat echo ls grep insmod rmmod lsmod \
              sleep mount umount mkdir mknod ip dmesg \
              timeout poweroff reboot; do
    ln -sf busybox "${ROOTFS_DIR}/bin/${applet}" 2>/dev/null || true
done
# sbin 中的命令
for applet in modprobe; do
    ln -sf ../bin/busybox "${ROOTFS_DIR}/sbin/${applet}" 2>/dev/null || true
done

ok "busybox 已安装到 rootfs: $(ls ${ROOTFS_DIR}/bin/ | wc -l) 个命令"

# ── Step 4: 复制 .ko 文件 ─────────────────────────────────────────────────────
info "Step 4/5: 收集并复制驱动模块 (.ko)..."

KO_COUNT=0
KO_MISSING=""

# 驱动模块映射表：章节目录 -> 模块文件名前缀
declare -A KO_MAP=(
    ["ch01_kernel_basics"]="kfifo_demo_static"
    ["ch02_char_basic"]="globalmem"
    ["ch03_char_advanced"]="globalfifo"
    ["ch04_timer"]="second"
    ["ch05_misc"]="misc_demo"
    ["ch06_platform"]="platform_demo"
    ["ch07_input"]="input_demo"
    ["ch08_regmap"]="regmap_demo"
    ["ch09_watchdog"]="watchdog_demo"
    ["ch10_rtc"]="rtc_demo"
    ["ch11_pwm"]="pwm_demo"
    ["ch12_dma"]="dma_demo"
    ["ch13_net_virtual"]="snull"
    ["ch14_net_mac_phy"]="eth_mac"
    ["ch15_i2c"]="i2c_virt_master i2c_virt_slave"
    ["ch16_spi"]="spi_virt_master spi_virt_slave"
    ["ch17_block"]="vmem_disk"
    ["ch18_mmc"]="mmc_virt"
)

for ch_dir in "${!KO_MAP[@]}"; do
    src_dir="${REPO_DIR}/${ch_dir}"
    for ko_name in ${KO_MAP[$ch_dir]}; do
        ko_file="${src_dir}/${ko_name}.ko"
        if [[ -f "${ko_file}" ]]; then
            # 目标文件名：ch目录名_模块名.ko（避免重名）
            dst_name="${ch_dir}_${ko_name}.ko"
            cp "${ko_file}" "${ROOTFS_DIR}/lib/modules/driver_test/${dst_name}"
            ok "  ✅ ${dst_name}"
            KO_COUNT=$((KO_COUNT + 1))
        else
            warn "  ⚠️  未找到: ${ko_file}"
            KO_MISSING="${KO_MISSING} ${ch_dir}/${ko_name}.ko"
        fi
    done
done

echo ""
ok "共复制 ${KO_COUNT} 个 .ko 文件"
if [[ -n "${KO_MISSING}" ]]; then
    warn "以下模块未找到（可能编译失败）:${KO_MISSING}"
fi

# ── Step 5: 生成 init 脚本 ────────────────────────────────────────────────────
info "Step 5/5: 生成 init 测试脚本..."

cp "${SCRIPT_DIR}/init.sh" "${ROOTFS_DIR}/init"
chmod +x "${ROOTFS_DIR}/init"
ok "init 脚本已写入 ${ROOTFS_DIR}/init"

# ── 打包 initramfs ────────────────────────────────────────────────────────────
info "打包 initramfs.cpio.gz..."
(cd "${ROOTFS_DIR}" && find . | cpio -H newc -o 2>/dev/null | gzip > "${INITRAMFS}")
INITRAMFS_SIZE=$(du -sh "${INITRAMFS}" | cut -f1)
ok "initramfs 打包完成: ${INITRAMFS} (${INITRAMFS_SIZE})"

# ── 自动检测/复制内核 ─────────────────────────────────────────────────────────
if [[ -z "${KERNEL_PATH}" ]]; then
    # 按优先级搜索内核
    for candidate in \
        "/boot/vmlinuz-5.15.0-173-generic" \
        "/boot/vmlinuz-$(uname -r)" \
        "/boot/vmlinuz"; do
        if [[ -f "${candidate}" ]]; then
            KERNEL_PATH="${candidate}"
            break
        fi
    done
fi

if [[ -n "${KERNEL_PATH}" && -f "${KERNEL_PATH}" ]]; then
    KERNEL_DST="${OUTPUT_DIR}/vmlinuz"
    if [[ "${KERNEL_PATH}" != "${KERNEL_DST}" ]]; then
        cp "${KERNEL_PATH}" "${KERNEL_DST}"
    fi
    ok "内核已就绪: ${KERNEL_DST} ($(du -sh "${KERNEL_DST}" | cut -f1))"
else
    warn "未找到内核文件，请手动指定: --kernel /boot/vmlinuz-X.Y.Z"
    warn "或安装: sudo apt-get install linux-image-5.15.0-173-generic"
fi

# ── 完成摘要 ──────────────────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}============================================================${NC}"
echo -e "${BOLD} 构建完成！${NC}"
echo -e "${BOLD}============================================================${NC}"
echo "  initramfs : ${INITRAMFS}"
echo "  内核      : ${OUTPUT_DIR}/vmlinuz"
echo "  .ko 数量  : ${KO_COUNT}"
echo ""
echo "  下一步：运行 QEMU 测试"
echo "    bash ${SCRIPT_DIR}/run_qemu.sh"
echo "  或一键测试："
echo "    bash ${SCRIPT_DIR}/qemu_test_all.sh"
echo ""
