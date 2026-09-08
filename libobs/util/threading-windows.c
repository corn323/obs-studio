/*
 * Copyright (c) 2023 Lain Bailey <lain@obsproject.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "bmem.h"
#include "threading.h"
#include "util/platform.h"

#include <string.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef __MINGW32__
#include <excpt.h>
#ifndef TRYLEVEL_NONE
#ifndef __MINGW64__
#define NO_SEH_MINGW
#endif
#ifndef __try
#define __try
#endif
#ifndef __except
#define __except (x) if (0)
#endif
#endif
#endif

int os_event_init(os_event_t **event, enum os_event_type type)
{
	HANDLE handle;

	handle = CreateEvent(NULL, (type == OS_EVENT_TYPE_MANUAL), FALSE, NULL);
	if (!handle)
		return -1;

	*event = (os_event_t *)handle;
	return 0;
}

void os_event_destroy(os_event_t *event)
{
	if (event)
		CloseHandle((HANDLE)event);
}

int os_event_wait(os_event_t *event)
{
	DWORD code;

	if (!event)
		return EINVAL;

	code = WaitForSingleObject((HANDLE)event, INFINITE);
	if (code != WAIT_OBJECT_0)
		return EINVAL;

	return 0;
}

int os_event_timedwait(os_event_t *event, unsigned long milliseconds)
{
	DWORD code;

	if (!event)
		return EINVAL;

	code = WaitForSingleObject((HANDLE)event, milliseconds);
	if (code == WAIT_TIMEOUT)
		return ETIMEDOUT;
	else if (code != WAIT_OBJECT_0)
		return EINVAL;

	return 0;
}

int os_event_try(os_event_t *event)
{
	DWORD code;

	if (!event)
		return EINVAL;

	code = WaitForSingleObject((HANDLE)event, 0);
	if (code == WAIT_TIMEOUT)
		return EAGAIN;
	else if (code != WAIT_OBJECT_0)
		return EINVAL;

	return 0;
}

int os_event_signal(os_event_t *event)
{
	if (!event)
		return EINVAL;

	if (!SetEvent((HANDLE)event))
		return EINVAL;

	return 0;
}

void os_event_reset(os_event_t *event)
{
	if (!event)
		return;

	ResetEvent((HANDLE)event);
}

int os_sem_init(os_sem_t **sem, int value)
{
	HANDLE handle = CreateSemaphore(NULL, (LONG)value, 0x7FFFFFFF, NULL);
	if (!handle)
		return -1;

	*sem = (os_sem_t *)handle;
	return 0;
}

void os_sem_destroy(os_sem_t *sem)
{
	if (sem)
		CloseHandle((HANDLE)sem);
}

int os_sem_post(os_sem_t *sem)
{
	if (!sem)
		return -1;
	return ReleaseSemaphore((HANDLE)sem, 1, NULL) ? 0 : -1;
}

int os_sem_wait(os_sem_t *sem)
{
	DWORD ret;

	if (!sem)
		return -1;
	ret = WaitForSingleObject((HANDLE)sem, INFINITE);
	return (ret == WAIT_OBJECT_0) ? 0 : -1;
}

#define VC_EXCEPTION 0x406D1388

#pragma pack(push, 8)
struct vs_threadname_info {
	DWORD type; /* 0x1000 */
	const char *name;
	DWORD thread_id;
	DWORD flags;
};
#pragma pack(pop)

#define THREADNAME_INFO_SIZE (sizeof(struct vs_threadname_info) / sizeof(ULONG_PTR))

