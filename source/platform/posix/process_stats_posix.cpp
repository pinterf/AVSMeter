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

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>


namespace
{
  // Fields of /proc/self/stat after the command name, which may contain spaces and ')'.
  // Index 0 is field 3 (state) of proc(5).
  bool ReadSelfStat(std::vector<std::string> &v_fields)
  {
    std::ifstream file("/proc/self/stat");
    std::string sLine;
    if (!std::getline(file, sLine))
      return false;

    size_t nPos = sLine.rfind(')');
    if (nPos == std::string::npos)
      return false;

    std::istringstream fields(sLine.substr(nPos + 1));
    v_fields.clear();
    for (std::string sField; fields >> sField;)
      v_fields.push_back(sField);

    return v_fields.size() > 17;
  }
}


// Both /proc/stat and /proc/self/stat count in clock ticks (USER_HZ).
bool platform::GetCpuTimes(CpuTimes &times)
{
  times = CpuTimes();

  std::ifstream stat("/proc/stat");
  std::string sLabel;
  if (!(stat >> sLabel) || sLabel != "cpu")
    return false;

  // user nice system idle iowait irq softirq steal; guest and guest_nice are already in user and nice
  uint64_t uiSystem = 0;
  for (int i = 0; i < 8; i++)
  {
    uint64_t uiValue = 0;
    if (!(stat >> uiValue))
      break;
    uiSystem += uiValue;
  }

  std::vector<std::string> vFields;
  if (uiSystem == 0 || !ReadSelfStat(vFields))
    return false;

  times.system = uiSystem;
  times.process = std::strtoull(vFields[11].c_str(), nullptr, 10) + std::strtoull(vFields[12].c_str(), nullptr, 10); // utime + stime

  return true;
}


uint64_t platform::ProcessWorkingSetBytes()
{
  std::ifstream statm("/proc/self/statm");
  uint64_t uiSize = 0, uiResident = 0;
  if (!(statm >> uiSize >> uiResident))
    return 0;

  return uiResident * (uint64_t)sysconf(_SC_PAGESIZE);
}


unsigned platform::ProcessThreadCount()
{
  std::vector<std::string> vFields;
  if (!ReadSelfStat(vFields))
    return 0;

  return (unsigned)std::strtoul(vFields[17].c_str(), nullptr, 10); // num_threads
}
