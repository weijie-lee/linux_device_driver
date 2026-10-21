/* SPDX-License-Identifier: GPL-2.0 */
#ifndef VIRT_UAPI_CMD_H
#define VIRT_UAPI_CMD_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
#else
#include <stdint.h>
#include <sys/ioctl.h>
typedef uint32_t __u32;
#endif

#define VIRT_UAPI_MAGIC		'V'
#define VIRT_UAPI_GET_LEN	_IOR(VIRT_UAPI_MAGIC, 1, __u32)
#define VIRT_UAPI_CLEAR		_IO(VIRT_UAPI_MAGIC, 2)

#endif
