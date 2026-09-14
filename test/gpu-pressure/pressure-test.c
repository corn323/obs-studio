/* SPDX-License-Identifier: MIT */
#include "../../frontend/utility/CornPressure.h"
#include "../../libobs/util/display-timing.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); exit(1); } } while (0)

int main(void)
{
	CHECK(corn_thumbnail_interval(100, false) == 100);
	CHECK(corn_thumbnail_interval(100, true) == 500);
	CHECK(corn_thumbnail_interval(500, true) == 500);
	CHECK(corn_thumbnail_interval(5000, true) == 5000);
	CHECK(corn_mode_parse(NULL) == CORN_COMPATIBILITY);
	CHECK(corn_mode_parse("typo") == CORN_COMPATIBILITY);
	CHECK(corn_mode_parse("balanced") == CORN_BALANCED);
	CHECK(corn_mode_parse("gaming") == CORN_GAMING);
	CHECK(corn_mode_override(CORN_GAMING, NULL) == CORN_GAMING);
	CHECK(corn_mode_override(CORN_GAMING, "off") == CORN_COMPATIBILITY);
	CHECK(corn_mode_override(CORN_GAMING, "typo") == CORN_COMPATIBILITY);
	CHECK(corn_mode_override(CORN_COMPATIBILITY, "on") == CORN_BALANCED);
	CHECK(corn_mode_override(CORN_COMPATIBILITY, "gaming") == CORN_GAMING);
	CHECK(!strcmp(corn_scheduler_mode(CORN_COMPATIBILITY), "off"));
	CHECK(!strcmp(corn_scheduler_mode(CORN_BALANCED), "mmcss"));
	CHECK(!strcmp(corn_scheduler_mode(CORN_GAMING), "mmcss"));
	const unsigned caps[] = {15, 10, 8, 5};
	for (int state = CORN_NORMAL; state <= CORN_CRITICAL; state++) {
		enum corn_pressure s = (enum corn_pressure)state;
		CHECK(corn_mode_preview_fps(s, CORN_COMPATIBILITY, true) == 0);
		CHECK(corn_mode_meter_interval(s, CORN_COMPATIBILITY, true) == 16);
		CHECK(corn_mode_preview_fps(s, CORN_GAMING, true) == caps[state]);
		CHECK(corn_mode_meter_interval(s, CORN_GAMING, true) == (state >= CORN_HIGH ? 200U : 100U));
		CHECK(corn_mode_preview_fps(s, CORN_GAMING, false) == 0);
		CHECK(corn_mode_meter_interval(s, CORN_GAMING, false) == 16);
		CHECK(corn_mode_preview_fps(s, CORN_BALANCED, true) == corn_preview_fps(s, true));
		/* A 60 Hz compositor still receives all 600 ticks; only display draws change. */
		uint64_t next = 0;
		unsigned draws = 0;
		for (unsigned tick = 0; tick < 600; tick++)
			draws += obs_display_frame_due(1000000000ULL + tick * 16666667ULL,
						      1000000000ULL / caps[state], &next, false);
		CHECK(draws >= caps[state] * 10 - 1 && draws <= caps[state] * 10 + 1);
	}
	/* Ten seconds of 60 Hz compositor ticks with +/-100 us timing jitter.
	 * The display policy must retain its average cap and never alter ticks. */
	uint64_t next = 0;
	unsigned draws = 0;
	for (unsigned i = 0; i < 600; i++) {
		uint64_t now = 1000000000ULL + i * 16666667ULL + (i % 2 ? 100000 : 0);
		draws += obs_display_frame_due(now, 33333333, &next, false);
	}
	CHECK(draws >= 299 && draws <= 301);
	CHECK(obs_display_frame_due(12000000000ULL, 33333333, &next, true));
	CHECK(!obs_display_frame_due(12000000001ULL, 33333333, &next, false));
	CHECK(obs_display_frame_due(12000000001ULL, 0, &next, false));
	CHECK(next == 0);
	CHECK(!corn_adaptive_enabled(NULL));
	CHECK(!corn_adaptive_enabled("off"));
	CHECK(!corn_adaptive_enabled("typo"));
	CHECK(corn_adaptive_enabled("balanced"));
	CHECK(corn_adaptive_enabled("on"));
	struct corn_pressure_policy p = {0};
	struct corn_pressure_sample s = {.memory_ratio = .8, .memory_valid = true};
	CHECK(corn_pressure_update(&p, s, 0) == CORN_ELEVATED);
	p = (struct corn_pressure_policy){0};
	s.memory_ratio = .9;
	CHECK(corn_pressure_update(&p, s, 0) == CORN_HIGH);
	p = (struct corn_pressure_policy){0};
	s.memory_ratio = .95;
	CHECK(corn_pressure_update(&p, s, 0) == CORN_CRITICAL);
	/* Boundary chatter must not cause recovery. */
	for (uint64_t t = 250; t <= 10000; t += 250) {
		s.memory_ratio = (t % 500) ? .94 : .95;
		CHECK(corn_pressure_update(&p, s, t) == CORN_CRITICAL);
	}
	s.memory_ratio = .1;
	for (uint64_t t = 10250; t <= 14000; t += 250) {
		CHECK(corn_pressure_update(&p, s, t) == CORN_CRITICAL);
	}
	for (uint64_t t = 14250; t <= 30000; t += 250) {
		corn_pressure_update(&p, s, t);
	}
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
	p = (struct corn_pressure_policy){.state = CORN_HIGH, .initialized = true,
		.recovering = true, .last_sample_ms = 250, .recovery_ms = 250};
	s = (struct corn_pressure_sample){.render_valid = true, .render_ratio = .1};
	CHECK(corn_pressure_update(&p, s, 10000) == CORN_HIGH);
	CHECK(p.recovery_ms == 10000);
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
