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


#if !defined(_PLATFORM_CONSOLE_H)
#define _PLATFORM_CONSOLE_H

#include <cstdint>
#include <string>

// Color attributes use the Windows console bit layout on every platform:
// bits 0-3 foreground (blue, green, red, intensity), bits 4-7 background.
inline constexpr uint16_t FG_BLACK     = 0x00;
inline constexpr uint16_t FG_RED       = 0x04;
inline constexpr uint16_t FG_GREEN     = 0x02;
inline constexpr uint16_t FG_BLUE      = 0x01;
inline constexpr uint16_t FG_YELLOW    = (FG_RED | FG_GREEN);
inline constexpr uint16_t FG_MAGENTA   = (FG_RED | FG_BLUE);
inline constexpr uint16_t FG_CYAN      = (FG_GREEN | FG_BLUE);
inline constexpr uint16_t FG_DARKGREY  = 0x08;
inline constexpr uint16_t FG_HRED      = (FG_RED | FG_DARKGREY);
inline constexpr uint16_t FG_HGREEN    = (FG_GREEN | FG_DARKGREY);
inline constexpr uint16_t FG_HBLUE     = (FG_BLUE | FG_DARKGREY);
inline constexpr uint16_t FG_HYELLOW   = (FG_YELLOW | FG_DARKGREY);
inline constexpr uint16_t FG_HMAGENTA  = (FG_MAGENTA | FG_DARKGREY);
inline constexpr uint16_t FG_HCYAN     = (FG_CYAN | FG_DARKGREY);
inline constexpr uint16_t FG_GREY      = (FG_RED | FG_GREEN | FG_BLUE);
inline constexpr uint16_t FG_WHITE     = (FG_GREY | FG_DARKGREY);

inline constexpr uint16_t BG_BLACK     = 0x00;
inline constexpr uint16_t BG_RED       = 0x40;
inline constexpr uint16_t BG_GREEN     = 0x20;
inline constexpr uint16_t BG_BLUE      = 0x10;
inline constexpr uint16_t BG_YELLOW    = (BG_RED | BG_GREEN);
inline constexpr uint16_t BG_MAGENTA   = (BG_RED | BG_BLUE);
inline constexpr uint16_t BG_CYAN      = (BG_GREEN | BG_BLUE);
inline constexpr uint16_t BG_DARKGREY  = 0x80;
inline constexpr uint16_t BG_HRED      = (BG_RED | BG_DARKGREY);
inline constexpr uint16_t BG_HGREEN    = (BG_GREEN | BG_DARKGREY);
inline constexpr uint16_t BG_HBLUE     = (BG_BLUE | BG_DARKGREY);
inline constexpr uint16_t BG_HYELLOW   = (BG_YELLOW | BG_DARKGREY);
inline constexpr uint16_t BG_HMAGENTA  = (BG_MAGENTA | BG_DARKGREY);
inline constexpr uint16_t BG_HCYAN     = (BG_CYAN | BG_DARKGREY);
inline constexpr uint16_t BG_GREY      = (BG_RED | BG_GREEN | BG_BLUE);
inline constexpr uint16_t BG_WHITE     = (BG_GREY | BG_DARKGREY);

namespace platform
{
  int   ConsoleWidth();
  void  ConsoleSetColors(uint16_t w_attributes);
  void  ConsoleResetColors();
  void  ConsoleCursorUp(unsigned int i_rows);
  // Writes UTF-8 text (invalid UTF-8 is read as ANSI) to stdout or stderr: full Unicode
  // on a console, UTF-8 bytes when redirected.
  void  ConsoleWrite(bool b_stdout, const std::string &s_text);

  bool  KeyPressed();
  int   ReadKey();
}


#endif //_PLATFORM_CONSOLE_H
