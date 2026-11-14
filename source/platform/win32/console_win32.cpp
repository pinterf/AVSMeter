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


void platform::ConsoleToOutputCP(char *psz_buffer)
{
  CharToOem(psz_buffer, psz_buffer);
}


bool platform::KeyPressed()
{
  return _kbhit() != 0;
}


int platform::ReadKey()
{
  return _getch();
}
