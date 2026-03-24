#!/usr/bin/env bash
# =============================================================================
# qemu_test_all.sh — 一键完成驱动编译、rootfs 构建和 QEMU 测试
#
# 用法：
#   cd /path/to/linux_device_driver
#   bash qemu_test/qemu_test_all.sh [选项]
#
# 选项：
#   --output-dir <dir>   指定输出目录（默认：./qemu_test/output）
#   --kernel <path>      指定内核 vmlinuz 路径（默认自动检测）
#   --timeout <secs>     QEMU 测试超时时间（默认：180 秒）
#   --skip-build         跳过驱动编译，直接使用已有 .ko 文件
#   --kvm                尝试启用 KVM 加速
#   --interactive        交互模式（调试用，不自动等待）
#   --help               显示帮助信息
#
# 工作流程：
#   1. build_rootfs.sh  — 编译驱动 + 构建 busybox rootfs + 打包 initramfs
#   2. run_qemu.sh      — 启动 QEMU + 等待测试 + 解析结果
#
# 退出码：
#   0   所有测试通过
#   1   有测试失败
#   2   构建或 QEMU 启动失败
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ── 颜色输出 ──────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
BLUE='\033[0;34m'; BOLD='\033[1m'; NC='\033[0m'
info()  { echo -e "${BLUE}[INFO]${NC}  $*"; }
ok()    { echo -e "${GREEN}[OK]${NC}    $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }

# ── 解析参数（透传给子脚本）──────────────────────────────────────────────────
BUILD_ARGS=()
RUN_ARGS=()
OUTPUT_DIR="${SCRIPT_DIR}/output"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --output-dir)
            OUTPUT_DIR="$2"
            BUILD_ARGS+=(--output-dir "$2")
            RUN_ARGS+=(--output-dir "$2")
            shift 2 ;;
        --kernel)
            BUILD_ARGS+=(--kernel "$2")
            shift 2 ;;
        --timeout)
            RUN_ARGS+=(--timeout "$2")
            shift 2 ;;
        --skip-build)
            BUILD_ARGS+=(--skip-build)
            shift ;;
        --kvm)
            RUN_ARGS+=(--kvm)
            shift ;;
        --interactive)
            RUN_ARGS+=(--interactive)
            shift ;;
        --help|-h)
            sed -n '2,30p' "$0" | grep '^#' | sed 's/^# \?//'
            exit 0 ;;
        *)
            error "未知参数: $1，使用 --help 查看帮助"
            exit 2 ;;
    esac
done

# ── 打印横幅 ──────────────────────────────────────────────────────────────────
START_TIME=$(date +%s)
echo ""
echo -e "${BOLD}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}║       Linux 驱动模块 QEMU 全量测试套件                   ║${NC}"
echo -e "${BOLD}╚══════════════════════════════════════════════════════════╝${NC}"
echo "  开始时间: $(date)"
echo "  输出目录: ${OUTPUT_DIR}"
echo ""

# ── Phase 1: 构建 rootfs ──────────────────────────────────────────────────────
echo -e "${BOLD}━━━ Phase 1/2: 构建 rootfs ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
if bash "${SCRIPT_DIR}/build_rootfs.sh" "${BUILD_ARGS[@]}"; then
    ok "rootfs 构建完成"
else
    error "rootfs 构建失败，退出"
    exit 2
fi

echo ""

# ── Phase 2: 运行 QEMU 测试 ───────────────────────────────────────────────────
echo -e "${BOLD}━━━ Phase 2/2: 运行 QEMU 测试 ━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
QEMU_EXIT=0
bash "${SCRIPT_DIR}/run_qemu.sh" "${RUN_ARGS[@]}" || QEMU_EXIT=$?

# ── 最终汇总 ──────────────────────────────────────────────────────────────────
END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))
MINUTES=$((ELAPSED / 60))
SECONDS=$((ELAPSED % 60))

echo ""
echo -e "${BOLD}╔══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}║                    测试完成                              ║${NC}"
echo -e "${BOLD}╚══════════════════════════════════════════════════════════╝${NC}"
echo "  总耗时: ${MINUTES}m ${SECONDS}s"
echo "  日志  : ${OUTPUT_DIR}/qemu_serial.log"
echo "  摘要  : ${OUTPUT_DIR}/test_summary.txt"
echo ""

if [[ $QEMU_EXIT -eq 0 ]]; then
    echo -e "  ${GREEN}${BOLD}🎉 所有测试通过！${NC}"
elif [[ $QEMU_EXIT -eq 1 ]]; then
    echo -e "  ${RED}${BOLD}❌ 有测试失败，请检查日志${NC}"
else
    echo -e "  ${RED}${BOLD}⚠️  QEMU 运行异常（退出码 ${QEMU_EXIT}）${NC}"
fi
echo ""

exit $QEMU_EXIT
