/*
	This file is part of AVSMeter,
	Copyright(C) Groucho2004.

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
*/

#ifndef _PROCESSINFO_H
#define _PROCESSINFO_H

#include "stdafx.h"

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
	void GetCPUUsage();
	inline unsigned __int64 SubtractTimes(const FILETIME& ftA, const FILETIME& ftB);
	BYTE btCPUUsage;
	BOOL bFirstRun;
	unsigned __int64 GetSTDTimer();
	unsigned __int64 uiLastSTDTimer;

	FILETIME ftSysIdle, ftSysKernel, ftSysUser;
	FILETIME ftProcCreation, ftProcExit, ftProcKernel, ftProcUser;

	//system total times
	FILETIME ftPrevSysKernel;
	FILETIME ftPrevSysUser;

	//process times
	FILETIME ftPrevProcKernel;
	FILETIME ftPrevProcUser;

	unsigned __int64 uiSysKernelDiff;
	unsigned __int64 uiSysUserDiff;
	unsigned __int64 uiProcKernelDiff;
	unsigned __int64 uiProcUserDiff;
	unsigned __int64 uiTotalSys;
	unsigned __int64 uiTotalProc;

	unsigned __int64 uiLastTimeRun;
	unsigned __int64 uiLastTimeRunThreadCount;
	HANDLE hProcess;
	PROCESS_MEMORY_COUNTERS pmc;
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

	uiLastTimeRun = 0;
	uiLastTimeRunThreadCount = 0;
	uiLastSTDTimer = 0;
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
	//Min. update interval 100ms
	if ((GetSTDTimer() - uiLastTimeRun) < 100)
		return;

	GetCPUUsage();
	bFirstRun = FALSE;

	if ((btCPUUsage < 1) || (btCPUUsage > 100))
		wCPUUsage = 0;
	else
		wCPUUsage = (WORD)btCPUUsage;

	if ((GetSTDTimer() - uiLastTimeRunThreadCount) > 2000)
	{
		wThreadCount = GetCurrentThreadCount();
		uiLastTimeRunThreadCount = GetSTDTimer();
	}

	::GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc));
	dwPhysicalMemMB = (DWORD)(((double)(pmc.WorkingSetSize) / 1048576.0) + 0.5);
	dwVirtualMemMB = (DWORD)(((double)(pmc.PagefileUsage) / 1048576.0) + 0.5);

	uiLastTimeRun = GetSTDTimer();

	return;
}


void CProcessInfo::GetCPUUsage()
{
	if (!::GetSystemTimes(&ftSysIdle, &ftSysKernel, &ftSysUser) || !::GetProcessTimes(::GetCurrentProcess(), &ftProcCreation, &ftProcExit, &ftProcKernel, &ftProcUser))
		return;

	if (!bFirstRun)
	{
		/*
		CPU usage is calculated by getting the total amount of time the system
		has operated since the last measurement (made up of kernel + user) and
		the total amount of time the process has run (kernel + user).
		*/
		uiSysKernelDiff = SubtractTimes(ftSysKernel, ftPrevSysKernel);
		uiSysUserDiff = SubtractTimes(ftSysUser, ftPrevSysUser);
		uiProcKernelDiff = SubtractTimes(ftProcKernel, ftPrevProcKernel);
		uiProcUserDiff = SubtractTimes(ftProcUser, ftPrevProcUser);
		uiTotalSys =  uiSysKernelDiff + uiSysUserDiff;
		uiTotalProc = uiProcKernelDiff + uiProcUserDiff;

		if (uiTotalSys > 0)
			btCPUUsage = (BYTE)((100.0 * (double)uiTotalProc) / (double)uiTotalSys);
	}
	
	ftPrevSysKernel = ftSysKernel;
	ftPrevSysUser = ftSysUser;
	ftPrevProcKernel = ftProcKernel;
	ftPrevProcUser = ftProcUser;

	return;
}


inline unsigned __int64 CProcessInfo::SubtractTimes(const FILETIME& ftA, const FILETIME& ftB)
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
	unsigned __int64 uiSTDTimer = (unsigned __int64)GetTickCount();
	unsigned __int64 uiTimerWrapComp = (uiSTDTimer < uiLastSTDTimer) ? 4294967296 : 0;
	uiLastSTDTimer = uiSTDTimer;

	return (uiSTDTimer + uiTimerWrapComp);
}


#endif //_PROCESSINFO_H

