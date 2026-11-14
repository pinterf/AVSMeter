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


#if !defined(_PROCESSINFO_H)
#define _PROCESSINFO_H

#include "common.h"

class CProcessInfo
{
public:
  CProcessInfo();
  virtual ~CProcessInfo();

  void   Update();
  void   CloseProcess();
  double dCPUUsage;
  WORD   wThreadCount;
  DWORD  dwMemMB;

private:
  WORD GetCurrentThreadCount();
  void GetCPUUsage();
  uint64_t SubtractTimes(const FILETIME& ftA, const FILETIME& ftB);
  bool bFirstRun;
  uint64_t GetSTDTimer();

  FILETIME ftPrevSysKernel;
  FILETIME ftPrevSysUser;
  FILETIME ftPrevProcKernel;
  FILETIME ftPrevProcUser;

  HANDLE hProcess;
  PROCESS_MEMORY_COUNTERS pmc;
  volatile LONG lRunCount;
};


#endif //_PROCESSINFO_H

