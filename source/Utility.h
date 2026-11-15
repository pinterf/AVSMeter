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


#if !defined(_UTILITY_H)
#define _UTILITY_H

#include "common.h"
#include <cstdarg>



// printf-style formatting into a string of any length; "Buffer overflow" on encoding error.
std::string StrVFormat(char const *fmt, va_list args);

class CUtils
{
public:
  CUtils();
  virtual        ~CUtils();

  //File/directory/system
  std::string    SysErrorMessage();
  std::string    GetFileTimeStamp(std::string s_file);
  std::string    GetFileDateStamp(std::string s_file);

  //string functions
  std::string    StrFormat(char const *fmt, ...);
  void           StrTrimLeft(std::string &s_string);
  void           StrTrimRight(std::string &s_string);
  void           StrTrim(std::string &s_string);
  void           StrToUC(std::string &s_string);
  void           StrToLC(std::string &s_string);
  std::string    StrFormatFPS(double d_fps);
  std::string    StrFormatTPF(double d_frametime);
  void           StrTokenize(std::string s_string, std::vector<std::string> &tokens, std::string s_delimiter, bool b_removedups);
  bool           IsNumeric(std::string s_string);

};


#endif //_UTILITY_H

