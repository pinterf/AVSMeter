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


#if !defined(_PLATFORM_TEXT_H)
#define _PLATFORM_TEXT_H

#include <string>

namespace platform
{
  // Text from outside (INI, scripts, AviSynth messages) as UTF-8: valid UTF-8 is
  // kept, anything else is read in the legacy 8 bit encoding (Windows: ANSI code page).
  std::string ToUtf8(const std::string &s_text);
}


#endif //_PLATFORM_TEXT_H
