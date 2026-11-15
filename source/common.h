/*
  This file is part of AVSMeter, Copyright(C) Groucho2004, 2026 pinterf.

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


#if !defined(_COMMON_H)
#define _COMMON_H

#include <cstdint>
#include <math.h>
#include <string>
#include <algorithm>
#include <fstream>
#include <vector>
#include <map>
#include <set>

#define MAX_PATH_LEN 32768
#define DSE_DELAY 500
const bool PROCESS_64 = (sizeof(void*) == 8);

// Process architecture for banners, labels and file names
#if defined(_M_X64) || defined(__x86_64__)
constexpr const char *kArchName = "x64";
#elif defined(_M_IX86) || defined(__i386__)
constexpr const char *kArchName = "x86";
#elif defined(_M_ARM64) || defined(__aarch64__)
constexpr const char *kArchName = "aarch64";
#elif defined(_M_ARM) || defined(__arm__)
constexpr const char *kArchName = "arm";
#else
constexpr const char *kArchName = PROCESS_64 ? "64 Bit" : "32 Bit";
#endif

inline bool CompareNoCase(std::string first, std::string second)
{
  size_t i = 0;
  while ((i < first.length()) && (i < second.length()))
  {
    if (tolower((unsigned char)first[i]) < tolower((unsigned char)second[i]))
      return true;
    else if (tolower((unsigned char)first[i]) > tolower((unsigned char)second[i]))
      return false;

   i++;
  }

  if (first.length() < second.length())
    return true;
  else
    return false;
}


#endif //_COMMON_H
