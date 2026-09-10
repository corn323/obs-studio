/* SPDX-License-Identifier: MIT */
/* Exercise the production adapter with injected API failures. No OBS instance,
 * global scheduler settings, registry, or other process is touched. */
#include "../../libobs/util/threading-scheduler-windows.c"
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define CHECK(condition) \
	do { \
		if (!(condition)) { \
			fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
			exit(1); \
		} \
	} while (0)

static int failure_at, apply_calls, restore_calls, warnings;
static bool cpu_active, priority_active, mmcss_active, power_active, rollback_failure;
static int selected_failure;
static bool verbose;
static SYSTEM_CPU_SET_INFORMATION topology_records[4];
static int topology_failure;
static bool saw_audio, saw_playback;
static AVRT_PRIORITY last_mmcss_priority;

void blog(int level, const char *format, ...)
{
	if (level <= LOG_WARNING)
		warnings++;
	if (verbose) {
		va_list args;
		va_start(args, format);
		vprintf(format, args);
		putchar('\n');
		va_end(args);
	}
}

static bool fail_apply(void)
{
	apply_calls++;
	if (failure_at == apply_calls) {
		SetLastError(ERROR_ACCESS_DENIED);
		return true;
	}
	return false;
}

static BOOL WINAPI fake_selected(HANDLE thread, PULONG ids, ULONG capacity, PULONG count)
{
	CHECK(thread == GetCurrentThread());
	CHECK(!ids && !capacity);
	*count = selected_failure == 2 ? 1 : 0;
	return selected_failure != 1;
}

static BOOL WINAPI fake_process_selected(HANDLE process, PULONG ids, ULONG capacity, PULONG count)
{
	CHECK(process == GetCurrentProcess());
	CHECK(!ids && !capacity);
	*count = selected_failure == 4 ? 1 : 0;
	return selected_failure != 3;
}

static BOOL WINAPI fake_select(HANDLE thread, const ULONG *ids, ULONG count)
{
	CHECK(thread == GetCurrentThread());
	if (!ids) {
		CHECK(count == 0);
		restore_calls++;
		if (!rollback_failure)
			cpu_active = false;
		return !rollback_failure;
	}
	CHECK(count == placement.count);
	if (fail_apply())
		return FALSE;
	cpu_active = true;
	return TRUE;
}

static BOOL WINAPI fake_priority(HANDLE thread, int value)
{
	CHECK(thread == GetCurrentThread());
	if (priority_active) {
		restore_calls++;
		CHECK(value == GetThreadPriority(thread));
		if (!rollback_failure)
			priority_active = false;
		return !rollback_failure;
	}
	CHECK(value == THREAD_PRIORITY_NORMAL);
	if (fail_apply())
		return FALSE;
	priority_active = true;
	return TRUE;
}

static HANDLE WINAPI fake_mmcss(LPCWSTR name, LPDWORD index)
{
	CHECK(*index == 0);
	saw_audio = wcscmp(name, L"Audio") == 0;
	saw_playback = wcscmp(name, L"Playback") == 0;
	CHECK(saw_audio || saw_playback);
	if (fail_apply())
		return NULL;
	mmcss_active = true;
	return (HANDLE)(uintptr_t)123;
}

static BOOL WINAPI fake_mmcss_priority(HANDLE handle, AVRT_PRIORITY priority)
{
	CHECK(handle == (HANDLE)(uintptr_t)123);
	last_mmcss_priority = priority;
	return !fail_apply();
}

static BOOL WINAPI fake_revert(HANDLE handle)
{
	CHECK(handle == (HANDLE)(uintptr_t)123 && mmcss_active);
	restore_calls++;
	if (!rollback_failure)
		mmcss_active = false;
	return !rollback_failure;
}

static BOOL WINAPI fake_power(HANDLE thread, THREAD_INFORMATION_CLASS type, LPVOID value, DWORD size)
{
	CHECK(thread == GetCurrentThread() && type == ThreadPowerThrottling);
	CHECK(size == sizeof(THREAD_POWER_THROTTLING_STATE));
	THREAD_POWER_THROTTLING_STATE *power = value;
	CHECK(power->Version == THREAD_POWER_THROTTLING_CURRENT_VERSION && power->StateMask == 0);
	if (power->ControlMask == 0) {
		restore_calls++;
		if (!rollback_failure)
			power_active = false;
		return !rollback_failure;
	}
	CHECK(power->ControlMask == THREAD_POWER_THROTTLING_EXECUTION_SPEED);
	if (fail_apply())
		return FALSE;
	power_active = true;
	return TRUE;
}

