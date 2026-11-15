/*
  This file is part of AVSMeter, (c) 2026 pinterf.

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

#include <climits>
#include <cstdlib>
#include <time.h>

#include <sys/stat.h>
#include <unistd.h>


int64_t platform::FileSize(const std::string &s_path)
{
  struct stat st;
  if (stat(s_path.c_str(), &st) != 0)
    return -1;

  return (int64_t)st.st_size;
}


bool platform::FileExists(const std::string &s_path)
{
  struct stat st;
  return (stat(s_path.c_str(), &st) == 0) && !S_ISDIR(st.st_mode);
}


bool platform::DirectoryExists(const std::string &s_path)
{
  struct stat st;
  return (stat(s_path.c_str(), &st) == 0) && S_ISDIR(st.st_mode);
}


bool platform::FileModifiedUtc(const std::string &s_path, std::tm &tm_utc)
{
  struct stat st;
  if (stat(s_path.c_str(), &st) != 0)
    return false;

  tm_utc = std::tm();
  return gmtime_r(&st.st_mtime, &tm_utc) != nullptr;
}


// Shared objects have no version resource.
std::string platform::FileVersion(const std::string &s_path)
{
  (void)s_path;
  return "n/a";
}


std::string platform::ProductVersion(const std::string &s_path)
{
  (void)s_path;
  return "n/a";
}


std::string platform::NativeSystemPath(const std::string &s_path)
{
  return s_path;
}


std::string platform::ExecutablePath()
{
  char szBuf[PATH_MAX + 1];
  ssize_t iLen = readlink("/proc/self/exe", szBuf, PATH_MAX);
  if (iLen <= 0)
    return std::string();

  return std::string(szBuf, (size_t)iLen);
}


std::string platform::IniFilePath()
{
  const std::string sName = "avsmeter.ini";

  std::string sExe = ExecutablePath();
  size_t nPos = sExe.rfind('/');
  if (nPos != std::string::npos)
  {
    std::string sPortable = sExe.substr(0, nPos + 1) + sName;
    if (FileExists(sPortable))
      return sPortable;
  }

  // XDG Base Directory spec: a relative $XDG_CONFIG_HOME is invalid and ignored
  std::string sDir;
  const char *pszXdg = getenv("XDG_CONFIG_HOME");
  const char *pszHome = getenv("HOME");
  if (pszXdg && pszXdg[0] == '/')
    sDir = pszXdg;
  else if (pszHome && pszHome[0] != '\0')
    sDir = std::string(pszHome) + "/.config";
  else
    return std::string();

  sDir += "/avsmeter";
  std::error_code ec;
  std::filesystem::create_directories(sDir, ec); // a failure shows up when the file is written

  return sDir + "/" + sName;
}


std::filesystem::path platform::PathFromUtf8(const std::string &s_path)
{
  return std::filesystem::path(s_path);
}


std::string platform::PathToUtf8(const std::filesystem::path &path)
{
  return path.string();
}


std::string platform::AbsolutePath(const std::string &s_path)
{
  std::error_code ec;
  std::filesystem::path path = std::filesystem::absolute(PathFromUtf8(s_path), ec);
  if (ec)
    return std::string();

  return PathToUtf8(path);
}


bool platform::LegacyPath(const std::string &s_path, std::string &s_legacy)
{
  s_legacy = s_path;
  return true;
}
