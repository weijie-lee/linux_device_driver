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

# 每个 busybox applet 一条符号链接。kernel/ 的实验用到 taskset、cmp、stat、usleep 等。
for applet in $(busybox --list); do
    [[ "${applet}" == "busybox" ]] && continue
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
    ["ch15_i2c"]="i2c_master i2c_slave"
    ["ch16_spi"]="spi_master spi_slave"
    ["ch17_block"]="vmem_disk"
    ["ch18_mmc"]="mmc_virt"
    ["ch19_pci_bus"]="pci_basic pci_advanced"
    ["ch20_usb_bus"]="usb_basic usb_advanced"
    ["ch21_gpio"]="virt_gpio"
    ["ch22_pinctrl"]="virt_pinctrl"
    ["ch23_clk"]="virt_clk"
    ["ch24_regulator"]="virt_regulator"
    ["ch25_reset"]="virt_reset"
    ["ch26_nvmem"]="virt_nvmem"
    ["ch27_hwmon"]="virt_hwmon"
    ["ch28_leds"]="virt_led"
    ["ch29_thermal"]="virt_thermal"
    ["ch30_power"]="virt_psy"
    ["ch31_backlight"]="virt_backlight"
    ["ch32_phy"]="virt_phy"
    ["ch33_pps"]="virt_pps"
    ["ch34_ptp"]="virt_ptp"
    ["ch35_tty"]="virt_tty"
    ["ch36_fb"]="virt_fb"
    ["ch37_mfd"]="virt_mfd"
    ["ch38_extcon"]="virt_extcon"
    ["ch39_scsi"]="virt_scsi"
    ["ch40_crypto"]="virt_crypto"
    ["ch41_virtio"]="virt_virtio"
    ["ch42_remoteproc"]="virt_rproc"
    ["ch43_devfreq"]="virt_devfreq"
    ["ch44_dmaengine"]="virt_dmaeng"
    ["ch45_rfkill"]="virt_rfkill"
    ["ch46_hwspinlock"]="virt_hwspin"
    ["ch47_mailbox"]="virt_mbox"
    ["ch48_powercap"]="virt_powercap"
    ["ch49_opp"]="virt_opp"
    ["ch50_mdio"]="virt_mdio"
    ["ch51_iio"]="virt_iio"
    ["ch52_alsa"]="virt_alsa"
    ["ch53_hid"]="virt_hid"
    ["ch54_mtd"]="virt_mtd"
    ["ch55_uio"]="virt_uio"
    ["ch56_w1"]="virt_w1"
    ["ch57_can"]="virt_can"
    ["ch58_mux"]="virt_mux"
    ["ch59_fpga"]="virt_fpga"
    ["ch60_spmi"]="virt_spmi"
    ["ch61_i3c"]="virt_i3c"
    ["ch62_gnss"]="virt_gnss"
    ["ch63_nfc"]="virt_nfc"
    ["ch64_bluetooth"]="virt_bt"
    ["ch65_v4l2"]="virt_v4l2"
    ["ch66_counter"]="virt_counter"
    ["ch67_drm"]="virt_drm"
    ["ch68_interconnect"]="virt_icc"
    ["learn/01_uapi"]="virt_uapi"
    ["learn/02_devicetree"]="virt_dt"
    ["learn/03_irq"]="virt_irq"
    ["learn/04_soc_sensor"]="virt_sensor"
    ["learn/05_pm"]="virt_pm"
    ["learn/06_dma"]="virt_dma"
    ["learn/07_debug"]="virt_debug"
    ["hardcode"]="hardcode_samples"
    ["kernel/01_driver_framework"]="virt_framework"
    ["kernel/02_kernel_interface"]="virt_kif"
    ["kernel/03_driver_calls"]="virt_call"
    ["kernel/04_user_paths"]="virt_upath"
    ["kernel/05_sleep_wake"]="virt_sleep"
    ["kernel/06_irq_bottom"]="virt_bottom"
    ["kernel/07_lock_context"]="virt_lock"
    ["kernel/08_pci_mmio"]="virt_mmio"
)

for ch_dir in "${!KO_MAP[@]}"; do
    src_dir="${REPO_DIR}/${ch_dir}"
    for ko_name in ${KO_MAP[$ch_dir]}; do
        ko_file="${src_dir}/${ko_name}.ko"
        if [[ -f "${ko_file}" ]]; then
            # 目标文件名：目录名_模块名.ko。learn/01_uapi 里的斜杠换成下划线。
            dst_name="${ch_dir//\//_}_${ko_name}.ko"
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

# 部分子系统在 5.15 generic 里是模块（=m）。把它们的依赖 .ko
# 按 modinfo 依赖顺序放进 initramfs，否则 virt_*.ko 会因未知符号加载失败。
if [[ -z "${KVER_BUILD:-}" ]]; then
    KVER_BUILD="5.15.0-173-generic"
