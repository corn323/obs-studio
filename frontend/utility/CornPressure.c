/* SPDX-License-Identifier: MIT */
#include "CornPressure.h"
#include <math.h>
#include <string.h>

bool corn_adaptive_enabled(const char *value)
{
	/* Opt in until hardware A/B acceptance. Unknown values fail open. */
	return value && (!strcmp(value, "on") || !strcmp(value, "balanced"));
}

enum corn_pressure corn_pressure_update(struct corn_pressure_policy *p,
				       struct corn_pressure_sample s, uint64_t now)
{
	s.memory_valid = s.memory_valid && isfinite(s.memory_ratio) && s.memory_ratio >= 0;
	s.render_valid = s.render_valid && isfinite(s.render_ratio) && s.render_ratio >= 0;
	if (!s.memory_valid && !s.render_valid) {
		*p = (struct corn_pressure_policy){0};
		return CORN_NORMAL;
	}
	const double weight = p->initialized ? 0.25 : 1.0;
	p->memory_ema = s.memory_valid ? p->memory_ema + weight * (s.memory_ratio - p->memory_ema) : 0;
	p->render_ema = s.render_valid ? p->render_ema + weight * (s.render_ratio - p->render_ema) : 0;
	p->initialized = true;
	double m = p->memory_ema, r = p->render_ema;
	double lag = s.render_valid && isfinite(s.lag_ratio) ? s.lag_ratio : 0;
	enum corn_pressure target = CORN_NORMAL;
	if (m >= 0.95 || r >= 0.95 || lag >= 0.05)
		target = CORN_CRITICAL;
	else if (m >= 0.90 || r >= 0.80 || lag >= 0.02)
		target = CORN_HIGH;
	else if (m >= 0.80 || r >= 0.65 || lag > 0)
		target = CORN_ELEVATED;
	if (target > p->state) {
		/* Escalate promptly after the EMA; recovery has the long cooldown. */
		p->state = target;
		p->changed_ms = now;
		p->recovering = false;
	} else if (target < p->state) {
		static const double memory_exit[] = {0, 0.75, 0.85, 0.90};
		static const double render_exit[] = {0, 0.55, 0.70, 0.85};
		bool clear = m < memory_exit[p->state] && r < render_exit[p->state] && lag <= 0;
		if (!clear) {
			p->recovering = false;
		} else if (!p->recovering) {
			p->recovering = true;
			p->recovery_ms = now;
		} else if (now >= p->recovery_ms && now - p->recovery_ms >= 5000 &&
			   now >= p->changed_ms && now - p->changed_ms >= 2000) {
			p->state--;
			p->changed_ms = now;
			p->recovering = false;
		}
	} else {
		p->recovering = false;
	}
	return p->state;
}

unsigned corn_preview_fps(enum corn_pressure state, bool enabled)
{
	static const unsigned fps[] = {0, 30, 15, 8};
	return enabled && state >= CORN_NORMAL && state <= CORN_CRITICAL ? fps[state] : 0;
}

unsigned corn_meter_interval(enum corn_pressure state, bool enabled)
{
	return enabled && state >= CORN_HIGH ? 67 : 33;
}
