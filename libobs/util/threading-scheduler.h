/* SPDX-License-Identifier: MIT */
#pragma once

#include "c99defs.h"

#ifdef __cplusplus
extern "C" {
#endif

enum os_thread_role {
	OS_THREAD_ROLE_AUDIO,
	OS_THREAD_ROLE_GRAPHICS,
	OS_THREAD_ROLE_VIDEO_IO,
	OS_THREAD_ROLE_GPU_ENCODE,
	OS_THREAD_ROLE_NETWORK,
	OS_THREAD_ROLE_BACKGROUND,
	OS_THREAD_ROLE_COUNT,
};

struct os_thread_scheduler;

/* Call once at thread entry, and end on the SAME thread before returning.
 * NULL is a valid inactive/fallback handle. A non-NULL handle can also retain
 * cleanup after incomplete rollback; it is not a success indicator.
 * Only the calling thread is changed.
 * Windows reads CORNOBS_SCHED once: unset/auto enables policy, off disables it.
 * Other platforms retain their existing media thread priority behavior. */
EXPORT struct os_thread_scheduler *os_thread_scheduler_begin(enum os_thread_role role);
EXPORT void os_thread_scheduler_end(struct os_thread_scheduler *state);

#ifdef __cplusplus
}
#endif
