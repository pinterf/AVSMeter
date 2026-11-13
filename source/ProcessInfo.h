#ifndef _PROCESSINFO_H
#define _PROCESSINFO_H

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

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

	DWORD dwLastTimeRun;
	DWORD dwLastTimeRunThreadCount;
	HANDLE hCurProcess;
	PROCESS_MEMORY_COUNTERS pmc;
};


CProcessInfo::CProcessInfo()
{
	hCurProcess = ::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, ::GetCurrentProcessId());

	dwPhysicalMemMB = 0;
	dwVirtualMemMB = 0;

	::ZeroMemory(&ftPrevSysKernel, sizeof(FILETIME));
	::ZeroMemory(&ftPrevSysUser, sizeof(FILETIME));
	::ZeroMemory(&ftPrevProcKernel, sizeof(FILETIME));
	::ZeroMemory(&ftPrevProcUser, sizeof(FILETIME));
	
	btCPUUsage = 0;
	wThreadCount = GetCurrentThreadCount();
	bFirstRun = TRUE;

	dwLastTimeRun = 0;
	dwLastTimeRunThreadCount = 0;
}

CProcessInfo::~CProcessInfo()
{
}


void CProcessInfo::CloseProcess()
{
	if (hCurProcess)
		::CloseHandle(hCurProcess);

	return;
}


void CProcessInfo::Update()
{
	if ((::GetTickCount() - dwLastTimeRun) < 100)
		return;

	GetCPUUsage();
	bFirstRun = FALSE;

	if ((btCPUUsage < 1) || (btCPUUsage > 100))
		wCPUUsage = 0;
	else
		wCPUUsage = (WORD)btCPUUsage;

	if ((::GetTickCount() - dwLastTimeRunThreadCount) > 1000)
	{
		wThreadCount = GetCurrentThreadCount();
		dwLastTimeRunThreadCount = GetTickCount();
	}

	::GetProcessMemoryInfo(hCurProcess, &pmc, sizeof(pmc));
	dwPhysicalMemMB = (DWORD)(((double)(pmc.WorkingSetSize) / 1048576.0) + 0.5);
	dwVirtualMemMB = (DWORD)(((double)(pmc.PagefileUsage) / 1048576.0) + 0.5);

	dwLastTimeRun = GetTickCount();

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
	LARGE_INTEGER a, b;
	a.LowPart = ftA.dwLowDateTime;
	a.HighPart = ftA.dwHighDateTime;
	b.LowPart = ftB.dwLowDateTime;
	b.HighPart = ftB.dwHighDateTime;

	return (unsigned __int64)(a.QuadPart - b.QuadPart);
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


#endif //_PROCESSINFO_H

