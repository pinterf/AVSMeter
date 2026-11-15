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


#include "platform/process.h"
#include "platform/posix/last_error_posix.h"

#include <cerrno>
#include <cstring>

#include <sys/resource.h>


namespace
{
  thread_local std::string tl_sDlError;
}


void platform::posix::SetLastDlError(const std::string &s_error)
{
  tl_sDlError = s_error;
}


const std::string &platform::posix::LastDlError()
{
  return tl_sDlError;
}


// A negative nice value needs CAP_SYS_NICE (or root), otherwise EACCES.
bool platform::SetHighPriority()
{
  return setpriority(PRIO_PROCESS, 0, -10) == 0;
}


std::vector<std::string> platform::CommandLineArgs(int argc, char *argv[])
{
  return std::vector<std::string>(argv, argv + argc);
}


void platform::SuppressErrorDialogs()
{
}


std::string platform::LastErrorMessage()
{
  if (!posix::LastDlError().empty())
    return posix::LastDlError();

  if (errno == 0)
    return "";

  return std::strerror(errno);
}
