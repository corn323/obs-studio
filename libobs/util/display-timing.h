/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Preserve phase across compositor frames: resetting the deadline to now on
 * every draw would turn small 60 Hz jitter into a persistent 20 Hz cap. */
static inline bool obs_display_frame_due(uint64_t now, uint64_t interval, uint64_t *next, bool force)
{
	if (!interval) {
		*next = 0;
		return true;
	}
	if (!force && *next && now < *next) {
		return false;
	}
	if (force || !*next || now > *next + interval) {
		*next = now + interval;
	} else {
		*next += interval;
	}
	return true;
}
