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


// Internal to the Win32 platform implementation.
// Conversions follow AviSynth+ (avs_core/core/strings.cpp): "ANSI" is the code
// page of the ANSI file APIs (ACP, or OEMCP after SetFileApisToOEM), sizes are
// queried first (DBCS and UTF-8 may need more bytes than wide chars), and
// characters missing from an ANSI code page become '?'.

#if !defined(_PLATFORM_UTF_WIN32_H)
#define _PLATFORM_UTF_WIN32_H

#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace platform::win32
{
  inline UINT AnsiCodePage()
  {
    return ::AreFileApisANSI() ? CP_ACP : CP_OEMCP;
  }

  // Empty on invalid input (MB_ERR_INVALID_CHARS)
  inline std::wstring CodePageToWide(UINT ui_cp, DWORD dw_flags, const char *psz)
  {
    int iLen = ::MultiByteToWideChar(ui_cp, dw_flags, psz, -1, NULL, 0);
    if (iLen <= 0)
      return std::wstring();

    std::wstring ws((size_t)iLen, L'\0');
    ::MultiByteToWideChar(ui_cp, dw_flags, psz, -1, &ws[0], iLen);
    ws.resize((size_t)iLen - 1);

    return ws;
  }

  inline std::string WideToCodePage(UINT ui_cp, const wchar_t *pwsz)
  {
    int iLen = ::WideCharToMultiByte(ui_cp, 0, pwsz, -1, NULL, 0, NULL, NULL);
    if (iLen <= 0)
      return std::string();

    std::string s((size_t)iLen, '\0');
    ::WideCharToMultiByte(ui_cp, 0, pwsz, -1, &s[0], iLen, NULL, NULL);
    s.resize((size_t)iLen - 1);

    return s;
  }

  inline std::wstring Utf8ToWide(const char *psz)    { return CodePageToWide(CP_UTF8, MB_ERR_INVALID_CHARS, psz); }
  inline std::wstring AnsiToWide(const char *psz)    { return CodePageToWide(AnsiCodePage(), 0, psz); }
  inline std::string  WideToUtf8(const wchar_t *pwsz) { return WideToCodePage(CP_UTF8, pwsz); }
  inline std::string  WideToAnsi(const wchar_t *pwsz) { return WideToCodePage(AnsiCodePage(), pwsz); }

  // Platform API strings are UTF-8, as in AviSynth+. Input that is not valid
  // UTF-8 is read as ANSI: AVSMeter still has ANSI strings (main(), INI,
  // file scans) until it switches to UTF-8 internally.
  inline std::wstring ToWide(const char *psz)
  {
    if (*psz == 0)
      return std::wstring();

    std::wstring ws = Utf8ToWide(psz);
    return ws.empty() ? AnsiToWide(psz) : ws;
  }

  // Back to the encoding ToWide() detected on psz_like.
  inline std::string FromWideAs(const wchar_t *pwsz, const char *psz_like)
  {
    if ((*psz_like != 0) && Utf8ToWide(psz_like).empty())
      return WideToAnsi(pwsz);

    return WideToUtf8(pwsz);
  }
}


#endif //_PLATFORM_UTF_WIN32_H
