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
*/


#include "platform/process_stats.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>


namespace
{
  uint64_t ToU64(const FILETIME &ft)
  {
    return (((uint64_t)ft.dwHighDateTime) << 32) + (uint64_t)ft.dwLowDateTime;
  }
}


bool platform::GetCpuTimes(CpuTimes &times)
{
  FILETIME ftSysIdle, ftSysKernel, ftSysUser, ftProcCreation, ftProcExit, ftProcKernel, ftProcUser;
  if (!::GetSystemTimes(&ftSysIdle, &ftSysKernel, &ftSysUser) || !::GetProcessTimes(::GetCurrentProcess(), &ftProcCreation, &ftProcExit, &ftProcKernel, &ftProcUser))
    return false;

  // System kernel time includes idle time
  times.system = ToU64(ftSysKernel) + ToU64(ftSysUser);
  times.process = ToU64(ftProcKernel) + ToU64(ftProcUser);

  return true;
}


uint64_t platform::ProcessWorkingSetBytes()
{
  PROCESS_MEMORY_COUNTERS pmc = {};
  if (!::GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc)))
    return 0;

  return (uint64_t)pmc.WorkingSetSize;
}


unsigned platform::ProcessThreadCount()
{
  DWORD dwPID = ::GetCurrentProcessId();
  HANDLE hThreadSnapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (hThreadSnapshot == INVALID_HANDLE_VALUE)
    return 0;

  THREADENTRY32 te32;
  te32.dwSize = sizeof(THREADENTRY32);

  if (!::Thread32First(hThreadSnapshot, &te32))
  {
    ::CloseHandle(hThreadSnapshot);
    return 0;
  }

  unsigned uiThreads = 0;
  while (::Thread32Next(hThreadSnapshot, &te32))
  {
    if (te32.th32OwnerProcessID == dwPID)
      ++uiThreads;
  }

  ::CloseHandle(hThreadSnapshot);

  return uiThreads;
}
