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


#include "platform/os_info.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdarg>
#include <cstdio>


namespace
{
  std::string StrFormat(const char *psz_fmt, ...)
  {
    char szBuf[256];
    va_list args;
    va_start(args, psz_fmt);
    vsnprintf(szBuf, sizeof(szBuf), psz_fmt, args);
    va_end(args);

    return szBuf;
  }
}


std::string platform::OsVersion()
{
  typedef LONG NTSTATUS, *PNTSTATUS;
  typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOEXW);
  std::string sOSVersion = "Unknown OS Version";
  std::string sOSAddendum = "";

  std::string sArchitecture = "(x86)";
  if (Is64BitOS())
    sArchitecture = "(x64)";

  HMODULE hMod = ::GetModuleHandleA("ntdll.dll");
  if (!hMod)
    return "Cannot get module handle to ntdll.dll";

  RtlGetVersionPtr rgvPtr = (RtlGetVersionPtr)::GetProcAddress(hMod, "RtlGetVersion");
  if (rgvPtr == NULL)
    return "Cannot invoke \"RtlGetVersion\"";

  RTL_OSVERSIONINFOEXW osvi = {0};
  osvi.dwOSVersionInfoSize = sizeof(osvi);
  if (rgvPtr(&osvi) != 0)
    return "Cannot invoke \"RtlGetVersion\"";

  if (osvi.dwMajorVersion == 5)
  {
    if (osvi.wProductType == VER_NT_WORKSTATION)
      sOSVersion = "Windows XP " + sArchitecture;
    else
      sOSVersion = "Windows Server 2003 " + sArchitecture;
  }

  if (osvi.dwMajorVersion == 6)
  {
    if (osvi.dwMinorVersion == 0)
    {
      if (osvi.wProductType == VER_NT_WORKSTATION)
        sOSVersion = "Windows Vista " + sArchitecture;
      else
        sOSVersion = "Windows Server 2008 " + sArchitecture;
    }
    if (osvi.dwMinorVersion == 1)
    {
      if (osvi.wProductType == VER_NT_WORKSTATION)
        sOSVersion = "Windows 7 " + sArchitecture;
      else
        sOSVersion = "Windows Server 2008 R2 " + sArchitecture;
    }
    if (osvi.dwMinorVersion == 2)
    {
      if (osvi.wProductType == VER_NT_WORKSTATION)
        sOSVersion = "Windows 8 " + sArchitecture;
      else
        sOSVersion = "Windows Server 2012 " + sArchitecture;
    }
    if (osvi.dwMinorVersion == 3)
    {
      if (osvi.wProductType == VER_NT_WORKSTATION)
        sOSVersion = "Windows 8.1 " + sArchitecture;
      else
        sOSVersion = "Windows Server 2012 R2 " + sArchitecture;
    }
  }

  if (osvi.dwMajorVersion == 10)
  {
    if (osvi.dwMinorVersion == 0)
      sOSVersion = "Windows 10 " + sArchitecture;
  }

  if ((osvi.wServicePackMajor > 0) || (osvi.wServicePackMinor > 0))
    sOSAddendum = StrFormat(" Service Pack %u.%u (Build %u)", (unsigned int)osvi.wServicePackMajor, (unsigned int)osvi.wServicePackMinor, (unsigned int)osvi.dwBuildNumber);
  else
    sOSAddendum = StrFormat(" (Build %u)", (unsigned int)osvi.dwBuildNumber);

  return sOSVersion + sOSAddendum;
}


bool platform::Is64BitOS()
{
  if (sizeof(void*) == 8)
    return true; //64 on 64

  BOOL bWoW64Process = FALSE;
  ::IsWow64Process(::GetCurrentProcess(), &bWoW64Process);

  return bWoW64Process != FALSE;
}