static BOOL WINAPI fake_topology(PSYSTEM_CPU_SET_INFORMATION buffer, ULONG bytes, PULONG returned, HANDLE process,
				 ULONG flags)
{
	CHECK(process == GetCurrentProcess() && flags == 0);
	*returned = sizeof(topology_records);
	if (!buffer) {
		CHECK(bytes == 0);
		if (topology_failure == 1) {
			*returned = 0;
			SetLastError(ERROR_ACCESS_DENIED);
		} else {
			SetLastError(ERROR_INSUFFICIENT_BUFFER);
		}
		return FALSE;
	}
	CHECK(bytes == sizeof(topology_records));
	if (topology_failure == 2)
		return FALSE;
	memcpy(buffer, topology_records, bytes);
	if (topology_failure == 3)
		buffer->Size = 0;
	if (topology_failure == 4)
		buffer->Size = bytes + 1;
	if (topology_failure == 5)
		buffer->Size = offsetof(SYSTEM_CPU_SET_INFORMATION, CpuSet);
	if (topology_failure == 6)
		*returned = bytes - 1;
	if (topology_failure == 7)
		*returned = bytes + 1;
	if (topology_failure == 8)
		*returned = 0;
	if (topology_failure == 9)
		buffer->Size++;
	return TRUE;
}

static BOOL CALLBACK skip_init(PINIT_ONCE once, PVOID param, PVOID *context)
{
	(void)once;
	(void)param;
	(void)context;
	return TRUE;
}

static void reset(void)
{
	free(selected_ids);
	selected_ids = NULL;
	memset(&placement, 0, sizeof(placement));
	init_failure = NULL;
	scheduler_enabled = true;
	failure_at = apply_calls = restore_calls = warnings = selected_failure = topology_failure = 0;
	cpu_active = priority_active = mmcss_active = power_active = rollback_failure = false;
	saw_audio = saw_playback = false;
	api = (struct scheduler_api){fake_topology,         fake_select, fake_selected,
				     fake_process_selected, fake_power,  fake_mmcss,
				     fake_mmcss_priority,   fake_revert, fake_priority};
	for (ULONG i = 0; i < 4; i++) {
		memset(&topology_records[i], 0, sizeof(topology_records[i]));
		topology_records[i].Size = sizeof(topology_records[i]);
		topology_records[i].Type = CpuSetInformation;
		topology_records[i].CpuSet.Id = 100 + i * 10;
		topology_records[i].CpuSet.LogicalProcessorIndex = (BYTE)i;
		topology_records[i].CpuSet.CoreIndex = (BYTE)i;
		topology_records[i].CpuSet.LastLevelCacheIndex = (BYTE)(i / 2);
	}
}

static void check_reverted(void)
{
	CHECK(!cpu_active && !priority_active && !mmcss_active && !power_active);
}

static void test_apply_and_cleanup(void)
{
	for (int role = OS_THREAD_ROLE_AUDIO; role <= OS_THREAD_ROLE_GPU_ENCODE; role++) {
		reset();
		CHECK(read_topology());
		CHECK(placement.local && placement.count == 2 && selected_ids);
		struct os_thread_scheduler *state = os_thread_scheduler_begin((enum os_thread_role)role);
		CHECK(state && apply_calls == 5);
		CHECK(cpu_active && priority_active && mmcss_active && power_active);
		CHECK((role == OS_THREAD_ROLE_AUDIO) == saw_audio);
		CHECK((role != OS_THREAD_ROLE_AUDIO) == saw_playback);
		CHECK(last_mmcss_priority == (role == OS_THREAD_ROLE_AUDIO ? AVRT_PRIORITY_NORMAL : AVRT_PRIORITY_LOW));
		os_thread_scheduler_end(state);
		CHECK(restore_calls == 4 && warnings == 0);
		check_reverted();
	}
}

