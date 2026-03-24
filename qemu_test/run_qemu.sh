#!/usr/bin/env bash
# =============================================================================
# run_qemu.sh — 启动 QEMU 运行驱动测试并收集结果
#
# 功能：
#   1. 检查 QEMU 和 initramfs/vmlinuz 是否就绪
#   2. 以无图形模式启动 QEMU（串口输出到终端/文件）
#   3. 等待测试完成（检测 QEMU_TEST_DONE 标记）
#   4. 解析 [PASS]/[FAIL] 结果并打印汇总
#   5. 返回退出码（0=全部通过，1=有失败）
#
# 用法：
#   bash qemu_test/run_qemu.sh [选项]
#
# 选项：
#   --output-dir <dir>   initramfs/vmlinuz 所在目录（默认：./qemu_test/output）
#   --log <file>         QEMU 串口输出日志文件（默认：output/qemu_serial.log）
#   --timeout <secs>     等待测试完成的超时时间（默认：180 秒）
#   --interactive        交互模式：不自动等待，直接连接串口（调试用）
#   --kvm                尝试启用 KVM 加速（需要宿主机支持）
#   --help               显示帮助信息
#
# 退出码：
#   0   所有测试通过（FAIL=0）
#   1   有测试失败（FAIL>0）
#   2   QEMU 启动失败或超时
# =============================================================================

set -euo pipefail

# ── 颜色输出 ──────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
BLUE='\033[0;34m'; BOLD='\033[1m'; NC='\033[0m'
info()  { echo -e "${BLUE}[INFO]${NC}  $*"; }
ok()    { echo -e "${GREEN}[OK]${NC}    $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }
die()   { error "$*"; exit 2; }

# ── 默认参数 ──────────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="${SCRIPT_DIR}/output"
LOG_FILE=""
TIMEOUT=180
INTERACTIVE=0
USE_KVM=0

# ── 解析命令行参数 ────────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --output-dir)  OUTPUT_DIR="$2"; shift 2 ;;
        --log)         LOG_FILE="$2"; shift 2 ;;
        --timeout)     TIMEOUT="$2"; shift 2 ;;
        --interactive) INTERACTIVE=1; shift ;;
        --kvm)         USE_KVM=1; shift ;;
        --help|-h)
            sed -n '2,35p' "$0" | grep '^#' | sed 's/^# \?//'
            exit 0 ;;
        *) die "未知参数: $1，使用 --help 查看帮助" ;;
    esac
done

VMLINUZ="${OUTPUT_DIR}/vmlinuz"
INITRAMFS="${OUTPUT_DIR}/initramfs.cpio.gz"
[[ -z "${LOG_FILE}" ]] && LOG_FILE="${OUTPUT_DIR}/qemu_serial.log"

echo -e "${BOLD}============================================================${NC}"
echo -e "${BOLD} Linux 驱动 QEMU 测试运行脚本${NC}"
echo -e "${BOLD}============================================================${NC}"

# ── Step 1: 检查依赖 ──────────────────────────────────────────────────────────
info "Step 1/3: 检查依赖..."

if ! command -v qemu-system-x86_64 &>/dev/null; then
    error "未找到 qemu-system-x86_64"
    echo "  安装方法: sudo apt-get install qemu-system-x86"
    exit 2
fi
QEMU_VER=$(qemu-system-x86_64 --version 2>&1 | head -1)
ok "QEMU: ${QEMU_VER}"

[[ -f "${VMLINUZ}" ]]    || die "内核文件不存在: ${VMLINUZ}，请先运行 build_rootfs.sh"
[[ -f "${INITRAMFS}" ]]  || die "initramfs 不存在: ${INITRAMFS}，请先运行 build_rootfs.sh"
ok "vmlinuz   : ${VMLINUZ} ($(du -sh "${VMLINUZ}" | cut -f1))"
ok "initramfs : ${INITRAMFS} ($(du -sh "${INITRAMFS}" | cut -f1))"

# ── Step 2: 构建 QEMU 命令行 ──────────────────────────────────────────────────
info "Step 2/3: 构建 QEMU 命令行..."

mkdir -p "${OUTPUT_DIR}"
rm -f "${LOG_FILE}"

# KVM 加速检测
KVM_ARGS=""
if [[ $USE_KVM -eq 1 ]]; then
    if [[ -c /dev/kvm ]] && [[ -r /dev/kvm ]] && [[ -w /dev/kvm ]]; then
        KVM_ARGS="-enable-kvm -cpu host"
        ok "KVM 加速已启用"
    else
        warn "KVM 不可用（/dev/kvm 不存在或无权限），使用软件模拟"
        KVM_ARGS="-cpu qemu64"
    fi
else
    KVM_ARGS="-cpu qemu64"
fi

# 内核命令行参数
KERNEL_CMDLINE="console=ttyS0 panic=5 quiet loglevel=3"

# QEMU 完整命令
QEMU_CMD=(
    qemu-system-x86_64
    -M q35                          # 使用 Q35 机器类型（支持 PCIe）
    -m 512M                         # 内存 512MB
    -smp 2                          # 2 个 CPU 核心
    ${KVM_ARGS}                     # CPU 类型/KVM
    -kernel "${VMLINUZ}"            # 内核
    -initrd "${INITRAMFS}"          # initramfs
    -append "${KERNEL_CMDLINE}"     # 内核参数
    -nographic                      # 无图形界面
    -serial "file:${LOG_FILE}"      # 串口输出到文件
    -no-reboot                      # 关机后不重启
    -monitor "none"                 # 禁用 QEMU monitor（避免交互）
)

