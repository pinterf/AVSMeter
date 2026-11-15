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


#include "platform/binary_info.h"

#include <cerrno>


// Phase 2 stub: ELF inspection (class, DT_NEEDED, dynamic symbols) comes with -avsinfo on Linux.
bool platform::InspectBinary(const std::string &s_name, BinaryInfo &info)
{
  (void)s_name;
  info = BinaryInfo();
  errno = ENOSYS;
  return false;
}


bool platform::IsVCRuntimeInstalled(const std::string &s_year, bool b_64bit)
{
  (void)s_year;
  (void)b_64bit;
  return false;
}
