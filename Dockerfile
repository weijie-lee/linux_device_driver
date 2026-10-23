# 实验镜像：Ubuntu 22.04 的 GCC 11 与 5.15.0-173-generic 是同一套编译器系列。
# 模块在镜像里编译，在 QEMU 客户机里加载。容器自己的内核不是 5.15。
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    KERNEL_VER=5.15.0-173-generic \
    KERNEL_DIR=/lib/modules/5.15.0-173-generic/build

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        sudo \
        file \
        bc \
        flex \
        bison \
        libelf-dev \
        libssl-dev \
        python3 \
        qemu-system-x86 \
        busybox-static \
        cpio \
        gzip \
        xz-utils \
        curl \
        git \
        gdb \
        sparse \
        gcc-multilib \
        device-tree-compiler \
        kmod \
    && printf '%s\n' \
        'do_symlinks = yes' \
        'do_bootloader = no' \
        'do_initrd = no' \
        'link_in_boot = yes' \
        > /etc/kernel-img.conf \
    && apt-get install -y --no-install-recommends \
        "linux-headers-${KERNEL_VER}" \
        "linux-image-${KERNEL_VER}" \
        "linux-modules-${KERNEL_VER}" \
        "linux-modules-extra-${KERNEL_VER}" \
    && rm -rf /var/lib/apt/lists/* \
    && test -d "${KERNEL_DIR}" \
    && test -f "/boot/vmlinuz-${KERNEL_VER}"

WORKDIR /linux_device_driver

COPY docker/entrypoint.sh /usr/local/bin/ldd-lab
COPY . .

RUN chmod 755 /usr/local/bin/ldd-lab

# 模块在构建镜像时就编好，docker run ... chapter 不必每次重新编译。
RUN make all > /tmp/make.log 2>&1 \
    || { tail -n 80 /tmp/make.log; exit 1; } \
    && rm -f /tmp/make.log

ENTRYPOINT ["/usr/local/bin/ldd-lab"]
CMD ["shell"]
