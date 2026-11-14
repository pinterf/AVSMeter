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

#include <atomic>
#include <cstdint>

#include "platform/process_stats.h"

class CProcessInfo
{
public:
  CProcessInfo();
  virtual ~CProcessInfo();

  void     Update();
  double   dCPUUsage;
  uint16_t wThreadCount;
  uint32_t dwMemMB;

private:
  void GetCPUUsage();
  bool bFirstRun;
  uint64_t GetSTDTimer();

  platform::CpuTimes prevTimes;
  std::atomic<long> lRunCount;
};


#endif //_PROCESSINFO_H
