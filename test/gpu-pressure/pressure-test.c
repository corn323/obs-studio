/* SPDX-License-Identifier: MIT */
#include "../../frontend/utility/CornPressure.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

int main(void)
{
	CHECK(!corn_adaptive_enabled(NULL));
	CHECK(!corn_adaptive_enabled("off"));
	CHECK(!corn_adaptive_enabled("typo"));
	CHECK(corn_adaptive_enabled("balanced"));
	CHECK(corn_adaptive_enabled("on"));
	struct corn_pressure_policy p = {0};
	struct corn_pressure_sample s = {.memory_ratio = .8, .memory_valid = true};
	CHECK(corn_pressure_update(&p, s, 0) == CORN_ELEVATED);
	p = (struct corn_pressure_policy){0}; s.memory_ratio = .9;
	CHECK(corn_pressure_update(&p, s, 0) == CORN_HIGH);
	p = (struct corn_pressure_policy){0}; s.memory_ratio = .95;
	CHECK(corn_pressure_update(&p, s, 0) == CORN_CRITICAL);
	/* Boundary chatter must not cause recovery. */
	for (uint64_t t = 250; t <= 10000; t += 250) {
		s.memory_ratio = (t % 500) ? .94 : .95;
		CHECK(corn_pressure_update(&p, s, t) == CORN_CRITICAL);
	}
	s.memory_ratio = .1;
	for (uint64_t t = 10250; t <= 14000; t += 250)
		CHECK(corn_pressure_update(&p, s, t) == CORN_CRITICAL);
	for (uint64_t t = 14250; t <= 30000; t += 250)
		corn_pressure_update(&p, s, t);
	CHECK(p.state == CORN_NORMAL);
	s = (struct corn_pressure_sample){.render_valid = true, .render_ratio = .8};
	p = (struct corn_pressure_policy){0};
	CHECK(corn_pressure_update(&p, s, 0) == CORN_HIGH);
	s.lag_ratio = .05;
	CHECK(corn_pressure_update(&p, s, 250) == CORN_CRITICAL);
	/* Missing adapter does not disable valid deadline evidence. */
	CHECK(!s.memory_valid && p.state == CORN_CRITICAL);
	s.render_ratio = NAN;
	CHECK(corn_pressure_update(&p, s, 500) == CORN_NORMAL);
	CHECK(!p.initialized);
	for (int state = CORN_NORMAL; state <= CORN_CRITICAL; state++) {
		CHECK(corn_preview_fps((enum corn_pressure)state, false) == 0);
		CHECK(corn_meter_interval((enum corn_pressure)state, false) == 33);
	}
	CHECK(corn_preview_fps(CORN_NORMAL, true) == 0);
	CHECK(corn_preview_fps(CORN_ELEVATED, true) == 30);
	CHECK(corn_preview_fps(CORN_HIGH, true) == 15);
	CHECK(corn_preview_fps(CORN_CRITICAL, true) == 8);
	CHECK(corn_meter_interval(CORN_HIGH, true) == 67);
	puts("GPU pressure policy tests passed");
	return 0;
}