echo "  QEMU 命令："
echo "    ${QEMU_CMD[*]}" | fold -s -w 80 | sed '2,$s/^/    /'
echo ""

# ── 交互模式 ──────────────────────────────────────────────────────────────────
if [[ $INTERACTIVE -eq 1 ]]; then
    warn "交互模式：串口输出到终端（Ctrl+A X 退出 QEMU）"
    QEMU_CMD[-4]="-serial"
    QEMU_CMD[-3]="mon:stdio"
    exec "${QEMU_CMD[@]}"
fi

# ── Step 3: 启动 QEMU 并等待结果 ─────────────────────────────────────────────
info "Step 3/3: 启动 QEMU（超时 ${TIMEOUT}s）..."

# 后台启动 QEMU
"${QEMU_CMD[@]}" &
QEMU_PID=$!
echo "  QEMU PID: ${QEMU_PID}"
echo "  日志文件: ${LOG_FILE}"
echo ""

# 等待测试完成或超时
ELAPSED=0
POLL_INTERVAL=3
FOUND=0

while [[ $ELAPSED -lt $TIMEOUT ]]; do
    sleep $POLL_INTERVAL
    ELAPSED=$((ELAPSED + POLL_INTERVAL))

    # 检查 QEMU 是否还在运行
    if ! kill -0 "$QEMU_PID" 2>/dev/null; then
        ok "QEMU 进程已退出（${ELAPSED}s）"
        FOUND=1
        break
    fi

    # 检查测试完成标记
    if [[ -f "${LOG_FILE}" ]] && grep -q "QEMU_TEST_DONE" "${LOG_FILE}" 2>/dev/null; then
        ok "检测到测试完成标记（${ELAPSED}s）"
        FOUND=1
        # 等待 QEMU 自行关机
        sleep 3
        kill "$QEMU_PID" 2>/dev/null || true
        break
    fi

    # 显示进度
    if [[ -f "${LOG_FILE}" ]]; then
        LINES=$(wc -l < "${LOG_FILE}" 2>/dev/null || echo 0)
        printf "\r  等待中... %ds / %ds（日志 %d 行）" "$ELAPSED" "$TIMEOUT" "$LINES"
    fi
done
echo ""

# 超时处理
if [[ $FOUND -eq 0 ]]; then
    warn "测试超时（${TIMEOUT}s），强制终止 QEMU"
    kill "$QEMU_PID" 2>/dev/null || true
    wait "$QEMU_PID" 2>/dev/null || true
fi

# ── 解析测试结果 ──────────────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}============================================================${NC}"
echo -e "${BOLD} 测试结果${NC}"
echo -e "${BOLD}============================================================${NC}"

if [[ ! -f "${LOG_FILE}" ]] || [[ ! -s "${LOG_FILE}" ]]; then
    error "日志文件为空或不存在: ${LOG_FILE}"
    exit 2
fi

# 打印所有 [PASS]/[FAIL]/[WARN] 行
echo ""
echo "  详细结果："
grep -E "^\[PASS\]|\[FAIL\]|\[WARN\]|--- Ch" "${LOG_FILE}" 2>/dev/null | \
    sed 's/^\[PASS\]/  ✅ [PASS]/; s/^\[FAIL\]/  ❌ [FAIL]/; s/^\[WARN\]/  ⚠️  [WARN]/' || true
echo ""

# 提取汇总数据
PASS_COUNT=$(grep -c "^\[PASS\]" "${LOG_FILE}" 2>/dev/null || echo "0")
FAIL_COUNT=$(grep -c "^\[FAIL\]" "${LOG_FILE}" 2>/dev/null || echo "0")
PASS_COUNT=$(echo "$PASS_COUNT" | tr -d ' \n')
FAIL_COUNT=$(echo "$FAIL_COUNT" | tr -d ' \n')
TOTAL=$((PASS_COUNT + FAIL_COUNT))

echo -e "${BOLD}  汇总：PASS=${PASS_COUNT}  FAIL=${FAIL_COUNT}  TOTAL=${TOTAL}${NC}"
echo ""

# 保存结果摘要
SUMMARY_FILE="${OUTPUT_DIR}/test_summary.txt"
{
    echo "QEMU 测试结果摘要"
    echo "生成时间: $(date)"
    echo "内核: $(strings "${VMLINUZ}" 2>/dev/null | grep -oP 'Linux version \S+' | head -1 || echo 'unknown')"
    echo ""
    echo "PASS: ${PASS_COUNT}"
    echo "FAIL: ${FAIL_COUNT}"
    echo "TOTAL: ${TOTAL}"
    echo ""
    echo "详细结果："
    grep -E "^\[PASS\]|\[FAIL\]" "${LOG_FILE}" 2>/dev/null || true
} > "${SUMMARY_FILE}"
ok "结果摘要已保存: ${SUMMARY_FILE}"

# 返回退出码
if [[ $FAIL_COUNT -eq 0 ]] && [[ $TOTAL -gt 0 ]]; then
    echo -e "${GREEN}${BOLD}  🎉 所有测试通过！${NC}"
    exit 0
elif [[ $TOTAL -eq 0 ]]; then
    error "未检测到任何测试结果，请检查日志: ${LOG_FILE}"
    exit 2
else
    echo -e "${RED}${BOLD}  ❌ 有 ${FAIL_COUNT} 个测试失败${NC}"
    exit 1
fi
