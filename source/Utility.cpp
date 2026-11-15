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


#include "Utility.h"
#include "platform/file_info.h"


CUtils::CUtils()
{
}

CUtils::~CUtils()
{
}


std::string CUtils::SysErrorMessage()
{
  DWORD dwLastError = GetLastError();
  if (dwLastError == ERROR_SUCCESS)
    return "";

  char msgBuf[2048];
  memset(msgBuf, 0, sizeof(msgBuf));
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwLastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), msgBuf, 2046, NULL);

  std::string sRet(msgBuf);
  sRet.erase(sRet.find_last_not_of(" \t\n\r") + 1);
  sRet.erase(0, sRet.find_first_not_of(" \t\n\r"));

  return sRet;
}


std::string CUtils::GetFileTimeStamp(std::string s_file)
{
  std::tm tmUtc;
  if (!platform::FileModifiedUtc(s_file, tmUtc))
    return "Cannot determine timestamp";

  return StrFormat("%04d-%02d-%02d, %02d:%02d:%02d (UTC)", tmUtc.tm_year + 1900, tmUtc.tm_mon + 1, tmUtc.tm_mday, tmUtc.tm_hour, tmUtc.tm_min, tmUtc.tm_sec);
}


std::string CUtils::GetFileDateStamp(std::string s_file)
{
  std::tm tmUtc;
  if (!platform::FileModifiedUtc(s_file, tmUtc))
    return "Cannot determine timestamp";

  return StrFormat("%04d-%02d-%02d", tmUtc.tm_year + 1900, tmUtc.tm_mon + 1, tmUtc.tm_mday);
}


void CUtils::StrTrimLeft(std::string &s_string)
{
  s_string.erase(0, s_string.find_first_not_of(" \t\n\r"));
  return;
}


void CUtils::StrTrimRight(std::string &s_string)
{
  s_string.erase(s_string.find_last_not_of(" \t\n\r") + 1);
  return;
}


void CUtils::StrTrim(std::string &s_string)
{
  s_string.erase(s_string.find_last_not_of(" \t\n\r") + 1);
  s_string.erase(0, s_string.find_first_not_of(" \t\n\r"));
  return;
}


void CUtils::StrToUC(std::string &s_string)
{
  std::transform(s_string.begin(), s_string.end(), s_string.begin(), ::toupper);
  return;
}


void CUtils::StrToLC(std::string &s_string)
{
  std::transform(s_string.begin(), s_string.end(), s_string.begin(), ::tolower);
  return;
}


std::string CUtils::StrFormatFPS(double d_fps)
{
  std::string sRet = "";

  if (d_fps < 9.999)      sRet = StrFormat("%.3f", d_fps);
  else if (d_fps < 99.99) sRet = StrFormat("%.2f", d_fps);
  else if (d_fps < 999.9) sRet = StrFormat("%.1f", d_fps);
  else                    sRet = StrFormat("%u", (unsigned int)(d_fps + 0.5));

  return sRet;
}


std::string CUtils::StrFormatTPF(double d_frametime)
{
  std::string sRet = "";

  if (d_frametime < 0.1)         sRet = StrFormat("%.5f", d_frametime);
  else if (d_frametime < 1.0)    sRet = StrFormat("%.4f", d_frametime);
  else if (d_frametime < 10.0)   sRet = StrFormat("%.3f", d_frametime);
  else if (d_frametime < 100.0)  sRet = StrFormat("%.2f", d_frametime);
  else if (d_frametime < 1000.0) sRet = StrFormat("%.1f", d_frametime);
  else                           sRet = StrFormat("%u", (unsigned int)(d_frametime + 0.5));

  return sRet;
}


bool CUtils::IsNumeric(std::string s_string)
{
  if (s_string == "")
    return false;

  for (unsigned int ipos = 0; ipos < s_string.length(); ipos++)
  {
    switch (s_string[ipos])
    {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
      case '-':
        continue;
      default:
        return false;
    }
  }

  return true;
}


void CUtils::StrTokenize(std::string s_string, std::vector<std::string> &tokens, std::string s_delimiter, bool b_removedups)
{
  StrTrim(s_string);
  std::string sToken = "";
  std::string sTokenLC = "";
  std::set<std::string> mDuplicates;

  size_t current = 0;
  size_t next = -1;
  do
  {
    current = next + 1;
    next = s_string.find_first_of(s_delimiter, current);
    sToken = s_string.substr(current, next - current);
    StrTrim(sToken);
    sTokenLC = sToken;
    StrToLC(sTokenLC);
    if ((sToken != "") && (mDuplicates.find(sTokenLC) == mDuplicates.end()))
    {
      tokens.push_back(sToken);
      if (b_removedups)
        mDuplicates.insert(sTokenLC);
    }
  }
  while (next != std::string::npos);

  return;
}


std::string StrVFormat(char const *fmt, va_list args)
{
  va_list args2;
  va_copy(args2, args);
  int len = vsnprintf(nullptr, 0, fmt, args2);
  va_end(args2);
  if (len < 0)
    return "Buffer overflow";

  std::string ret((size_t)len, '\0');
  vsnprintf(&ret[0], (size_t)len + 1, fmt, args);
  return ret;
}


std::string CUtils::StrFormat(char const *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  std::string ret = StrVFormat(fmt, args);
  va_end(args);

  return ret;
}
