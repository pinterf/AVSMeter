/*
  This file is part of AVSMeter, (c) 2026 pinterf.

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


// Phase 2 stub: /proc/stat, /proc/self/stat and /proc/self/status
bool platform::GetCpuTimes(CpuTimes &times)
{
  times = CpuTimes();
  return false;
}


uint64_t platform::ProcessWorkingSetBytes()
{
  return 0;
}


unsigned platform::ProcessThreadCount()
{
  return 0;
}
