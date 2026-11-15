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


#if !defined(_PLATFORM_OS_INFO_H)
#define _PLATFORM_OS_INFO_H

#include <string>

namespace platform
{
  std::string OsVersion(); // name, architecture and build, for the logs
  bool        Is64BitOS();

  // CPU details from the OS, for when libcpuid cannot identify the CPU (libcpuid 0.8.2 reads the
  // ARM ID registers on Linux and FreeBSD only). Implemented for Windows on ARM64, false elsewhere.
  struct OsCpuInfo
  {
    std::string brand;
    int         cores = 0;
    int         logical_cores = 0;
    std::string instruction_sets; // "NEON, DOTPROD, ...", the names of the libcpuid ARM list
  };
  bool        GetOsCpuInfo(OsCpuInfo &info);
}


#endif //_PLATFORM_OS_INFO_H
