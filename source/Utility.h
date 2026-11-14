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



class CUtils
{
public:
  CUtils();
  virtual        ~CUtils();

  //File/directory/system
  string         SysErrorMessage();
  string         GetFileTimeStamp(string s_file);
  string         GetFileDateStamp(string s_file);

  //string functions
  string         StrFormat(char const *fmt, ...);
  void           StrTrimLeft(string &s_string);
  void           StrTrimRight(string &s_string);
  void           StrTrim(string &s_string);
  void           StrToUC(string &s_string);
  void           StrToLC(string &s_string);
  string         StrFormatFPS(double d_fps);
  string         StrFormatTPF(double d_frametime);
  void           StrTokenize(string s_string, vector<string> &tokens, string s_delimiter, bool b_removedups);
  bool           IsNumeric(string s_string);

};


#endif //_UTILITY_H

