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


#if !defined(_SYSINFO_H)
#define _SYSINFO_H

#include "common.h"
#include "cpuid/libcpuid.h"
#include "Utility.h"

class CSysInfo
{
public:
  CSysInfo();
  virtual        ~CSysInfo();
  std::string    GetOSVersion();
  std::string    GetFormattedSystemDateTime();

  //CPUID
  void           GetCPUID();
  bool           bCPUIDSuccess;
  struct CPUData
  {
    std::string  CPUBrandString;
    std::string  CPUCodeName;
    int32_t      CPUCores;
    int32_t      CPULogicalCores;
    int32_t      NumCPUs;
    int32_t      CPUTotalLogicalCores;
    std::string  CPUSupportedInstructionSets;
  } cpudata;

private:
  CUtils         utils;
  std::string    RemoveMultipleWhiteSpaces(std::string s);
};


#endif //_SYSINFO_H
