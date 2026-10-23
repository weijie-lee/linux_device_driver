/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * virt_upath.h — /dev/virt_upath 的 ioctl 编号和参数，驱动和用户态程序共用。
 *
 * struct upath_info 在 i386 和 x86_64 上大小都是 16 字节、字段偏移相同，
 * _IOR 编码进去的大小因此一致，32 位程序可以直接走 compat_ptr_ioctl。
 */
#ifndef VIRT_UPATH_H
#define VIRT_UPATH_H

#include <linux/ioctl.h>
#include <linux/types.h>

struct upath_info {
	__u32 value;
	__u32 calls;
	__u64 stamp_ns;
};

#define UPATH_GET	_IOR('u', 1, struct upath_info)
#define UPATH_SET	_IOW('u', 2, __u32)

#endif
