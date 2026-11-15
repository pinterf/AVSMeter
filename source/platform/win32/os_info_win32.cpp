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
#include "platform/win32/utf_win32.h"

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

  const wchar_t *pwszCurrentVersionKey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

  std::string RegString(const wchar_t *pwsz_value)
  {
    wchar_t wszBuf[64] = L"";
    DWORD dwSize = sizeof(wszBuf);
    if (::RegGetValueW(HKEY_LOCAL_MACHINE, pwszCurrentVersionKey, pwsz_value, RRF_RT_REG_SZ, NULL, wszBuf, &dwSize) != ERROR_SUCCESS)
      return "";

    return platform::win32::WideToUtf8(wszBuf);
  }

  bool RegDword(const wchar_t *pwsz_value, DWORD &dw_value)
  {
    DWORD dwSize = sizeof(dw_value);
    return ::RegGetValueW(HKEY_LOCAL_MACHINE, pwszCurrentVersionKey, pwsz_value, RRF_RT_REG_DWORD, NULL, &dw_value, &dwSize) == ERROR_SUCCESS;
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
    {
      // Windows 11 and the servers since 2016 still report 10.0; the build tells them apart
      const char *pszName = "Windows 10";
      if (osvi.wProductType == VER_NT_WORKSTATION)
      {
        if (osvi.dwBuildNumber >= 22000)
          pszName = "Windows 11";
      }
      else
      {
        if (osvi.dwBuildNumber >= 26100)
          pszName = "Windows Server 2025";
        else if (osvi.dwBuildNumber >= 20348)
          pszName = "Windows Server 2022";
        else if (osvi.dwBuildNumber >= 17763)
          pszName = "Windows Server 2019";
        else
          pszName = "Windows Server 2016";
      }
      sOSVersion = std::string(pszName) + " " + sArchitecture;
    }
  }

  // Feature update (e.g. 24H2) and update revision as shown by winver.
  // DisplayVersion since 20H2; ReleaseId before (it stays "2009" after).
  if (osvi.dwMajorVersion == 10)
  {
    std::string sDisplayVersion = RegString(L"DisplayVersion");
    if (sDisplayVersion == "")
      sDisplayVersion = RegString(L"ReleaseId");
    if (sDisplayVersion != "")
      sOSVersion += " " + sDisplayVersion;
  }

  std::string sBuild = StrFormat("%u", (unsigned int)osvi.dwBuildNumber);
  DWORD dwUBR = 0;
  if (RegDword(L"UBR", dwUBR))
    sBuild += StrFormat(".%u", (unsigned int)dwUBR);

  if ((osvi.wServicePackMajor > 0) || (osvi.wServicePackMinor > 0))
    sOSAddendum = StrFormat(" Service Pack %u.%u (Build %s)", (unsigned int)osvi.wServicePackMajor, (unsigned int)osvi.wServicePackMinor, sBuild.c_str());
  else
    sOSAddendum = " (Build " + sBuild + ")";

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
