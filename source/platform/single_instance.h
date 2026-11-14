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


#if !defined(_PLATFORM_SINGLE_INSTANCE_H)
#define _PLATFORM_SINGLE_INSTANCE_H

namespace platform
{
  // Takes the named system-wide lock for the rest of the process lifetime.
  // Returns false only if another process already holds it.
  bool AcquireSingleInstance(const char *psz_name);
}


#endif //_PLATFORM_SINGLE_INSTANCE_H
