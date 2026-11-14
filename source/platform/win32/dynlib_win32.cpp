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


#include "platform/dynlib.h"
#include "platform/win32/utf_win32.h"

#include <algorithm>


platform::LibHandle platform::LoadLib(const char *psz_name)
{
  // Wide API: a non-ASCII folder may not fit the ANSI code page.
  // LOAD_WITH_ALTERED_SEARCH_PATH needs backslashes.
  std::wstring wsName = win32::ToWide(psz_name);
  std::replace(wsName.begin(), wsName.end(), L'/', L'\\');

  return (LibHandle)::LoadLibraryExW(wsName.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
}


void *platform::GetLibSymbol(LibHandle h_lib, const char *psz_symbol)
{
  return (void *)::GetProcAddress((HMODULE)h_lib, psz_symbol);
}


bool platform::FreeLib(LibHandle h_lib)
{
  return ::FreeLibrary((HMODULE)h_lib) != FALSE;
}