void os_set_thread_name(const char *name)
{
#ifdef __MINGW32__
	UNUSED_PARAMETER(name);
#else
	struct vs_threadname_info info;
	info.type = 0x1000;
	info.name = name;
	info.thread_id = GetCurrentThreadId();
	info.flags = 0;

#ifdef NO_SEH_MINGW
	__try1(EXCEPTION_EXECUTE_HANDLER)
	{
#else
	__try {
#endif
		RaiseException(VC_EXCEPTION, 0, THREADNAME_INFO_SIZE, (ULONG_PTR *)&info);
#ifdef NO_SEH_MINGW
	}
	__except1
	{
#else
	} __except (EXCEPTION_EXECUTE_HANDLER) {
#endif
	}
#endif

	const HMODULE hModule = LoadLibrary(L"KernelBase.dll");
	if (hModule) {
		typedef HRESULT(WINAPI * set_thread_description_t)(HANDLE, PCWSTR);

		const set_thread_description_t std =
			(set_thread_description_t)GetProcAddress(hModule, "SetThreadDescription");
		if (std) {
			wchar_t *wname;
			os_utf8_to_wcs_ptr(name, 0, &wname);

			std(GetCurrentThread(), wname);

			bfree(wname);
		}

		FreeLibrary(hModule);
	}
}

int os_set_thread_priority(enum os_thread_priority priority)
{
	int native;

	switch (priority) {
	case OS_THREAD_PRIORITY_HIGH:
		native = THREAD_PRIORITY_HIGHEST;
		break;
	case OS_THREAD_PRIORITY_ABOVE_NORMAL:
		native = THREAD_PRIORITY_ABOVE_NORMAL;
		break;
	default:
		native = THREAD_PRIORITY_NORMAL;
		break;
	}

	return SetThreadPriority(GetCurrentThread(), native) ? 0 : -1;
}

#ifndef THREAD_POWER_THROTTLING_CURRENT_VERSION
#define THREAD_POWER_THROTTLING_CURRENT_VERSION 1
#define THREAD_POWER_THROTTLING_EXECUTION_SPEED 0x1
typedef struct _THREAD_POWER_THROTTLING_STATE {
	ULONG Version;
	ULONG ControlMask;
	ULONG StateMask;
} THREAD_POWER_THROTTLING_STATE;
#endif

/* ThreadPowerThrottling THREAD_INFORMATION_CLASS value (Win10 1809+). */
#define CORNOBS_ThreadPowerThrottling ((THREAD_INFORMATION_CLASS)3)

void os_thread_enable_realtime_media(void)
{
	/* MMCSS: register with the multimedia class scheduler so the thread
	 * gets glitch-resistant scheduling even while a game holds the
	 * foreground. Loaded dynamically to avoid linking avrt. The task
	 * handle is deliberately leaked - it is meant to live for the whole
	 * lifetime of the thread and these threads run until shutdown. */
	static HMODULE avrt = NULL;
	if (!avrt)
		avrt = LoadLibraryW(L"avrt.dll");

	if (avrt) {
		typedef HANDLE(WINAPI * set_mm_char_t)(LPCWSTR, LPDWORD);
		const set_mm_char_t set_mm_char =
			(set_mm_char_t)GetProcAddress(avrt, "AvSetMmThreadCharacteristicsW");
		if (set_mm_char) {
			DWORD task_index = 0;
			set_mm_char(L"Pro Audio", &task_index);
		}
	}

	/* Opt this thread out of EcoQoS / power throttling so Windows will
	 * not park it on an efficiency core or slow it down when OBS is not
	 * the foreground process. Available on Windows 10 1809+. */
	THREAD_POWER_THROTTLING_STATE state;
	memset(&state, 0, sizeof(state));
	state.Version = THREAD_POWER_THROTTLING_CURRENT_VERSION;
	state.ControlMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED;
	state.StateMask = 0; /* 0 = throttling disabled */
	SetThreadInformation(GetCurrentThread(), CORNOBS_ThreadPowerThrottling, &state, sizeof(state));
}

static int popcount64(uint64_t v)
{
	int c = 0;
	while (v) {
		v &= v - 1;
		c++;
	}
	return c;
}

uint64_t os_thread_pin_to_media_die(void)
{
	char *mode = getenv("CORNOBS_SCHED");
	if (!mode || strcmp(mode, "ccd") != 0)
		return 0;

	DWORD len = 0;
	GetLogicalProcessorInformationEx(RelationCache, NULL, &len);
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || len == 0)
		return 0;

	SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *buf = bmalloc(len);
	if (!GetLogicalProcessorInformationEx(RelationCache, buf, &len)) {
		bfree(buf);
		return 0;
	}

	/* Collect the affinity mask of every L3 cache. On Ryzen each L3 is one
	 * CCX/CCD, so this enumerates the dies. Only single-group systems
	 * (<= 64 logical processors) are handled; bail otherwise. */
	uint64_t die_masks[16];
	int die_count = 0;

	BYTE *p = (BYTE *)buf;
	BYTE *end = p + len;
	while (p < end && die_count < 16) {
		SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *info = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *)p;
		if (info->Relationship == RelationCache && info->Cache.Level == 3 &&
		    info->Cache.GroupCount == 1 && info->Cache.GroupMask.Group == 0) {
			die_masks[die_count++] = (uint64_t)info->Cache.GroupMask.Mask;
		} else if (info->Relationship == RelationCache && info->Cache.Level == 3) {
			/* multi-group / group != 0: out of scope, don't guess */
			bfree(buf);
			return 0;
		}
		p += info->Size;
	}
	bfree(buf);

	if (die_count < 2)
		return 0; /* single die: leave scheduling to the OS */

	/* Pick the die with the most logical processors (ties -> first). */
	uint64_t best = die_masks[0];
	int best_pop = popcount64(die_masks[0]);
	for (int i = 1; i < die_count; i++) {
		int pop = popcount64(die_masks[i]);
		if (pop > best_pop) {
			best_pop = pop;
			best = die_masks[i];
		}
	}

	if (best_pop < 4)
		return 0; /* too small to be worth confining to */

	return SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)best) ? best : 0;
}
