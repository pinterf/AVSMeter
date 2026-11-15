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


#include "platform/console.h"
#include "platform/win32/utf_win32.h"

#include <algorithm>
#include <cstdio>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <conio.h>


namespace
{
  // Saves the console colors at startup and restores them at exit.
  struct ConsoleState
  {
    HANDLE hConsole;
    WORD   wSavedAttributes;

    ConsoleState()
    {
      hConsole = ::GetStdHandle(STD_OUTPUT_HANDLE);

      CONSOLE_SCREEN_BUFFER_INFO csbi;
      GetConsoleScreenBufferInfo(hConsole, &csbi);
      wSavedAttributes = csbi.wAttributes;
    }

    ~ConsoleState()
    {
      SetConsoleTextAttribute(hConsole, wSavedAttributes);
    }
  };

  ConsoleState &State()
  {
    static ConsoleState state;
    return state;
  }
}


int platform::ConsoleWidth()
{
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(State().hConsole, &csbi);

  return (int)csbi.dwSize.X;
}


void platform::ConsoleSetColors(uint16_t w_attributes)
{
  SetConsoleTextAttribute(State().hConsole, w_attributes);
}


void platform::ConsoleResetColors()
{
  SetConsoleTextAttribute(State().hConsole, State().wSavedAttributes);
}


void platform::ConsoleCursorUp(unsigned int i_rows)
{
  COORD pos;
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  ::GetConsoleScreenBufferInfo(State().hConsole, &csbi);
  pos.Y = (short)(csbi.dwCursorPosition.Y - i_rows);
  pos.X = 0;
  ::SetConsoleCursorPosition(State().hConsole, pos);
}


void platform::ConsoleWrite(bool b_stdout, const std::string &s_text)
{
  FILE *fp = b_stdout ? stdout : stderr;
  HANDLE hOut = ::GetStdHandle(b_stdout ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);
  std::wstring wsText = win32::ToWide(s_text.c_str());

  DWORD dwMode = 0;
  if (::GetConsoleMode(hOut, &dwMode))
  {
    fflush(fp);
    // Older consoles (Windows 7) fail on large writes: write in chunks, not splitting surrogate pairs.
    const size_t chunk = 8192;
    size_t pos = 0;
    while (pos < wsText.size())
    {
      size_t len = (std::min)(chunk, wsText.size() - pos);
      if ((pos + len < wsText.size()) && IS_HIGH_SURROGATE(wsText[pos + len - 1]))
        len--;
      DWORD dwWritten = 0;
      ::WriteConsoleW(hOut, wsText.data() + pos, (DWORD)len, &dwWritten, NULL);
      pos += len;
    }
    return;
  }

  std::string sUtf8 = win32::WideToUtf8(wsText.c_str());
  fwrite(sUtf8.data(), 1, sUtf8.size(), fp);
  fflush(fp);
}


bool platform::KeyPressed()
{
  return _kbhit() != 0;
}


int platform::ReadKey()
{
  return _getch();
}
