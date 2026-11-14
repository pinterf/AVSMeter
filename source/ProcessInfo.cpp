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
  dwMemMB = 0;
  prevTimes = {};
  dCPUUsage = 0.0;
  wThreadCount = (uint16_t)platform::ProcessThreadCount();
  bFirstRun = true;
  lRunCount = 0;
}

CProcessInfo::~CProcessInfo()
{
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

  dwMemMB = (uint32_t)(((double)platform::ProcessWorkingSetBytes() / 1048576.0) + 0.5);

  //Thread count should only be updated every few seconds to reduce overhead
  if ((GetSTDTimer() - uiLastTimeRunTH) < 5000)
    return;

  wThreadCount = (uint16_t)platform::ProcessThreadCount();
  uiLastTimeRunTH = GetSTDTimer();

  return;
}


void CProcessInfo::GetCPUUsage()
{
  if (++lRunCount == 1)
  {
    platform::CpuTimes times;
    if (!platform::GetCpuTimes(times))
    {
      --lRunCount;
      return;
    }

    if (!bFirstRun)
    {
      /*
      CPU usage is calculated by getting the total amount of time the system
      has operated since the last measurement (made up of kernel + user) and
      the total amount of time the process has run (kernel + user).
      */
      uint64_t uiSysDiff = times.system - prevTimes.system;
      uint64_t uiProcDiff = times.process - prevTimes.process;

      if (uiSysDiff > 0)
        dCPUUsage = (100.0 * (double)uiProcDiff) / (double)uiSysDiff;
    }

    prevTimes = times;
  }

  --lRunCount;

  return;
}


uint64_t CProcessInfo::GetSTDTimer()
{
  using namespace std::chrono;
  return (uint64_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
