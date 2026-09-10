/* SPDX-License-Identifier: MIT */
#include "threading-scheduler-policy.h"
#include "base.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <avrt.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

typedef BOOL(WINAPI *get_topology_t)(PSYSTEM_CPU_SET_INFORMATION, ULONG, PULONG, HANDLE, ULONG);
typedef BOOL(WINAPI *set_cpu_sets_t)(HANDLE, const ULONG *, ULONG);
typedef BOOL(WINAPI *get_cpu_sets_t)(HANDLE, PULONG, ULONG, PULONG);
typedef BOOL(WINAPI *set_power_t)(HANDLE, THREAD_INFORMATION_CLASS, LPVOID, DWORD);
typedef HANDLE(WINAPI *set_mmcss_t)(LPCWSTR, LPDWORD);
typedef BOOL(WINAPI *set_mmcss_priority_t)(HANDLE, AVRT_PRIORITY);
typedef BOOL(WINAPI *revert_mmcss_t)(HANDLE);
typedef BOOL(WINAPI *set_priority_t)(HANDLE, int);

struct scheduler_api {
	get_topology_t topology;
	set_cpu_sets_t select;
	get_cpu_sets_t selected;
	get_cpu_sets_t process_selected;
	set_power_t power;
	set_mmcss_t mmcss;
	set_mmcss_priority_t mmcss_priority;
	revert_mmcss_t revert;
	set_priority_t priority;
};

static struct scheduler_api api;
static INIT_ONCE scheduler_once = INIT_ONCE_STATIC_INIT;
static bool scheduler_enabled;
static struct scheduler_placement placement;
static ULONG *selected_ids;
static const char *init_failure = "not initialized";

struct os_thread_scheduler {
	DWORD owner;
	const char *role;
	int original_priority;
	HANDLE mmcss;
	bool cpu_changed;
	bool priority_changed;
	bool power_changed;
};

static void log_error(const char *role, const char *operation)
{
	DWORD error = GetLastError();
	blog(LOG_WARNING, "CornOBS scheduler: role=%s fallback=%s error=%lu", role, operation, (unsigned long)error);
}

