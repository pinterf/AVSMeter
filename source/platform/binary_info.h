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


#if !defined(_PLATFORM_BINARY_INFO_H)
#define _PLATFORM_BINARY_INFO_H

#include <string>
#include <vector>

namespace platform
{
  // Shared library inspected from the file, without loading it.
  struct BinaryInfo
  {
    std::string              path;    // resolved full path
    bool                     is64Bit = false;
    std::vector<std::string> exports; // exported symbol names
    std::vector<std::string> imports; // names of the libraries it depends on
  };

  // A name without path is searched like the OS library loader does.
  // false: not found or not a valid binary, the reason is in the last system error.
  bool InspectBinary(const std::string &s_name, BinaryInfo &info);

  // Visual C++ 2005/2008 runtimes live in WinSxS: test-loading them by name fails even when installed.
  bool IsVCRuntimeInstalled(const std::string &s_year, bool b_64bit);
}


#endif //_PLATFORM_BINARY_INFO_H
