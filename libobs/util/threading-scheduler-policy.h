/* SPDX-License-Identifier: MIT */
#pragma once

#include "threading-scheduler.h"
#include <stddef.h>
#include <stdint.h>

/* Internal, platform-independent inputs. All topology indices are group-relative. */
struct scheduler_cpu {
	uint32_t id;
	uint16_t group;
	uint8_t logical;
	uint8_t core;
	uint8_t llc;
	uint8_t efficiency;
	bool available;
};

struct scheduler_placement {
	bool valid;
	bool hybrid;
	bool local;
	uint8_t efficiency;
	uint16_t group;
	uint8_t llc;
	size_t domains;
	size_t count;
	const char *reason;
};

enum scheduler_mmcss { SCHEDULER_MMCSS_NONE, SCHEDULER_MMCSS_AUDIO, SCHEDULER_MMCSS_PLAYBACK };
struct scheduler_role_policy {
	const char *name;
	enum scheduler_mmcss mmcss;
	bool critical;
	/* Relative MMCSS priority: -1 (low) or 0 (normal), never high/critical. */
	int mmcss_priority;
};

bool scheduler_auto_mode(const char *mode);
struct scheduler_role_policy scheduler_policy(enum os_thread_role role);
struct scheduler_placement scheduler_select(const struct scheduler_cpu *cpus, size_t count, uint32_t seed);
bool scheduler_cpu_selected(const struct scheduler_cpu *cpu, const struct scheduler_placement *placement);
