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

#if !defined(_PLATFORM_CONSOLE_POSIX_H)
#define _PLATFORM_CONSOLE_POSIX_H

namespace platform::posix
{
  // Puts stdin back into the mode it had before KeyPressed/ReadKey. Async-signal-safe.
  void RestoreConsoleInput();
}


#endif //_PLATFORM_CONSOLE_POSIX_H
