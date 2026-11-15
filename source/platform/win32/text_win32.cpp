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


#include "platform/text.h"
#include "platform/win32/utf_win32.h"


std::string platform::ToUtf8(const std::string &s_text)
{
  if (s_text.empty() || !win32::Utf8ToWide(s_text.c_str()).empty())
    return s_text;

  return win32::WideToUtf8(win32::AnsiToWide(s_text.c_str()).c_str());
}
