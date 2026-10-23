#!/bin/bash
# 实验镜像入口。模块编译用镜像里的 5.15 头文件，加载发生在 QEMU 客户机里。
set -euo pipefail

cd /linux_device_driver

usage() {
    cat <<'EOF'
Linux 驱动实验镜像（编译与 QEMU 客户机均为 5.15.0-173-generic）

  docker run --rm -it ldd-lab
      进入 /linux_device_driver，可 make、改代码、跑单章。

  docker run --rm ldd-lab test
      编译全部章节并在 QEMU 里跑 init.sh。

  docker run --rm ldd-lab chapter ch21
      只跑某一章，客户机回显命令和结果。章节名如 ch01、ch21、learn01、hardcode、kernel01 到 kernel09、all。

  docker run --rm ldd-lab build
      只编译并打包 initramfs。

  docker run --rm -it ldd-lab qemu --interactive
      用已打好的 initramfs 进入客户机串口。退出：Ctrl-A 然后 X。

本机仓库挂进容器，改动留在本机：

  docker run --rm -it -v "$PWD":/linux_device_driver ldd-lab

有 KVM 时加上 --device /dev/kvm，并把 --kvm 传给 test：

  docker run --rm --device /dev/kvm ldd-lab test --kvm

容器里的 uname -r 是宿主机内核。不要在容器里 insmod 这些 .ko。
EOF
}

cmd="${1:-shell}"
case "${cmd}" in
    shell)
        usage
        echo
        exec bash
        ;;
    test)
        shift
        exec bash qemu_test/qemu_test_all.sh "$@"
        ;;
    build)
        shift
        exec bash qemu_test/build_rootfs.sh "$@"
        ;;
    chapter)
        shift
        exec bash qemu_test/run_chapter.sh "$@"
        ;;
    qemu)
        shift
        exec bash qemu_test/run_qemu.sh "$@"
        ;;
    help|-h|--help)
        usage
        ;;
    *)
        exec "$@"
        ;;
esac
