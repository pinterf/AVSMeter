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


// Internal to the POSIX platform implementation.

#if !defined(_PLATFORM_LAST_ERROR_POSIX_H)
#define _PLATFORM_LAST_ERROR_POSIX_H

#include <string>

namespace platform::posix
{
  // dlopen/dlsym report errors via dlerror(), not errno: the message is kept per thread for
  // LastErrorMessage(). Empty: the last call succeeded.
  void               SetLastDlError(const std::string &s_error);
  const std::string &LastDlError();
}


#endif //_PLATFORM_LAST_ERROR_POSIX_H