static bool read_topology(void)
{
	ULONG bytes = 0;
	api.topology(NULL, 0, &bytes, GetCurrentProcess(), 0);
	if (!bytes || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
		init_failure = "GetSystemCpuSetInformation size query";
		return false;
	}
	/* Bound allocations and validate variable-sized records before accessing them. */
	if (bytes > 16 * 1024 * 1024) {
		init_failure = "unreasonable topology buffer size";
		return false;
	}
	void *buffer = malloc(bytes);
	if (!buffer) {
		init_failure = "topology allocation";
		return false;
	}
	ULONG returned = 0;
	if (!api.topology(buffer, bytes, &returned, GetCurrentProcess(), 0) || returned > bytes) {
		free(buffer);
		init_failure = "GetSystemCpuSetInformation";
		return false;
	}
	size_t capacity = returned / sizeof(SYSTEM_CPU_SET_INFORMATION);
	struct scheduler_cpu *cpus = calloc(capacity, sizeof(*cpus));
	if (!cpus) {
		free(buffer);
		init_failure = "CPU topology allocation";
		return false;
	}
	size_t count = 0, offset = 0;
	bool valid = true;
	while (offset < returned) {
		if (offset % _Alignof(SYSTEM_CPU_SET_INFORMATION)) {
			valid = false;
			break;
		}
		const SYSTEM_CPU_SET_INFORMATION *info = (const void *)((const unsigned char *)buffer + offset);
		if (returned - offset < offsetof(SYSTEM_CPU_SET_INFORMATION, CpuSet) ||
		    info->Size < offsetof(SYSTEM_CPU_SET_INFORMATION, CpuSet) || info->Size > returned - offset) {
			valid = false;
			break;
		}
		if (info->Type == CpuSetInformation) {
			if (info->Size < sizeof(*info) || count >= capacity) {
				valid = false;
				break;
			}
			cpus[count++] = (struct scheduler_cpu){
				.id = info->CpuSet.Id,
				.group = info->CpuSet.Group,
				.logical = info->CpuSet.LogicalProcessorIndex,
				.core = info->CpuSet.CoreIndex,
				.llc = info->CpuSet.LastLevelCacheIndex,
				.efficiency = info->CpuSet.EfficiencyClass,
				.available = (!info->CpuSet.Allocated || info->CpuSet.AllocatedToTargetProcess) &&
					     !info->CpuSet.RealTime,
			};
		}
		offset += info->Size;
	}
	free(buffer);
	if (!valid) {
		free(cpus);
		init_failure = "malformed CPU topology records";
		return false;
	}
	DWORD seed = GetCurrentProcessId();
	placement = scheduler_select(cpus, count, seed);
	blog(LOG_INFO, "CornOBS scheduler: locality_seed=%lu (fixed for process lifetime)", (unsigned long)seed);
	blog(LOG_INFO, "CornOBS scheduler: logical_processors=%zu LLC_domains=%zu hybrid=%s preferred_efficiency=%u",
	     count, placement.domains, placement.hybrid ? "yes" : "no", (unsigned)placement.efficiency);
	for (size_t i = 0; i < count; i++) {
		blog(LOG_DEBUG,
		     "CornOBS scheduler: CPU_set=%lu group=%u LP=%u core=%u LLC=%u EfficiencyClass=%u available=%s",
		     (unsigned long)cpus[i].id, (unsigned)cpus[i].group, (unsigned)cpus[i].logical,
		     (unsigned)cpus[i].core, (unsigned)cpus[i].llc, (unsigned)cpus[i].efficiency,
		     cpus[i].available ? "yes" : "no");
		bool seen = false;
		for (size_t j = 0; j < i; j++)
			if (cpus[j].group == cpus[i].group && cpus[j].llc == cpus[i].llc &&
			    cpus[j].efficiency == cpus[i].efficiency)
				seen = true;
		if (!seen)
			blog(LOG_INFO, "CornOBS scheduler: group=%u LLC=%u EfficiencyClass=%u", (unsigned)cpus[i].group,
			     (unsigned)cpus[i].llc, (unsigned)cpus[i].efficiency);
	}
	if (scheduler_enabled && placement.valid && (placement.hybrid || placement.local)) {
		selected_ids = malloc(placement.count * sizeof(*selected_ids));
		if (!selected_ids) {
			free(cpus);
			init_failure = "CPU set allocation";
			return false;
		}
		size_t selected = 0;
		for (size_t i = 0; i < count; i++) {
			if (!scheduler_cpu_selected(&cpus[i], &placement))
				continue;
			selected_ids[selected++] = cpus[i].id;
			blog(LOG_INFO, "CornOBS scheduler: selected CPU_set=%lu group=%u LLC=%u",
			     (unsigned long)cpus[i].id, (unsigned)cpus[i].group, (unsigned)cpus[i].llc);
		}
	}
	free(cpus);
	init_failure = placement.valid ? NULL : placement.reason;
	return placement.valid;
}

