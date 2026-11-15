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


#include "platform/os_info.h"

#include <cstring>
#include <fstream>
#include <map>

#include <sys/utsname.h>


namespace
{
  // os-release(5): KEY=value lines, value optionally quoted with shell-style escapes
  std::string DistributionName()
  {
    std::ifstream file("/etc/os-release");
    if (!file)
      file.open("/usr/lib/os-release");
    if (!file)
      return "";

    std::map<std::string, std::string> values;
    std::string sLine;
    while (std::getline(file, sLine))
    {
      size_t pos = sLine.find('=');
      if (pos == std::string::npos || sLine[0] == '#')
        continue;

      std::string sValue = sLine.substr(pos + 1);
      if (!sValue.empty() && sValue.back() == '\r')
        sValue.pop_back();
      if (sValue.size() >= 2 && (sValue[0] == '"' || sValue[0] == '\'') && sValue.back() == sValue[0])
      {
        std::string sUnquoted;
        for (size_t i = 1; i + 1 < sValue.size(); i++)
        {
          if (sValue[i] == '\\' && i + 2 < sValue.size())
            i++;
          sUnquoted += sValue[i];
        }
        sValue = sUnquoted;
      }
      values[sLine.substr(0, pos)] = sValue;
    }

    if (!values["PRETTY_NAME"].empty())
      return values["PRETTY_NAME"];

    std::string sName = values["NAME"].empty() ? "Linux" : values["NAME"];
    return values["VERSION"].empty() ? sName : sName + " " + values["VERSION"];
  }
}


// e.g. "Ubuntu 26.04.1 LTS, Linux 6.18.58.1-microsoft-standard-WSL2 (x86_64)"
std::string platform::OsVersion()
{
  std::string sDistribution = DistributionName();

  struct utsname un;
  if (uname(&un) != 0)
    return sDistribution.empty() ? "Unknown OS" : sDistribution;

  std::string sKernel = std::string(un.sysname) + " " + un.release + " (" + un.machine + ")";
  return sDistribution.empty() ? sKernel : sDistribution + ", " + sKernel;
}


bool platform::Is64BitOS()
{
  if (sizeof(void *) == 8)
    return true;

  struct utsname un;
  if (uname(&un) != 0)
    return false;

  return (std::strstr(un.machine, "64") != nullptr);
}


bool platform::GetOsCpuInfo(OsCpuInfo &)
{
  return false;
}
