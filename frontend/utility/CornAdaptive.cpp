/* SPDX-License-Identifier: MIT */
#include "CornAdaptive.hpp"
#include <OBSApp.hpp>
#include <QTimer>
#include <util/platform.h>
#include <utility>
#include <algorithm>
#ifdef _WIN32
#include <d3d11.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#endif

namespace {
unsigned meterInterval = 16; // GUI thread only, including VolumeMeter timer callbacks.
bool shedThumbnails = false; // GUI thread only; no source tick or output control.
struct MemorySample {
	uint64_t usage = 0, budget = 0, available = 0, reservation = 0;
	bool valid = false;
};

MemorySample ReadMemory()
{
	MemorySample result;
#ifdef _WIN32
	using Microsoft::WRL::ComPtr;
	/* Obtain the current OBS adapter every sample, so device rebuilds cannot
	 * leave a stale COM device in the controller. No per-frame query. */
	obs_enter_graphics();
	if (gs_get_device_type() == GS_DEVICE_DIRECT3D_11) {
		auto *device = static_cast<ID3D11Device *>(gs_get_device_obj());
		ComPtr<IDXGIDevice> dxgi;
		ComPtr<IDXGIAdapter> adapter;
		ComPtr<IDXGIAdapter3> adapter3;
		DXGI_QUERY_VIDEO_MEMORY_INFO info = {};
		if (device && SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dxgi))) &&
		    SUCCEEDED(dxgi->GetAdapter(&adapter)) && SUCCEEDED(adapter.As(&adapter3)) &&
		    SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info)) &&
		    info.Budget > 0) {
			result = {info.CurrentUsage, info.Budget, info.AvailableForReservation, info.CurrentReservation,
				  true};
		}
	}
	obs_leave_graphics();
#endif
	return result;
}
} // namespace

CornAdaptive::CornAdaptive(QObject *parent, std::function<obs_display_t *()> preview, std::function<bool()> active)
	: QObject(parent),
	  display(std::move(preview)),
	  streaming(std::move(active))
{
	mode = corn_mode_parse(config_get_string(App()->GetAppConfig(), "CornOBS", "PerformanceMode"));
	const QByteArray override = qgetenv("CORNOBS_GPU_ADAPTIVE");
	mode = corn_mode_override(mode,
				  qEnvironmentVariableIsSet("CORNOBS_GPU_ADAPTIVE") ? override.constData() : nullptr);
	const char *modeName = mode == CORN_GAMING ? "gaming" : mode == CORN_BALANCED ? "balanced" : "compatibility";
	blog(LOG_INFO, "CornOBS GPU adaptive: mode=%s; display-only shedding; debug_override=%s", modeName,
	     qEnvironmentVariableIsSet("CORNOBS_GPU_ADAPTIVE") ? "yes" : "no");
	if (mode == CORN_COMPATIBILITY) {
		return;
	}
	clock.start();
	auto *timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, [this] { sample(); });
	timer->start(250);
	sample();
}

bool CornAdaptive::ShedThumbnails()
{
	return shedThumbnails;
}

unsigned CornAdaptive::MeterInterval()
{
	return meterInterval;
}

void CornAdaptive::sample()
{
	const bool active = streaming();
	shedThumbnails = mode == CORN_GAMING && active;
	if (mode == CORN_COMPATIBILITY || (mode == CORN_GAMING && !active)) {
		policy = {};
		haveCounters = false;
		meterInterval = 16;
		obs_display_set_max_fps(display(), 0);
		return;
	}
	struct obs_video_info info = {};
	if (!obs_get_video_info(&info) || !info.fps_num || !info.fps_den) {
		policy = {};
		haveCounters = false;
		meterInterval = corn_mode_meter_interval(CORN_NORMAL, mode, active);
		obs_display_set_max_fps(display(), corn_mode_preview_fps(CORN_NORMAL, mode, active));
		return;
	}
	const MemorySample memory = ReadMemory();
	submissionPeakUs = std::max(submissionPeakUs, obs_take_gpu_encode_submission_peak_us());
	const auto now = clock.elapsed();
	const double budgetNs = 1e9 * info.fps_den / info.fps_num;
	const uint64_t renderNs = obs_get_average_frame_time_ns();
	const uint32_t frames = obs_get_total_frames(), lag = obs_get_lagged_frames();
	double lagRatio = 0;
	if (haveCounters && frames > previousFrames && lag >= previousLag) {
		lagRatio = double(lag - previousLag) / double(frames - previousFrames);
	}
	previousFrames = frames;
	previousLag = lag;
	haveCounters = true;
	const auto before = policy.state;
	const corn_pressure_sample input = {memory.valid ? double(memory.usage) / double(memory.budget) : 0,
					    double(renderNs) / budgetNs, lagRatio, memory.valid, renderNs > 0};
	corn_pressure_update(&policy, input, uint64_t(now));
	const unsigned cap = corn_mode_preview_fps(policy.state, mode, active);
	obs_display_t *preview = display();
	obs_display_set_max_fps(preview, cap);
	const uint64_t previewFrames = obs_display_get_rendered_frames(preview);
	const double previewFps =
		previousPreviewMs && now > previousPreviewMs && previewFrames >= previousPreviewFrames
			? double(previewFrames - previousPreviewFrames) * 1000.0 / double(now - previousPreviewMs)
			: 0;
	previousPreviewFrames = previewFrames;
	previousPreviewMs = now;
	meterInterval = corn_mode_meter_interval(policy.state, mode, active);
	if (policy.state != before || now - lastLog >= 30000) {
		static const char *names[] = {"NORMAL", "ELEVATED", "HIGH", "CRITICAL"};
		video_t *video = obs_get_video();
		blog(LOG_INFO,
		     "CornOBS GPU: state=%s adaptive=%s process_local_memory_valid=%d usage=%llu budget=%llu "
		     "available_reservation=%llu reservation=%llu render_ms=%.3f rendering_lag=%u "
		     "encoding_lag=%u preview_cap=%u preview_fps=%.2f meter_interval_ms=%u encode_host_call_peak_us=%u",
		     names[policy.state], mode == CORN_GAMING ? "gaming" : "balanced", int(memory.valid),
		     (unsigned long long)memory.usage, (unsigned long long)memory.budget,
		     (unsigned long long)memory.available, (unsigned long long)memory.reservation,
		     double(renderNs) / 1e6, lag, video ? video_output_get_skipped_frames(video) : 0, cap, previewFps,
		     meterInterval, submissionPeakUs);
		submissionPeakUs = 0;
		lastLog = now;
	}
}
