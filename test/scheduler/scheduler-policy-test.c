/* SPDX-License-Identifier: MIT */
#include "../../libobs/util/threading-scheduler-policy.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) \
	do { \
		if (!(condition)) { \
			fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
			exit(1); \
		} \
	} while (0)

static struct scheduler_cpu cpu(uint32_t id, uint16_t group, uint8_t logical, uint8_t core, uint8_t llc,
				uint8_t efficiency)
{
	return (struct scheduler_cpu){id, group, logical, core, llc, efficiency, true};
}

static void test_hybrid(void)
{
	struct scheduler_cpu cpus[] = {cpu(31, 0, 0, 0, 0, 8), cpu(33, 0, 1, 0, 0, 8), cpu(35, 0, 2, 1, 0, 8),
				       cpu(37, 0, 3, 1, 0, 8), cpu(39, 0, 4, 2, 1, 0), cpu(41, 0, 5, 3, 1, 0)};
	struct scheduler_placement p = scheduler_select(cpus, 6, 123);
	CHECK(p.valid && p.hybrid && p.efficiency == 8 && p.count == 4);
	CHECK(p.local && p.llc == 0 && p.domains == 2);
	for (size_t i = 0; i < 6; i++)
		CHECK(scheduler_cpu_selected(&cpus[i], &p) == (i < 4));
	/* Hybrid single LLC still prefers performance cores, but never CCD-pins. */
	cpus[4].llc = cpus[5].llc = 0;
	p = scheduler_select(cpus, 6, 123);
	CHECK(p.valid && p.hybrid && !p.local && p.domains == 1 && p.count == 4);
	/* All preferred cores reserved by another process: no narrow E-core policy. */
	for (size_t i = 0; i < 4; i++)
		cpus[i].available = false;
	p = scheduler_select(cpus, 6, 123);
	CHECK(!p.valid);
}

static void test_multi_llc(void)
{
	/* Unequal domain sizes and non-contiguous CPU set IDs. Neither LP count
	 * nor input order is permitted to decide the selected domain. */
	struct scheduler_cpu cpus[] = {cpu(10, 0, 0, 0, 0, 0), cpu(20, 0, 1, 1, 0, 0), cpu(30, 0, 2, 2, 1, 0),
				       cpu(40, 0, 3, 3, 1, 0), cpu(50, 0, 4, 4, 1, 0), cpu(60, 0, 5, 5, 1, 0)};
	struct scheduler_cpu reversed[6];
	for (size_t i = 0; i < 6; i++)
		reversed[i] = cpus[5 - i];
	bool selected_small = false, selected_large = false;
	for (uint32_t seed = 0; seed < 100; seed++) {
		struct scheduler_placement p = scheduler_select(cpus, 6, seed);
		struct scheduler_placement q = scheduler_select(reversed, 6, seed);
		CHECK(p.valid && p.domains == 2 && p.local && !p.hybrid);
		CHECK(p.group == q.group && p.llc == q.llc && p.count == q.count);
		CHECK(p.count == (p.llc == 0 ? 2 : 4));
		selected_small |= p.llc == 0;
		selected_large |= p.llc == 1;
		/* All frame roles use the same immutable process placement. */
		for (int role = OS_THREAD_ROLE_AUDIO; role <= OS_THREAD_ROLE_GPU_ENCODE; role++)
			CHECK(scheduler_policy((enum os_thread_role)role).critical);
	}
	CHECK(selected_small && selected_large);
}

static void test_single_llc(void)
{
	struct scheduler_cpu cpus[] = {cpu(101, 0, 0, 0, 0, 0), cpu(102, 0, 1, 0, 0, 0), cpu(201, 0, 2, 1, 0, 0),
				       cpu(202, 0, 3, 1, 0, 0)};
	struct scheduler_placement p = scheduler_select(cpus, 4, 0);
	CHECK(p.valid && p.domains == 1 && !p.local && !p.hybrid && p.count == 4);
	/* Two SMT siblings are one physical core, not a suitable critical pool. */
	p = scheduler_select(cpus, 2, 0);
	CHECK(!p.valid);
}

