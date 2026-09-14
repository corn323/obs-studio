/* SPDX-License-Identifier: MIT */
#pragma once
#include "CornPressure.h"
#include <QObject>
#include <QElapsedTimer>
#include <functional>
#include <obs.h>

class CornAdaptive : public QObject {
	struct corn_pressure_policy policy = {};
	QElapsedTimer clock;
	std::function<obs_display_t *()> display;
	std::function<bool()> streaming;
	uint32_t previousFrames = 0, previousLag = 0;
	qint64 lastLog = -30000;
	bool haveCounters = false;
	enum corn_mode mode = CORN_COMPATIBILITY;
	uint32_t submissionPeakUs = 0;
	uint64_t previousPreviewFrames = 0;
	qint64 previousPreviewMs = 0;
	void sample();

public:
	CornAdaptive(QObject *parent, std::function<obs_display_t *()> preview, std::function<bool()> streaming);
	static unsigned MeterInterval();
	static bool ShedThumbnails();
};
