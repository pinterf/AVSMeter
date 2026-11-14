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

#define FG_BLACK     0
#define FG_RED       FOREGROUND_RED
#define FG_GREEN     FOREGROUND_GREEN
#define FG_BLUE      FOREGROUND_BLUE
#define FG_YELLOW    FOREGROUND_RED | FOREGROUND_GREEN
#define FG_MAGENTA   FOREGROUND_RED | FOREGROUND_BLUE
#define FG_CYAN      FOREGROUND_GREEN | FOREGROUND_BLUE
#define FG_HRED      FOREGROUND_RED | FOREGROUND_INTENSITY
#define FG_HGREEN    FOREGROUND_GREEN | FOREGROUND_INTENSITY
#define FG_HBLUE     FOREGROUND_BLUE | FOREGROUND_INTENSITY
#define FG_HYELLOW   FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY
#define FG_HMAGENTA  FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY
#define FG_HCYAN     FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY
#define FG_DARKGREY  FOREGROUND_INTENSITY
#define FG_GREY      FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED
#define FG_WHITE     FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY

#define BG_BLACK     0
#define BG_RED       BACKGROUND_RED
#define BG_GREEN     BACKGROUND_GREEN
#define BG_BLUE      BACKGROUND_BLUE
#define BG_YELLOW    BACKGROUND_RED | BACKGROUND_GREEN
#define BG_MAGENTA   BACKGROUND_RED | BACKGROUND_BLUE
#define BG_CYAN      BACKGROUND_GREEN | BACKGROUND_BLUE
#define BG_HRED      BACKGROUND_RED | BACKGROUND_INTENSITY
#define BG_HGREEN    BACKGROUND_GREEN | BACKGROUND_INTENSITY
#define BG_HBLUE     BACKGROUND_BLUE | BACKGROUND_INTENSITY
#define BG_HYELLOW   BACKGROUND_GREEN | BACKGROUND_RED | BACKGROUND_INTENSITY
#define BG_HMAGENTA  BACKGROUND_RED | BACKGROUND_BLUE | BACKGROUND_INTENSITY
#define BG_HCYAN     BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY
#define BG_DARKGREY  BACKGROUND_INTENSITY
#define BG_GREY      BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_RED
#define BG_WHITE     BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_RED | BACKGROUND_INTENSITY


class CUtils
{
public:
  CUtils();
  virtual        ~CUtils();

  //File/directory/system
  string         SysErrorMessage();
  __int64        FileSize(string s_file);
  string         GetFileVersion(string s_file);
  string         GetProductVersion(string s_file);
  string         GetFileTimeStamp(string s_file);
  string         GetFileDateStamp(string s_file);
  BOOL           FileExists(string s_file);
  BOOL           DirectoryExists(string s_dir);
  string         GetWOW64FilePath(string s_filepath);

  //Console functions
  int            GetConsoleWidth();
  void           SetConsoleColors(WORD wAttributes);
  void           ResetConsoleColors();
  void           CursorUp(unsigned int iLines);

  //string functions
  string         StrFormat(char const *fmt, ...);
  void           StrTrimLeft(string &s_string);
  void           StrTrimRight(string &s_string);
  void           StrTrim(string &s_string);
  void           StrToUC(string &s_string);
  void           StrToLC(string &s_string);
  string         StrFormatFPS(double d_fps);
  string         StrFormatTPF(double d_frametime);
  void           StrTokenize(string s_string, vector<string> &tokens, string s_delimiter, BOOL b_removedups);
  BOOL           IsNumeric(string s_string);

private:
  HANDLE         hConsole;
  WORD           wSavedAttributes;
};


#endif //_UTILITY_H

