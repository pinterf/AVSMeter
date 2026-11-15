/*
  This file is part of AVSMeter, (c) Groucho2004, 2026 pinterf.

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


#if !defined(_PLATFORM_FILE_INFO_H)
#define _PLATFORM_FILE_INFO_H

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <string>

namespace platform
{
  // kPathSep builds paths; kPathSeparators lists every separator the OS accepts.
#ifdef _WIN32
  inline constexpr char        kPathSep = '\\';
  inline constexpr const char *kPathSeparators = "\\/";
#else
  inline constexpr char        kPathSep = '/';
  inline constexpr const char *kPathSeparators = "/";
#endif

  int64_t     FileSize(const std::string &s_path); // -1: not found
  bool        FileExists(const std::string &s_path); // regular file, not a directory
  bool        DirectoryExists(const std::string &s_path);
  bool        FileModifiedUtc(const std::string &s_path, std::tm &tm_utc);

  // Version resource as "a.b.c.d", "n/a" if none. Empty path: own executable.
  std::string FileVersion(const std::string &s_path);
  std::string ProductVersion(const std::string &s_path);

  // Path as seen by the OS: System32 maps to SysWOW64 for a 32 bit process on 64 bit Windows.
  std::string NativeSystemPath(const std::string &s_path);

  std::string ExecutablePath(); // own executable, empty on error

  // UTF-8 path <-> std::filesystem::path, for streams and directory iterators.
  std::filesystem::path PathFromUtf8(const std::string &s_path);
  std::string           PathToUtf8(const std::filesystem::path &path);
  std::string           AbsolutePath(const std::string &s_path); // empty on error

  // Path in the legacy 8 bit encoding (Windows: ANSI code page) for APIs without
  // UTF-8 support. False if not representable.
  bool                  LegacyPath(const std::string &s_path, std::string &s_legacy);
}


#endif //_PLATFORM_FILE_INFO_H
