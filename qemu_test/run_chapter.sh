#!/usr/bin/env bash
# 编译（已有 .ko 时跳过编译）并在 QEMU 客户机里只跑一章，打印命令和回显。
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "用法: bash qemu_test/run_chapter.sh <章节> [run_qemu.sh 的选项]" >&2
    echo "  章节例子: ch01  ch21  learn01  hardcode  kernel01  kernel02  kernel03  all" >&2
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

# 某个模块目录里的 .c/.h/Makefile 比该目录最旧的 .ko 新，才重新编译。
# 只含用户态程序、还没有 Makefile 的目录不参与这个判断。
skip=()
if [[ -f ch01_kernel_basics/kfifo_demo_static.ko && -f learn/07_debug/virt_debug.ko ]]; then
    need=""
    # 只看顶层 Makefile 真正会编译的目录。ch07_pci_bus 这类旧目录不在 CHAPTERS 里，没有 .ko 是正常的。
    chapters=$(sed -n '/^CHAPTERS :=/,/^$/p' Makefile | grep -oE '[A-Za-z0-9_./]+' | grep -v '^CHAPTERS$')
    for dir in ${chapters}; do
        [[ -f "${dir}/Makefile" ]] || continue
        newest_ko="$(find "${dir}" -maxdepth 1 -name '*.ko' -printf '%T@ %p\n' | sort -n | tail -n 1)"
        if [[ -z "${newest_ko}" ]]; then
            need="${dir}（没有 .ko）"
            break
        fi
        newest_src="$(find "${dir}" -maxdepth 1 \( -name '*.c' -o -name '*.h' -o -name Makefile \) -not -name '*.mod.c' -printf '%T@ %p\n' | sort -n | tail -n 1)"
        # 同一目录里有多个模块时，比“最新的源码”和“最新的 .ko”。
        # 旧模块的 .ko 可能早于后改的另一个 .c，那不代表这个目录需要整库重编。
        if [[ -n "${newest_src}" && "${newest_src%% *}" > "${newest_ko%% *}" ]]; then
            need="${newest_src#* }"
            break
        fi
    done
    if [[ -z "${need}" ]]; then
        skip=(--skip-build)
    else
        echo "源码比同目录的 .ko 新（${need}），重新编译"
    fi
fi

bash "${SCRIPT_DIR}/build_rootfs.sh" "${skip[@]}"
exec bash "${SCRIPT_DIR}/run_qemu.sh" --chapter "${CHAPTER}" "$@"
