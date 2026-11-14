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


#include "platform/file_info.h"
#include "platform/win32/utf_win32.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <vector>


namespace
{
  bool GetAttributeData(const std::string &s_path, WIN32_FILE_ATTRIBUTE_DATA &fad)
  {
    std::wstring wsPath = platform::win32::ToWide(s_path.c_str());
    return ::GetFileAttributesExW(wsPath.c_str(), GetFileExInfoStandard, &fad) != FALSE;
  }


  bool GetFixedVersionInfo(const std::string &s_path, VS_FIXEDFILEINFO &ffi)
  {
    std::wstring wsPath;
    if (s_path.empty())
    {
      wsPath = platform::win32::ModuleFileNameW(NULL);
      if (wsPath.empty())
        return false;
    }
    else
      wsPath = platform::win32::ToWide(s_path.c_str());

    DWORD dwDummy;
    DWORD dwSize = ::GetFileVersionInfoSizeW(wsPath.c_str(), &dwDummy);
    if (dwSize == 0)
      return false;

    std::vector<BYTE> vVersionInfo(dwSize);
    if (!::GetFileVersionInfoW(wsPath.c_str(), 0, dwSize, vVersionInfo.data()))
      return false;

    UINT uiLen;
    VS_FIXEDFILEINFO *pFixedInfo = nullptr;
    if (!::VerQueryValueW(vVersionInfo.data(), L"\\", (LPVOID *)&pFixedInfo, &uiLen) || !pFixedInfo)
      return false;

    ffi = *pFixedInfo;
    return true;
  }


  std::string FormatVersion(DWORD dw_ms, DWORD dw_ls)
  {
    char szBuf[64];
    snprintf(szBuf, sizeof(szBuf), "%u.%u.%u.%u", HIWORD(dw_ms), LOWORD(dw_ms), HIWORD(dw_ls), LOWORD(dw_ls));
    return szBuf;
  }


  std::string AsciiLower(std::string s_string)
  {
    std::transform(s_string.begin(), s_string.end(), s_string.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s_string;
  }
}


int64_t platform::FileSize(const std::string &s_path)
{
  WIN32_FILE_ATTRIBUTE_DATA fad;
  if (!GetAttributeData(s_path, fad))
    return -1;

  return (((int64_t)fad.nFileSizeHigh) << 32) + fad.nFileSizeLow;
}


bool platform::FileExists(const std::string &s_path)
{
  DWORD dwAttrib = ::GetFileAttributesW(win32::ToWide(s_path.c_str()).c_str());
  return (dwAttrib != INVALID_FILE_ATTRIBUTES) && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY);
}


bool platform::DirectoryExists(const std::string &s_path)
{
  DWORD dwAttrib = ::GetFileAttributesW(win32::ToWide(s_path.c_str()).c_str());
  return (dwAttrib != INVALID_FILE_ATTRIBUTES) && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY);
}


bool platform::FileModifiedUtc(const std::string &s_path, std::tm &tm_utc)
{
  WIN32_FILE_ATTRIBUTE_DATA fad;
  SYSTEMTIME st;
  if (!GetAttributeData(s_path, fad) || !::FileTimeToSystemTime(&fad.ftLastWriteTime, &st))
    return false;

  tm_utc = std::tm();
  tm_utc.tm_year = st.wYear - 1900;
  tm_utc.tm_mon = st.wMonth - 1;
  tm_utc.tm_mday = st.wDay;
  tm_utc.tm_hour = st.wHour;
  tm_utc.tm_min = st.wMinute;
  tm_utc.tm_sec = st.wSecond;
  tm_utc.tm_wday = st.wDayOfWeek;
  return true;
}


std::string platform::FileVersion(const std::string &s_path)
{
  VS_FIXEDFILEINFO ffi;
  if (!GetFixedVersionInfo(s_path, ffi))
    return "n/a";

  return FormatVersion(ffi.dwFileVersionMS, ffi.dwFileVersionLS);
}


std::string platform::ProductVersion(const std::string &s_path)
{
  VS_FIXEDFILEINFO ffi;
  if (!GetFixedVersionInfo(s_path, ffi))
    return "n/a";

  return FormatVersion(ffi.dwProductVersionMS, ffi.dwProductVersionLS);
}


std::string platform::NativeSystemPath(const std::string &s_path)
{
  BOOL bWoW64Process = FALSE;
  if (!::IsWow64Process(::GetCurrentProcess(), &bWoW64Process) || !bWoW64Process)
    return s_path;

  // ANSI API: the path keeps the encoding of the caller, only the system folder prefix is replaced.
  char szWinDir[MAX_PATH + 1];
  UINT uiLen = ::GetWindowsDirectoryA(szWinDir, sizeof(szWinDir));
  if ((uiLen == 0) || (uiLen >= sizeof(szWinDir)))
    return s_path;

  std::string sSystem32 = AsciiLower(std::string(szWinDir) + "\\system32\\");
  if (AsciiLower(s_path.substr(0, sSystem32.length())) != sSystem32)
    return s_path;

  char szSysWOW64Dir[MAX_PATH + 1];
  uiLen = ::GetSystemWow64DirectoryA(szSysWOW64Dir, sizeof(szSysWOW64Dir));
  if ((uiLen <= 1) || (uiLen >= sizeof(szSysWOW64Dir)))
    return s_path;

  return std::string(szSysWOW64Dir) + "\\" + s_path.substr(sSystem32.length());
}
