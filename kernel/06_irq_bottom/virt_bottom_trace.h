/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM virt_bottom

#if !defined(_VIRT_BOTTOM_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _VIRT_BOTTOM_TRACE_H

#include <linux/tracepoint.h>

TRACE_EVENT(virt_bottom_step,
	TP_PROTO(const char *step, unsigned int n),
	TP_ARGS(step, n),
	TP_STRUCT__entry(
		__string(step, step)
		__field(unsigned int, n)
	),
	TP_fast_assign(
		__assign_str(step, step);
		__entry->n = n;
	),
	TP_printk("step=%s n=%u", __get_str(step), __entry->n)
);

#endif /* _VIRT_BOTTOM_TRACE_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE virt_bottom_trace
#include <trace/define_trace.h>
