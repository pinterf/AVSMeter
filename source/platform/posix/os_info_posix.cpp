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

#include <sys/utsname.h>


// Phase 2: distribution name from /etc/os-release
std::string platform::OsVersion()
{
  struct utsname un;
  if (uname(&un) != 0)
    return "Unknown OS";

  return std::string(un.sysname) + " " + un.release + " (" + un.machine + ")";
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
