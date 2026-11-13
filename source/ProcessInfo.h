/*
	This file is part of AVSMeter, Copyright(C) Groucho2004.

	AVSMeter is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	AVSMeter is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with AVSMeter. If not, see <http://www.gnu.org/licenses/>.

	Windows part for CPU usage by (c) 2009 Ben Watson	derived from
	http://www.philosophicalgeek.com/2009/01/03/determine-cpu-usage-of-current-process-c-and-c/
*/

#pragma once

#include "common.h"

class CProcessInfo
{
public:
	CProcessInfo();
	virtual ~CProcessInfo();
	void Update();
	void CloseProcess();

	//public variables:
	WORD  wCPUUsage;
	WORD  wThreadCount;
	DWORD dwPhysicalMemMB;
	DWORD dwVirtualMemMB;

private:
	WORD GetCurrentThreadCount();
	BYTE GetCPUUsage();
	unsigned __int64 SubtractTimes(const FILETIME& ftA, const FILETIME& ftB);
	BYTE btCPUUsage;
	BOOL bFirstRun;
	unsigned __int64 GetSTDTimer();

	FILETIME ftPrevSysKernel;
	FILETIME ftPrevSysUser;
	FILETIME ftPrevProcKernel;
	FILETIME ftPrevProcUser;

	HANDLE hProcess;
	PROCESS_MEMORY_COUNTERS pmc;
	volatile LONG lRunCount;
};


CProcessInfo::CProcessInfo()
{
	hProcess = ::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, ::GetCurrentProcessId());

	dwPhysicalMemMB = 0;
	dwVirtualMemMB = 0;

	::ZeroMemory(&ftPrevSysKernel, sizeof(FILETIME));
	::ZeroMemory(&ftPrevSysUser, sizeof(FILETIME));
	::ZeroMemory(&ftPrevProcKernel, sizeof(FILETIME));
	::ZeroMemory(&ftPrevProcUser, sizeof(FILETIME));

	btCPUUsage = 0;
	wThreadCount = GetCurrentThreadCount();
	bFirstRun = TRUE;
	lRunCount = 0;
}

CProcessInfo::~CProcessInfo()
{
}


void CProcessInfo::CloseProcess()
{
	if (hProcess)
		::CloseHandle(hProcess);

	return;
}


void CProcessInfo::Update()
{
	static unsigned __int64 uiLastTimeRunCPU;
	static unsigned __int64 uiLastTimeRunThreadCount;

	//Min. update interval 200ms
	if ((GetSTDTimer() - uiLastTimeRunCPU) < 100)
		return;

	btCPUUsage = GetCPUUsage();
	bFirstRun = FALSE;

	if ((btCPUUsage < 1) || (btCPUUsage > 100))
		wCPUUsage = 0;
	else
		wCPUUsage = (WORD)btCPUUsage;

	::GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc));
	dwPhysicalMemMB = (DWORD)(((double)(pmc.WorkingSetSize) / 1048576.0) + 0.5);
	dwVirtualMemMB = (DWORD)(((double)(pmc.PagefileUsage) / 1048576.0) + 0.5);

	uiLastTimeRunCPU = GetSTDTimer();

	//Thread count only needs to be updated every few seconds
	if ((GetSTDTimer() - uiLastTimeRunThreadCount) < 4000)
		return;

	wThreadCount = GetCurrentThreadCount();
	uiLastTimeRunThreadCount = GetSTDTimer();

	return;
}


BYTE CProcessInfo::GetCPUUsage()
{
	//create a local copy to protect against race conditions in setting the member variable
	BYTE btCPUUsageCopy = btCPUUsage;

	if (::InterlockedIncrement(&lRunCount) == 1)
	{
		FILETIME ftSysIdle, ftSysKernel, ftSysUser, ftProcCreation, ftProcExit, ftProcKernel, ftProcUser;
		if (!::GetSystemTimes(&ftSysIdle, &ftSysKernel, &ftSysUser) || !::GetProcessTimes(::GetCurrentProcess(), &ftProcCreation, &ftProcExit, &ftProcKernel, &ftProcUser))
		{
			::InterlockedDecrement(&lRunCount);
			return btCPUUsageCopy;
		}

		if (!bFirstRun)
		{
			/*
			CPU usage is calculated by getting the total amount of time the system
			has operated since the last measurement (made up of kernel + user) and
			the total amount of time the process has run (kernel + user).
			*/
			unsigned __int64 uiSysKernelDiff = SubtractTimes(ftSysKernel, ftPrevSysKernel);
			unsigned __int64 uiSysUserDiff = SubtractTimes(ftSysUser, ftPrevSysUser);
			unsigned __int64 uiProcKernelDiff = SubtractTimes(ftProcKernel, ftPrevProcKernel);
			unsigned __int64 uiProcUserDiff = SubtractTimes(ftProcUser, ftPrevProcUser);

			if ((uiSysKernelDiff + uiSysUserDiff) > 0)
				btCPUUsage = (BYTE)((100.0 * (double)(uiProcKernelDiff + uiProcUserDiff)) / (double)(uiSysKernelDiff + uiSysUserDiff));
		}

		ftPrevSysKernel = ftSysKernel;
		ftPrevSysUser = ftSysUser;
		ftPrevProcKernel = ftProcKernel;
		ftPrevProcUser = ftProcUser;

		btCPUUsageCopy = btCPUUsage;
	}

	::InterlockedDecrement(&lRunCount);

	return btCPUUsageCopy;
}


unsigned __int64 CProcessInfo::SubtractTimes(const FILETIME& ftA, const FILETIME& ftB)
{
	unsigned __int64 a = (((unsigned __int64)ftA.dwHighDateTime) << 32) + (unsigned __int64)ftA.dwLowDateTime;
	unsigned __int64 b = (((unsigned __int64)ftB.dwHighDateTime) << 32) + (unsigned __int64)ftB.dwLowDateTime;

	return (a - b);
}


WORD CProcessInfo::GetCurrentThreadCount()
{
	DWORD dwPID = ::GetCurrentProcessId();
	HANDLE hThreadSnapshot = INVALID_HANDLE_VALUE;
	THREADENTRY32 te32;

	hThreadSnapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hThreadSnapshot == INVALID_HANDLE_VALUE)
		return 0;

	te32.dwSize = sizeof(THREADENTRY32);

	if (!::Thread32First(hThreadSnapshot, &te32))
	{
		::CloseHandle(hThreadSnapshot);
		return 0;
	}

	WORD wThreads = 0;
	while (::Thread32Next(hThreadSnapshot, &te32))
	{
		if (te32.th32OwnerProcessID == dwPID)
			++wThreads;
	}

	::CloseHandle(hThreadSnapshot);

	return wThreads;
}


unsigned __int64 CProcessInfo::GetSTDTimer()
{
	static unsigned __int64 uiLastSTDTimer;
	unsigned __int64 uiSTDTimer = (unsigned __int64)GetTickCount();
	unsigned __int64 uiTimerWrapComp = (uiSTDTimer < uiLastSTDTimer) ? 4294967296 : 0;
	uiLastSTDTimer = uiSTDTimer;

	return (uiSTDTimer + uiTimerWrapComp);
}


