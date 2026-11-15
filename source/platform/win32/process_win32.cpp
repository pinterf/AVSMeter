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


#include "platform/process.h"
#include "platform/win32/utf_win32.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>


bool platform::SetHighPriority()
{
  return ::SetPriorityClass(::GetCurrentProcess(), HIGH_PRIORITY_CLASS) != FALSE;
}


std::vector<std::string> platform::CommandLineArgs(int argc, char *argv[])
{
  std::vector<std::string> vArgs;

  int iArgs = 0;
  LPWSTR *ppwszArgs = ::CommandLineToArgvW(::GetCommandLineW(), &iArgs);
  if (ppwszArgs)
  {
    for (int i = 0; i < iArgs; i++)
      vArgs.push_back(win32::WideToUtf8(ppwszArgs[i]));
    ::LocalFree(ppwszArgs);
    return vArgs;
  }

  for (int i = 0; i < argc; i++)
    vArgs.push_back(win32::WideToUtf8(win32::AnsiToWide(argv[i]).c_str()));

  return vArgs;
}
