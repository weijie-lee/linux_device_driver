#!/usr/bin/env bash
# 编译（已有 .ko 时跳过编译）并在 QEMU 客户机里只跑一章，打印命令和回显。
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "用法: bash qemu_test/run_chapter.sh <章节> [run_qemu.sh 的选项]" >&2
    echo "  章节例子: ch01  ch21  learn01  hardcode  kernel01  kernel02  all" >&2
    exit 2
fi

CHAPTER="$1"
shift

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${REPO_DIR}"

if [[ "${CHAPTER}" == "ch00" || "${CHAPTER}" == "ch00_framework" ]]; then
    echo "======== ch00_framework ========"
    echo "+ test -f ch00_framework/driver_template.c -a -f ch00_framework/README.md"
    test -f ch00_framework/driver_template.c -a -f ch00_framework/README.md
    echo "files ok"
    echo "+ grep -n 'alloc_chrdev_region\\|cdev_init\\|cdev_add\\|class_create\\|device_create' ch00_framework/driver_template.c"
    grep -n 'alloc_chrdev_region\|cdev_init\|cdev_add\|class_create\|device_create' ch00_framework/driver_template.c
    echo "[PASS] ch00_framework"
    echo "本章没有 .ko。成功是模板里能看到这五步注册，并且文件都在镜像里。"
    exit 0
fi

# .ko 都在，并且没有源码比最旧的 .ko 更新时才跳过编译。
# 挂载本机目录后改了 .c，要重新编译，否则打进 initramfs 的是旧模块。
skip=()
if [[ -f ch01_kernel_basics/kfifo_demo_static.ko && -f learn/07_debug/virt_debug.ko ]]; then
    oldest="$(find ch* learn hardcode kernel -name '*.ko' -printf '%T@ %p\n' | sort -n | head -n 1 | cut -d' ' -f2-)"
    newer="$(find ch* learn hardcode kernel -not -path '*/tests/*' \( -name '*.c' -o -name '*.h' -o -name Makefile \) -not -name '*.mod.c' -newer "${oldest}" -print -quit)"
    if [[ -z "${newer}" ]]; then
        skip=(--skip-build)
    else
        echo "源码比 .ko 新（${newer}），重新编译"
    fi
fi

bash "${SCRIPT_DIR}/build_rootfs.sh" "${skip[@]}"
exec bash "${SCRIPT_DIR}/run_qemu.sh" --chapter "${CHAPTER}" "$@"
