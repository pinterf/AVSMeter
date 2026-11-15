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


#if !defined(_PLATFORM_DYNLIB_H)
#define _PLATFORM_DYNLIB_H

namespace platform
{
  using LibHandle = void *;

#if defined(_WIN32)
  inline constexpr const char *kAvsLibName = "avisynth";
  inline constexpr const char *kAvsLibFileName = "avisynth.dll"; // for messages
  inline constexpr const char *kAvsLibLabel = "Avisynth.dll";    // for log labels
  inline constexpr const char *kPluginExt = ".dll";               // compared case-insensitively
#else
  inline constexpr const char *kAvsLibName = "libavisynth.so";
  inline constexpr const char *kAvsLibFileName = "libavisynth.so";
  inline constexpr const char *kAvsLibLabel = "libavisynth.so";
#if defined(__APPLE__)
  inline constexpr const char *kPluginExt = ".dylib";             // compared case-sensitively
#else
  inline constexpr const char *kPluginExt = ".so";
#endif
#endif

  // psz_name is UTF-8 (Windows: ANSI is accepted if not valid UTF-8).
  // On failure the OS error is left for SysErrorMessage().
  LibHandle LoadLib(const char *psz_name);
  void     *GetLibSymbol(LibHandle h_lib, const char *psz_symbol);
  bool      FreeLib(LibHandle h_lib);
}


#endif //_PLATFORM_DYNLIB_H