fi
PRELOAD_DIR="${ROOTFS_DIR}/lib/modules/driver_test/preload"
mkdir -p "${PRELOAD_DIR}"
declare -A PRE_SEEN=()
PRE_ORDER=()
resolve_dep() {
    local ko="$1"
    local base deps d f
    [[ -f "${ko}" ]] || return 0
    base="$(basename "${ko}")"
    [[ -n "${PRE_SEEN[${base}]:-}" ]] && return 0
    PRE_SEEN[${base}]=1
    deps="$(modinfo -F depends "${ko}" 2>/dev/null || true)"
    deps="${deps//,/ }"
    for d in ${deps}; do
        [[ -z "${d}" ]] && continue
        f="$(find "/lib/modules/${KVER_BUILD}" -name "${d}.ko" -print -quit 2>/dev/null || true)"
        if [[ -n "${f}" ]]; then
            resolve_dep "${f}"
        else
            warn "依赖模块未找到: ${d}.ko ($(basename "${ko}"))"
        fi
    done
    if [[ "${ko}" == /lib/modules/* ]]; then
        PRE_ORDER+=("${ko}")
    fi
}
shopt -s nullglob
for ko in "${ROOTFS_DIR}/lib/modules/driver_test/"*.ko; do
    resolve_dep "${ko}"
done
shopt -u nullglob
: > "${ROOTFS_DIR}/lib/modules/driver_test/preload.list"
for ko in "${PRE_ORDER[@]}"; do
    base="$(basename "${ko}")"
    cp "${ko}" "${PRELOAD_DIR}/${base}"
    echo "${base}" >> "${ROOTFS_DIR}/lib/modules/driver_test/preload.list"
done
ok "预加载依赖模块: $(wc -l < "${ROOTFS_DIR}/lib/modules/driver_test/preload.list" | tr -d ' ') 个"

info "编译用户态 uapi_test（静态链接，initramfs 里没有动态装载器）..."
gcc -static -O2 -Wall -o "${ROOTFS_DIR}/bin/uapi_test" \
    "${REPO_DIR}/learn/01_uapi/uapi_test.c"
chmod 755 "${ROOTFS_DIR}/bin/uapi_test"
ok "uapi_test 已放入 rootfs"

# 各章 tests/ 里的用户态程序。静态链接后放进 /bin，由 lab.sh 在客户机里执行。
USER_TESTS=(
    "ch02_char_basic/tests/test_globalmem.c"
    "ch03_char_advanced/tests/test_globalfifo.c"
    "ch05_misc/tests/test_misc.c"
    "ch06_platform/tests/test_platform.c"
    "ch08_regmap/tests/test_regmap.c"
    "kernel/04_user_paths/upath_ioctl.c"
    "kernel/12_fault/fault_touch.c"
    "kernel/14_block_mq/blk_direct.c"
    "kernel/21_events/uevent_listen.c"
    "kernel/25_wait_forms/poll_wait.c"
)
for src in "${USER_TESTS[@]}"; do
    bin="$(basename "${src}" .c)"
    if gcc -static -O2 -Wall -o "${ROOTFS_DIR}/bin/${bin}" "${REPO_DIR}/${src}"; then
        chmod 755 "${ROOTFS_DIR}/bin/${bin}"
        ok "用户态测试 ${bin} 已放入 rootfs"
    else
        die "编译 ${src} 失败"
    fi
done

# kernel/04 用 32 位程序走 compat_ioctl。需要 gcc-multilib。
if gcc -m32 -static -O2 -Wall -o "${ROOTFS_DIR}/bin/upath_ioctl32" \
       "${REPO_DIR}/kernel/04_user_paths/upath_ioctl.c"; then
    chmod 755 "${ROOTFS_DIR}/bin/upath_ioctl32"
    ok "32 位 upath_ioctl32 已放入 rootfs"
else
    die "编译 32 位 upath_ioctl32 失败（缺 gcc-multilib？）"
fi

# kernel/18 的固件。ldd-v2/ 给 firmware_class.path 换搜索目录用。
mkdir -p "${ROOTFS_DIR}/lib/firmware/ldd-v2"
cp "${REPO_DIR}/kernel/18_firmware/virt_fw.bin" "${ROOTFS_DIR}/lib/firmware/"
cp "${REPO_DIR}/kernel/18_firmware/ldd-v2/virt_fw.bin" "${ROOTFS_DIR}/lib/firmware/ldd-v2/"
ok "固件 virt_fw.bin 已放入 rootfs"

# ── Step 5: 生成 init 脚本 ────────────────────────────────────────────────────
info "Step 5/5: 生成 init 测试脚本..."

cp "${SCRIPT_DIR}/init.sh" "${ROOTFS_DIR}/init"
cp "${SCRIPT_DIR}/lab.sh" "${ROOTFS_DIR}/lab.sh"
chmod +x "${ROOTFS_DIR}/init" "${ROOTFS_DIR}/lab.sh"
mkdir -p "${ROOTFS_DIR}/lab.d"
for f in "${REPO_DIR}"/kernel/[0-9][0-9]_*/lab.sh; do
    [[ -f "${f}" ]] || continue
    chapter="$(basename "$(dirname "${f}")")"
    cp "${f}" "${ROOTFS_DIR}/lab.d/kernel${chapter%%_*}.sh"
done
ok "kernel/ 各章实验脚本: $(ls "${ROOTFS_DIR}/lab.d" | wc -l) 个"
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
