/*
  This file is part of AVSMeter, (c) Groucho2004.

  AVSMeter is free software. You can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, either
  version 3 of the License, or any later version.

  AVSMeter is distributed in the hope that it will be useful
  but WITHOUT ANY WARRANTY and without the implied warranty
  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with AVSMeter. If not, see <http://www.gnu.org/licenses/>.

  Windows part for CPU usage by (c) 2009 Ben Watson derived from
  http://www.philosophicalgeek.com/2009/01/03/determine-cpu-usage-of-current-process-c-and-c/
*/


#include "ProcessInfo.h"

#include <chrono>


CProcessInfo::CProcessInfo()
{
  hProcess = ::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, ::GetCurrentProcessId());

  dwMemMB = 0;

  ::ZeroMemory(&ftPrevSysKernel, sizeof(FILETIME));
  ::ZeroMemory(&ftPrevSysUser, sizeof(FILETIME));
  ::ZeroMemory(&ftPrevProcKernel, sizeof(FILETIME));
  ::ZeroMemory(&ftPrevProcUser, sizeof(FILETIME));

  dCPUUsage = 0.0;
  wThreadCount = GetCurrentThreadCount();
  bFirstRun = true;
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
  static uint64_t uiLastTimeRun;
  static uint64_t uiLastTimeRunTH;

  if (((GetSTDTimer() - uiLastTimeRun) < 500) && !bFirstRun)
    return;

  uiLastTimeRun = GetSTDTimer();

  GetCPUUsage();
  bFirstRun = false;

  if (dCPUUsage >= 100.0)
    dCPUUsage = 99.99999999;

  ::GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc));
  dwMemMB = (DWORD)(((double)(pmc.WorkingSetSize) / 1048576.0) + 0.5);

  //Thread count should only be updated every few seconds to reduce overhead
  if ((GetSTDTimer() - uiLastTimeRunTH) < 5000)
    return;

  wThreadCount = GetCurrentThreadCount();
  uiLastTimeRunTH = GetSTDTimer();

  return;
}


void CProcessInfo::GetCPUUsage()
{
  if (::InterlockedIncrement(&lRunCount) == 1)
  {
    FILETIME ftSysIdle, ftSysKernel, ftSysUser, ftProcCreation, ftProcExit, ftProcKernel, ftProcUser;
    if (!::GetSystemTimes(&ftSysIdle, &ftSysKernel, &ftSysUser) || !::GetProcessTimes(::GetCurrentProcess(), &ftProcCreation, &ftProcExit, &ftProcKernel, &ftProcUser))
    {
      ::InterlockedDecrement(&lRunCount);
      return;
    }

    if (!bFirstRun)
    {
      /*
      CPU usage is calculated by getting the total amount of time the system
      has operated since the last measurement (made up of kernel + user) and
      the total amount of time the process has run (kernel + user).
      */
      uint64_t uiSysKernelDiff = SubtractTimes(ftSysKernel, ftPrevSysKernel);
      uint64_t uiSysUserDiff = SubtractTimes(ftSysUser, ftPrevSysUser);
      uint64_t uiProcKernelDiff = SubtractTimes(ftProcKernel, ftPrevProcKernel);
      uint64_t uiProcUserDiff = SubtractTimes(ftProcUser, ftPrevProcUser);

      if ((uiSysKernelDiff + uiSysUserDiff) > 0)
        dCPUUsage = (100.0 * (double)(uiProcKernelDiff + uiProcUserDiff)) / (double)(uiSysKernelDiff + uiSysUserDiff);
    }

    ftPrevSysKernel = ftSysKernel;
    ftPrevSysUser = ftSysUser;
    ftPrevProcKernel = ftProcKernel;
    ftPrevProcUser = ftProcUser;
  }

  ::InterlockedDecrement(&lRunCount);

  return;
}


uint64_t CProcessInfo::SubtractTimes(const FILETIME& ftA, const FILETIME& ftB)
{
  uint64_t a = (((uint64_t)ftA.dwHighDateTime) << 32) + (uint64_t)ftA.dwLowDateTime;
  uint64_t b = (((uint64_t)ftB.dwHighDateTime) << 32) + (uint64_t)ftB.dwLowDateTime;

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


uint64_t CProcessInfo::GetSTDTimer()
{
  using namespace std::chrono;
  return (uint64_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
