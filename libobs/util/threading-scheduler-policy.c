/* SPDX-License-Identifier: MIT */
#include "threading-scheduler-policy.h"
#include <string.h>

bool scheduler_auto_mode(const char *mode)
{
	return !mode || strcmp(mode, "auto") == 0;
}

struct scheduler_role_policy scheduler_policy(enum os_thread_role role)
{
	switch (role) {
	case OS_THREAD_ROLE_AUDIO:
		return (struct scheduler_role_policy){"AUDIO", SCHEDULER_MMCSS_AUDIO, true, 0};
	case OS_THREAD_ROLE_GRAPHICS:
		return (struct scheduler_role_policy){"GRAPHICS", SCHEDULER_MMCSS_PLAYBACK, true, -1};
	case OS_THREAD_ROLE_VIDEO_IO:
		return (struct scheduler_role_policy){"VIDEO_IO", SCHEDULER_MMCSS_PLAYBACK, true, -1};
	case OS_THREAD_ROLE_GPU_ENCODE:
		return (struct scheduler_role_policy){"GPU_ENCODE", SCHEDULER_MMCSS_PLAYBACK, true, -1};
	case OS_THREAD_ROLE_NETWORK:
		return (struct scheduler_role_policy){"NETWORK", SCHEDULER_MMCSS_NONE, false, 0};
	case OS_THREAD_ROLE_BACKGROUND:
		return (struct scheduler_role_policy){"BACKGROUND", SCHEDULER_MMCSS_NONE, false, 0};
	default:
		return (struct scheduler_role_policy){"UNKNOWN", SCHEDULER_MMCSS_NONE, false, 0};
	}
}

bool scheduler_cpu_selected(const struct scheduler_cpu *cpu, const struct scheduler_placement *p)
{
	return p->valid && cpu->available && (!p->hybrid || cpu->efficiency == p->efficiency) &&
	       (!p->local || (cpu->group == p->group && cpu->llc == p->llc));
}

static size_t core_count(const struct scheduler_cpu *cpus, size_t count, const struct scheduler_placement *p)
{
	size_t cores = 0;
	for (size_t i = 0; i < count; i++) {
		if (!scheduler_cpu_selected(&cpus[i], p))
			continue;
		bool seen = false;
		for (size_t j = 0; j < i; j++)
			if (scheduler_cpu_selected(&cpus[j], p) && cpus[j].group == cpus[i].group &&
			    cpus[j].core == cpus[i].core)
				seen = true;
		cores += !seen;
	}
	return cores;
}

static uint32_t domain_score(uint32_t key, uint32_t seed)
{
	/* Rendezvous selection, independent of enumeration order and LP counts.
	 * Seed once per process; this is distribution, not a load estimate. */
	uint32_t x = key ^ seed;
	x ^= x >> 16;
	x *= UINT32_C(0x7feb352d);
	x ^= x >> 15;
	x *= UINT32_C(0x846ca68b);
	return x ^ (x >> 16);
}

struct scheduler_placement scheduler_select(const struct scheduler_cpu *cpus, size_t count, uint32_t seed)
{
	struct scheduler_placement p = {.reason = "unknown/empty topology"};
	if (!cpus || !count)
		return p;
	uint8_t min_eff = UINT8_MAX;
	for (size_t i = 0; i < count; i++) {
		if (cpus[i].logical >= 64) {
			p.reason = "invalid processor index";
			return p;
		}
		bool domain_seen = false;
		for (size_t j = 0; j < i; j++) {
			if (cpus[j].id == cpus[i].id ||
			    (cpus[j].group == cpus[i].group && cpus[j].logical == cpus[i].logical)) {
				p.reason = "duplicate CPU topology record";
				return p;
			}
			if (cpus[j].group == cpus[i].group && cpus[j].llc == cpus[i].llc)
				domain_seen = true;
			if (cpus[j].group == cpus[i].group && cpus[j].core == cpus[i].core &&
			    (cpus[j].llc != cpus[i].llc || cpus[j].efficiency != cpus[i].efficiency)) {
				p.reason = "inconsistent core topology";
				return p;
			}
		}
		p.domains += !domain_seen;
		if (cpus[i].efficiency < min_eff)
			min_eff = cpus[i].efficiency;
		if (cpus[i].efficiency > p.efficiency)
			p.efficiency = cpus[i].efficiency;
	}
	p.hybrid = min_eff != p.efficiency;
	p.valid = true;
	if (core_count(cpus, count, &p) < 2) {
		p.valid = false;
		p.reason = "fewer than two available preferred physical cores";
		return p;
	}

	if (p.domains > 1) {
		bool found = false;
		uint32_t best_score = 0, best_key = 0;
		for (size_t i = 0; i < count; i++) {
			bool seen = false;
			for (size_t j = 0; j < i; j++)
				if (cpus[j].group == cpus[i].group && cpus[j].llc == cpus[i].llc)
					seen = true;
			if (seen)
				continue;
			struct scheduler_placement candidate = p;
			candidate.local = true;
			candidate.group = cpus[i].group;
			candidate.llc = cpus[i].llc;
			if (core_count(cpus, count, &candidate) < 2)
				continue;
			uint32_t key = ((uint32_t)candidate.group << 8) | candidate.llc;
			uint32_t score = domain_score(key, seed);
			if (!found || score > best_score || (score == best_score && key < best_key)) {
				found = true;
				best_score = score;
				best_key = key;
				p.group = candidate.group;
				p.llc = candidate.llc;
			}
		}
		p.local = found;
	}
	for (size_t i = 0; i < count; i++)
		p.count += scheduler_cpu_selected(&cpus[i], &p);
	p.reason = p.local    ? "stable process-local LLC assignment"
		   : p.hybrid ? "preferred efficiency class; no eligible LLC restriction"
			      : "Windows default placement";
	return p;
}