static BOOL CALLBACK scheduler_init(PINIT_ONCE once, PVOID param, PVOID *context)
{
	(void)once;
	(void)param;
	(void)context;
	char mode[16];
	SetLastError(ERROR_SUCCESS);
	DWORD length = GetEnvironmentVariableA("CORNOBS_SCHED", mode, sizeof(mode));
	bool unset = !length && GetLastError() == ERROR_ENVVAR_NOT_FOUND;
	scheduler_enabled = unset || (length && length < sizeof(mode) && scheduler_auto_mode(mode));
	init_failure = scheduler_enabled ? NULL : "mode off or unsupported (use off/auto; ccd retired)";
	blog(LOG_INFO, "CornOBS scheduler: mode=%s%s", scheduler_enabled ? "auto" : "off", unset ? " (default)" : "");
	HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
#define LOAD_API(module, member, type, name) \
	do { \
		FARPROC proc = GetProcAddress(module, name); \
		_Static_assert(sizeof(type) == sizeof(proc), "Windows function pointer size"); \
		memcpy(&api.member, &proc, sizeof(proc)); \
	} while (0)
	LOAD_API(kernel, topology, get_topology_t, "GetSystemCpuSetInformation");
	if (!scheduler_enabled) {
		/* Off still reports topology for A/B logs, but loads no MMCSS module
		 * and makes no scheduling mutations. */
		if (!api.topology || !read_topology())
			blog(LOG_INFO,
			     "CornOBS scheduler: logical_processors/LLC/EfficiencyClass unavailable in off mode");
		init_failure = "mode off or unsupported (use off/auto; ccd retired)";
		blog(LOG_INFO, "CornOBS scheduler: Windows defaults; %s", init_failure);
		return TRUE;
	}

	HMODULE avrt = LoadLibraryExW(L"avrt.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
	LOAD_API(kernel, select, set_cpu_sets_t, "SetThreadSelectedCpuSets");
	LOAD_API(kernel, selected, get_cpu_sets_t, "GetThreadSelectedCpuSets");
	LOAD_API(kernel, process_selected, get_cpu_sets_t, "GetProcessDefaultCpuSets");
	LOAD_API(kernel, power, set_power_t, "SetThreadInformation");
	LOAD_API(kernel, priority, set_priority_t, "SetThreadPriority");
	if (avrt) {
		LOAD_API(avrt, mmcss, set_mmcss_t, "AvSetMmThreadCharacteristicsW");
		LOAD_API(avrt, mmcss_priority, set_mmcss_priority_t, "AvSetMmThreadPriority");
		LOAD_API(avrt, revert, revert_mmcss_t, "AvRevertMmThreadCharacteristics");
	}
#undef LOAD_API
	/* Module and immutable topology intentionally live as long as libobs/process.
	 * MMCSS thread registrations, unlike the module, are always reverted. */
	if (!api.topology || !api.select || !api.selected || !api.process_selected || !api.power || !api.priority ||
	    !api.mmcss || !api.mmcss_priority || !api.revert)
		init_failure = "required Windows scheduling API unavailable";
	else
		read_topology();
	if (init_failure)
		blog(LOG_WARNING, "CornOBS scheduler: fallback=%s; Windows defaults", init_failure);
	return TRUE;
}

static bool scheduler_restore(struct os_thread_scheduler *state)
{
	HANDLE thread = GetCurrentThread();
	if (state->mmcss) {
		if (api.revert(state->mmcss))
			state->mmcss = NULL;
		else
			log_error(state->role, "AvRevertMmThreadCharacteristics rollback failed");
	}
	if (state->power_changed) {
		/* These are fresh OBS-owned threads. ControlMask=0 returns power policy
		 * to Windows, rather than explicitly enabling throttling. */
		THREAD_POWER_THROTTLING_STATE power = {.Version = THREAD_POWER_THROTTLING_CURRENT_VERSION};
		if (api.power(thread, ThreadPowerThrottling, &power, sizeof(power)))
			state->power_changed = false;
		else
			log_error(state->role, "SetThreadInformation rollback failed");
	}
	if (state->priority_changed) {
		if (api.priority(thread, state->original_priority))
			state->priority_changed = false;
		else
			log_error(state->role, "SetThreadPriority rollback failed");
	}
	if (state->cpu_changed) {
		if (api.select(thread, NULL, 0))
			state->cpu_changed = false;
		else
			log_error(state->role, "SetThreadSelectedCpuSets rollback failed");
	}
	return !state->mmcss && !state->power_changed && !state->priority_changed && !state->cpu_changed;
}

struct os_thread_scheduler *os_thread_scheduler_begin(enum os_thread_role role)
{
	struct scheduler_role_policy policy = scheduler_policy(role);
	if (!InitOnceExecuteOnce(&scheduler_once, scheduler_init, NULL, NULL)) {
		log_error(policy.name, "scheduler initialization");
		return NULL;
	}
	if (!scheduler_enabled || init_failure || !policy.critical) {
		blog(LOG_INFO,
		     "CornOBS scheduler: role=%s MMCSS=none priority=unchanged CPU_sets=default "
		     "LLC=unrestricted power=Windows fallback=%s",
		     policy.name, init_failure ? init_failure : "non-critical role");
		return NULL;
	}
	HANDLE thread = GetCurrentThread();
	ULONG previous_count = 0;
	if (!api.selected(thread, NULL, 0, &previous_count) || previous_count) {
		log_error(policy.name, "existing thread CPU sets or GetThreadSelectedCpuSets failure; left unchanged");
		return NULL;
	}
	if (!api.process_selected(GetCurrentProcess(), NULL, 0, &previous_count) || previous_count) {
		log_error(policy.name, "existing process CPU sets or GetProcessDefaultCpuSets failure; left unchanged");
		return NULL;
	}
	DWORD_PTR process_mask = 0, system_mask = 0;
	if (!GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask) || !process_mask ||
	    process_mask != system_mask) {
		log_error(policy.name, "restricted/unknown process affinity; left unchanged");
		return NULL;
	}
	struct os_thread_scheduler *state = calloc(1, sizeof(*state));
	if (!state) {
		log_error(policy.name, "thread state allocation");
		return NULL;
	}
	state->owner = GetCurrentThreadId();
	state->role = policy.name;
	state->original_priority = GetThreadPriority(thread);
	const char *failure = "GetThreadPriority";
	if (state->original_priority == THREAD_PRIORITY_ERROR_RETURN)
		goto fail;
	if (selected_ids) {
		failure = "SetThreadSelectedCpuSets";
		if (!api.select(thread, selected_ids, (ULONG)placement.count))
			goto fail;
		state->cpu_changed = true;
	}
	/* MMCSS owns dynamic priority; do not stack a HIGHEST base boost on it. */
	failure = "SetThreadPriority";
	if (!api.priority(thread, THREAD_PRIORITY_NORMAL))
		goto fail;
	state->priority_changed = true;
	DWORD task_index = 0;
	failure = "AvSetMmThreadCharacteristicsW";
	state->mmcss = api.mmcss(policy.mmcss == SCHEDULER_MMCSS_AUDIO ? L"Audio" : L"Playback", &task_index);
	if (!state->mmcss)
		goto fail;
	failure = "AvSetMmThreadPriority";
	if (!api.mmcss_priority(state->mmcss, policy.mmcss_priority == 0 ? AVRT_PRIORITY_NORMAL : AVRT_PRIORITY_LOW))
		goto fail;
	THREAD_POWER_THROTTLING_STATE power = {.Version = THREAD_POWER_THROTTLING_CURRENT_VERSION,
					       .ControlMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED,
					       .StateMask = 0};
	failure = "SetThreadInformation";
	if (!api.power(thread, ThreadPowerThrottling, &power, sizeof(power)))
		goto fail;
	state->power_changed = true;
	blog(LOG_INFO,
	     "CornOBS scheduler: role=%s MMCSS=%s/%s priority=NORMAL CPU_sets=%s count=%zu "
	     "LLC=%s group=%u index=%u power=execution-speed-unthrottled fallback=none (%s)",
	     policy.name, policy.mmcss == SCHEDULER_MMCSS_AUDIO ? "Audio" : "Playback",
	     policy.mmcss_priority == 0 ? "NORMAL" : "LOW", selected_ids ? "selected" : "Windows-default",
	     selected_ids ? placement.count : 0, placement.local ? "selected" : "unrestricted",
	     (unsigned)placement.group, (unsigned)placement.llc, placement.reason);
	return state;

fail:
	log_error(policy.name, failure);
	if (!scheduler_restore(state)) {
		blog(LOG_WARNING,
		     "CornOBS scheduler: role=%s rollback incomplete; retaining state for cleanup at thread exit",
		     policy.name);
		return state;
	}
	blog(LOG_INFO,
	     "CornOBS scheduler: role=%s MMCSS=none priority=restored CPU_sets=default "
	     "LLC=unrestricted power=Windows fallback=%s (rollback complete)",
	     policy.name, failure);
	free(state);
	return NULL;
}

void os_thread_scheduler_end(struct os_thread_scheduler *state)
{
	if (!state)
		return;
	if (state->owner != GetCurrentThreadId()) {
		blog(LOG_ERROR, "CornOBS scheduler: role=%s cleanup must run on owning thread", state->role);
		return;
	}
	scheduler_restore(state);
	free(state);
}