static void test_failures(void)
{
	/* SetThreadSelectedCpuSets, SetThreadPriority, MMCSS registration,
	 * MMCSS relative priority, and power throttling, one failure at a time. */
	for (int step = 1; step <= 5; step++) {
		reset();
		CHECK(read_topology());
		failure_at = step;
		CHECK(!os_thread_scheduler_begin(OS_THREAD_ROLE_GRAPHICS));
		CHECK(apply_calls == step && warnings == 1);
		CHECK(restore_calls == (step > 3 ? 3 : step - 1));
		check_reverted();
	}
	/* Rollback failures must be reported, and must not skip remaining cleanup. */
	reset();
	CHECK(read_topology());
	struct os_thread_scheduler *state = os_thread_scheduler_begin(OS_THREAD_ROLE_AUDIO);
	CHECK(state);
	rollback_failure = true;
	os_thread_scheduler_end(state);
	CHECK(restore_calls == 4 && warnings == 4);
	os_thread_scheduler_end(NULL);
	/* Retain an MMCSS handle after incomplete fallback, then retry on exit. */
	reset();
	CHECK(read_topology());
	failure_at = 5;
	rollback_failure = true;
	state = os_thread_scheduler_begin(OS_THREAD_ROLE_GRAPHICS);
	CHECK(state && state->mmcss && state->cpu_changed && state->priority_changed);
	CHECK(warnings == 5);
	rollback_failure = false;
	os_thread_scheduler_end(state);
	check_reverted();
}

static void test_inactive_and_existing_constraints(void)
{
	reset();
	scheduler_enabled = false;
	CHECK(!os_thread_scheduler_begin(OS_THREAD_ROLE_AUDIO) && apply_calls == 0);
	reset();
	init_failure = "unknown topology";
	CHECK(!os_thread_scheduler_begin(OS_THREAD_ROLE_AUDIO) && apply_calls == 0);
	reset();
	CHECK(!os_thread_scheduler_begin(OS_THREAD_ROLE_NETWORK));
	CHECK(!os_thread_scheduler_begin(OS_THREAD_ROLE_BACKGROUND));
	CHECK(apply_calls == 0);
	for (int failure = 1; failure <= 4; failure++) {
		reset();
		selected_failure = failure;
		CHECK(!os_thread_scheduler_begin(OS_THREAD_ROLE_AUDIO));
		CHECK(apply_calls == 0 && warnings == 1);
	}
}

static void test_topology(void)
{
	for (int failure = 1; failure <= 9; failure++) {
		reset();
		topology_failure = failure;
		CHECK(!read_topology());
		CHECK(init_failure && !selected_ids);
		CHECK(!os_thread_scheduler_begin(OS_THREAD_ROLE_AUDIO) && apply_calls == 0);
	}
	reset();
	for (size_t i = 0; i < 4; i++)
		topology_records[i].CpuSet.LastLevelCacheIndex = 0;
	CHECK(read_topology());
	CHECK(!placement.local && !selected_ids);
	struct os_thread_scheduler *state = os_thread_scheduler_begin(OS_THREAD_ROLE_AUDIO);
	CHECK(state && !cpu_active && apply_calls == 4);
	os_thread_scheduler_end(state);
	check_reverted();
	reset();
	for (size_t i = 0; i < 4; i++) {
		topology_records[i].CpuSet.EfficiencyClass = i < 2 ? 9 : 1;
		topology_records[i].CpuSet.Parked = 1;
	}
	CHECK(read_topology());
	CHECK(placement.hybrid && placement.count == 2 && placement.efficiency == 9);
	CHECK(selected_ids[0] == 100 && selected_ids[1] == 110);
	reset();
	for (size_t i = 0; i < 4; i++)
		topology_records[i].CpuSet.Allocated = 1;
	CHECK(!read_topology());
	reset();
	for (size_t i = 0; i < 4; i++) {
		topology_records[i].CpuSet.Allocated = 1;
		topology_records[i].CpuSet.AllocatedToTargetProcess = 1;
	}
	CHECK(read_topology());
}

int main(int argc, char **argv)
{
	/* Optional smoke run exercises only this disposable helper's own thread. */
	if (argc == 2 && strcmp(argv[1], "--live") == 0) {
		verbose = true;
		struct os_thread_scheduler *state = os_thread_scheduler_begin(OS_THREAD_ROLE_AUDIO);
		printf("Live API result: %s (not a performance test)\n", warnings ? "failure reported"
									 : state  ? "applied"
										  : "inactive/fallback");
		os_thread_scheduler_end(state);
		return warnings ? 1 : 0;
	}
	CHECK(InitOnceExecuteOnce(&scheduler_once, skip_init, NULL, NULL));
	test_apply_and_cleanup();
	test_failures();
	test_inactive_and_existing_constraints();
	test_topology();
	free(selected_ids);
	puts("PASS: Windows role policy, cleanup, all apply/rollback failures, off/default roles, existing CPU sets, topology parsing");
	return 0;
}
