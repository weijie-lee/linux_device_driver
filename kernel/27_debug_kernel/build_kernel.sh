#!/usr/bin/env bash
# 下载并编译 kernel/27、kernel/28 用的调试内核（lockdep、DEBUG_ATOMIC_SLEEP、KASAN、kmemleak、KUnit）。
# 结果缓存在 qemu_test/output/debug-kernel，debug.config 或本脚本不变时直接复用。
set -euo pipefail

VER=5.15.223
SHA256=c1faba89652ec4af585fb96131fdb70d99e891c2b368cbf6d9c1e5fb7f156c23
URL="https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-${VER}.tar.xz"

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "${HERE}/../.." && pwd)"
CACHE="${LDD_DEBUG_DIR:-${REPO}/qemu_test/output/debug-kernel}"
SRC="${CACHE}/linux-${VER}"
STAMP="${SRC}/.ldd-stamp"
FRAG="${HERE}/debug.config"

if [[ "${1:-}" == "--print-src" ]]; then
    echo "${SRC}"
    exit 0
fi

want="$(cat "${FRAG}" "${BASH_SOURCE[0]}" | sha256sum | cut -d' ' -f1)"
if [[ -f "${STAMP}" && "$(cat "${STAMP}")" == "${want}" && -f "${SRC}/arch/x86/boot/bzImage" ]]; then
    echo "debug kernel ready: ${SRC}"
    exit 0
fi

fetch() {
    if command -v curl >/dev/null; then
        curl -fL --retry 4 -o "$2" "$1"
    else
        python3 -c 'import sys, urllib.request; urllib.request.urlretrieve(sys.argv[1], sys.argv[2])' "$1" "$2"
    fi
}

mkdir -p "${CACHE}"
tarball="${CACHE}/linux-${VER}.tar.xz"
if [[ ! -f "${tarball}" ]] || ! echo "${SHA256}  ${tarball}" | sha256sum -c --quiet - 2>/dev/null; then
    echo "+ fetch ${URL}"
    fetch "${URL}" "${tarball}.part"
    mv "${tarball}.part" "${tarball}"
fi
echo "+ sha256sum -c"
echo "${SHA256}  ${tarball}" | sha256sum -c -

rm -rf "${SRC}"
echo "+ tar -xf ${tarball##*/}"
tar -xf "${tarball}" -C "${CACHE}"

cd "${SRC}"
echo "+ make x86_64_defconfig"
make -s x86_64_defconfig
echo "+ scripts/kconfig/merge_config.sh -m .config debug.config"
scripts/kconfig/merge_config.sh -m .config "${FRAG}" >/dev/null
make -s olddefconfig

# merge_config.sh 在依赖不满足时会静默丢掉选项，这里逐项核对。
missing=0
while IFS= read -r line; do
    case "${line}" in
        CONFIG_*=*)
            grep -qx "${line}" .config || { echo "config missing: ${line}"; missing=1; }
            ;;
        "# CONFIG_"*" is not set")
            opt="${line#\# }"
            opt="${opt%% *}"
            if grep -q "^${opt}=" .config; then
                echo "config still set: ${opt}"
                missing=1
            fi
            ;;
    esac
done < "${FRAG}"
[[ ${missing} -eq 0 ]] || exit 1

echo "+ make -j$(nproc) bzImage modules scripts_gdb"
make -j"$(nproc)" bzImage modules scripts_gdb

# 编外部模块只需要头文件、脚本、Module.symvers 和 objtool；gdb 需要 vmlinux 和 scripts/gdb。
# 删掉目标文件，缓存小得多。
find . -type f \( -name '*.o' -o -name '*.a' -o -name '.*.cmd' -o -name '*.ko' -o -name '*.mod.c' -o -name '*.mod' \) -delete
rm -f vmlinux.o
echo "${want}" > "${STAMP}"
echo "debug kernel ready: ${SRC}"
