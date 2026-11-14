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


#if !defined(_PLATFORM_CRASH_GUARD_H)
#define _PLATFORM_CRASH_GUARD_H

namespace platform
{
  // From now on, a hardware exception (access violation, division by zero, ...)
  // on the calling thread throws a std::exception describing the fault and the
  // module it happened in. Per thread; no-op where unsupported.
  //
  // test STATUS_FLOAT_DIVIDE_BY_ZERO:
  //   int a = 0;
  //   int b = 100;
  //   volatile float c = b/a;
  //
  // test STATUS_ACCESS_VIOLATION:
  //   volatile int *pInt = 0x00000000;
  //   *pInt = 20;
  void TranslateCrashes();
}


#endif //_PLATFORM_CRASH_GUARD_H
