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


#include "platform/console.h"

#include <cstdio>

#include <sys/ioctl.h>
#include <unistd.h>


int platform::ConsoleWidth()
{
  struct winsize ws;
  if ((ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) && (ws.ws_col > 0))
    return (int)ws.ws_col;

  return 80;
}


// Phase 2 stub: ANSI colors on a terminal
void platform::ConsoleSetColors(uint16_t w_attributes)
{
  (void)w_attributes;
}


void platform::ConsoleResetColors()
{
}


void platform::ConsoleCursorUp(unsigned int i_rows)
{
  if ((i_rows > 0) && isatty(STDOUT_FILENO))
  {
    std::printf("\x1b[%uA", i_rows);
    std::fflush(stdout);
  }
}


void platform::ConsoleWrite(bool b_stdout, const std::string &s_text)
{
  FILE *pFile = b_stdout ? stdout : stderr;
  std::fwrite(s_text.data(), 1, s_text.size(), pFile);
  std::fflush(pFile);
}


// Phase 2 stub: termios raw mode for "press any key" and ESC
bool platform::KeyPressed()
{
  return false;
}


int platform::ReadKey()
{
  int iChar = std::getchar();
  return (iChar == EOF) ? 1 : iChar;
}
