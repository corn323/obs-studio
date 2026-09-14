/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum corn_pressure { CORN_NORMAL, CORN_ELEVATED, CORN_HIGH, CORN_CRITICAL };
enum corn_mode { CORN_COMPATIBILITY, CORN_BALANCED, CORN_GAMING };
enum corn_mode corn_mode_parse(const char *value);
enum corn_mode corn_mode_override(enum corn_mode configured, const char *override);
const char *corn_scheduler_mode(enum corn_mode mode);
unsigned corn_mode_preview_fps(enum corn_pressure state, enum corn_mode mode, bool streaming);
unsigned corn_mode_meter_interval(enum corn_pressure state, enum corn_mode mode, bool streaming);
unsigned corn_thumbnail_interval(unsigned requested_ms, bool shedding);
struct corn_pressure_sample {
	double memory_ratio;
	double render_ratio;
	double lag_ratio;
	bool memory_valid;
	bool render_valid;
};
struct corn_pressure_policy {
	enum corn_pressure state;
	double memory_ema, render_ema;
	uint64_t changed_ms, recovery_ms;
	uint64_t last_sample_ms;
	bool initialized, recovering;
};
bool corn_adaptive_enabled(const char *value);
enum corn_pressure corn_pressure_update(struct corn_pressure_policy *policy, struct corn_pressure_sample sample,
					uint64_t now_ms);
unsigned corn_preview_fps(enum corn_pressure state, bool enabled);
unsigned corn_meter_interval(enum corn_pressure state, bool enabled);
#ifdef __cplusplus
}
#endif