static void test_groups_and_small_domains(void)
{
	struct scheduler_cpu cpus[] = {cpu(11, 0, 0, 0, 0, 0), cpu(12, 0, 1, 1, 0, 0), cpu(21, 1, 0, 0, 0, 0),
				       cpu(22, 1, 1, 1, 0, 0)};
	struct scheduler_placement p = scheduler_select(cpus, 4, 9);
	CHECK(p.valid && p.domains == 2 && p.local && p.count == 2);
	CHECK(scheduler_cpu_selected(&cpus[0], &p) != scheduler_cpu_selected(&cpus[2], &p));
	/* One core per LLC: preserve at least two cores and relax locality. */
	cpus[1].core = cpus[3].core = 0;
	p = scheduler_select(cpus, 4, 9);
	CHECK(p.valid && !p.local && p.count == 4);
	/* Available CPUs only; parked CPUs are deliberately not filtered by the adapter. */
	cpus[0].available = cpus[1].available = false;
	p = scheduler_select(cpus, 4, 9);
	CHECK(!p.valid);
}

static void test_unknown(void)
{
	CHECK(!scheduler_select(NULL, 0, 1).valid);
	CHECK(!scheduler_select(NULL, 4, 1).valid);
	struct scheduler_cpu cpus[] = {cpu(7, 0, 0, 0, 0, 0), cpu(8, 0, 1, 1, 0, 0)};
	cpus[1].id = 7;
	CHECK(!scheduler_select(cpus, 2, 1).valid);
	cpus[1] = cpu(8, 0, 0, 1, 0, 0);
	CHECK(!scheduler_select(cpus, 2, 1).valid);
	cpus[1] = cpu(8, 0, 64, 1, 0, 0);
	CHECK(!scheduler_select(cpus, 2, 1).valid);
	cpus[1] = cpu(8, 0, 1, 0, 1, 0);
	CHECK(!scheduler_select(cpus, 2, 1).valid);
	cpus[1] = cpu(8, 0, 1, 0, 0, 8);
	CHECK(!scheduler_select(cpus, 2, 1).valid);
}

static void test_roles_and_modes(void)
{
	CHECK(scheduler_auto_mode(NULL));
	CHECK(scheduler_auto_mode("auto"));
	CHECK(!scheduler_auto_mode("off"));
	CHECK(!scheduler_auto_mode("ccd"));
	CHECK(!scheduler_auto_mode(""));
	CHECK(!scheduler_auto_mode("typo"));
	struct scheduler_role_policy audio = scheduler_policy(OS_THREAD_ROLE_AUDIO);
	CHECK(audio.mmcss == SCHEDULER_MMCSS_AUDIO && audio.mmcss_priority == 0);
	for (int role = OS_THREAD_ROLE_GRAPHICS; role <= OS_THREAD_ROLE_GPU_ENCODE; role++) {
		struct scheduler_role_policy p = scheduler_policy((enum os_thread_role)role);
		CHECK(p.critical && p.mmcss == SCHEDULER_MMCSS_PLAYBACK && p.mmcss_priority == -1);
	}
	for (int role = OS_THREAD_ROLE_NETWORK; role <= OS_THREAD_ROLE_COUNT; role++) {
		struct scheduler_role_policy p = scheduler_policy((enum os_thread_role)role);
		CHECK(!p.critical && p.mmcss == SCHEDULER_MMCSS_NONE);
	}
}

int main(void)
{
	test_hybrid();
	test_multi_llc();
	test_single_llc();
	test_groups_and_small_domains();
	test_unknown();
	test_roles_and_modes();
	puts("PASS: hybrid, multi-LLC stability/distribution, single-LLC, groups, insufficient/unknown topology, roles/modes");
	return 0;
}
