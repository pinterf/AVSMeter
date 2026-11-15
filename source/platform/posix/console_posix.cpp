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
#include "platform/posix/console_posix.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>


namespace
{
  uint16_t g_wColors = 0; // pending attributes for the next ConsoleWrite, 0: none


  bool ColorsAllowed()
  {
    static const bool bAllowed = []
    {
      const char *pszNoColor = getenv("NO_COLOR"); // https://no-color.org
      const char *pszTerm = getenv("TERM");
      return !(pszNoColor && pszNoColor[0] != '\0') && !(pszTerm && strcmp(pszTerm, "dumb") == 0);
    }();
    return bAllowed;
  }


  // Windows attribute bits (blue 1, green 2, red 4, intensity 8) -> ANSI SGR. A black
  // background keeps the terminal's own background, grey is the default foreground and
  // white is bold, so light terminal themes stay readable.
  std::string AnsiSequence(uint16_t w_attributes)
  {
    auto Index = [](unsigned bits) { return ((bits & 4) ? 1 : 0) | ((bits & 2) ? 2 : 0) | ((bits & 1) ? 4 : 0); };

    unsigned fg = w_attributes & 0x0F;
    unsigned bg = (w_attributes >> 4) & 0x0F;
    std::string sSeq = "\x1b[0";

    if (fg == (FG_GREY | FG_DARKGREY))
      sSeq += ";1";
    else if (fg != FG_GREY)
      sSeq += ";" + std::to_string(((fg & 8) ? 90 : 30) + Index(fg));

    if (bg != 0)
      sSeq += ";" + std::to_string(((bg & 8) ? 100 : 40) + Index(bg));

    return sSeq + "m";
  }


  // stdin in non-canonical mode without echo, so single keys (ESC) arrive without Enter.
  // ISIG stays on: Ctrl+C still works. Restored at exit and on terminating signals.
  struct termios g_SavedTermios;
  volatile sig_atomic_t g_bRawInput = 0;


  void RestoreInput()
  {
    if (g_bRawInput)
    {
      tcsetattr(STDIN_FILENO, TCSANOW, &g_SavedTermios);
      g_bRawInput = 0;
    }
  }


  void RestoreInputOnSignal(int i_signal)
  {
    RestoreInput();
    signal(i_signal, SIG_DFL);
    raise(i_signal);
  }


  bool EnterRawInput()
  {
    static bool bTried = false;
    if (bTried)
      return g_bRawInput != 0;
    bTried = true;

    // A background job would be stopped by SIGTTOU when changing the terminal
    if (!isatty(STDIN_FILENO) || tcgetpgrp(STDIN_FILENO) != getpgrp() || tcgetattr(STDIN_FILENO, &g_SavedTermios) != 0)
      return false;

    struct termios raw = g_SavedTermios;
    raw.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0)
      return false;

    g_bRawInput = 1;
    atexit(RestoreInput);
    for (int iSignal : { SIGINT, SIGTERM, SIGHUP, SIGQUIT })
      signal(iSignal, RestoreInputOnSignal);

    return true;
  }


  bool InputReady(int i_timeout_ms)
  {
    struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
    return poll(&pfd, 1, i_timeout_ms) > 0;
  }
}


void platform::posix::RestoreConsoleInput()
{
  RestoreInput();
}


int platform::ConsoleWidth()
{
  struct winsize ws;
  if ((ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) && (ws.ws_col > 0))
    return (int)ws.ws_col;

  return 80;
}


// Applied by ConsoleWrite, only when the target stream is a terminal.
void platform::ConsoleSetColors(uint16_t w_attributes)
{
  g_wColors = w_attributes;
}


void platform::ConsoleResetColors()
{
  g_wColors = 0;
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
  bool bColor = (g_wColors != 0) && ColorsAllowed() && isatty(fileno(pFile));

  std::string sOut;
  if (bColor)
    sOut = AnsiSequence(g_wColors) + s_text + "\x1b[0m";
  const std::string &sText = bColor ? sOut : s_text;

  std::fwrite(sText.data(), 1, sText.size(), pFile);
  std::fflush(pFile);
}


bool platform::KeyPressed()
{
  return EnterRawInput() && InputReady(0);
}


// Blocks until a key is pressed. ESC alone gives 0x1B; an escape sequence (arrow and
// function keys) is consumed and gives 0xE0, like a Windows extended key. 1 at end of input.
int platform::ReadKey()
{
  if (!EnterRawInput())
  {
    int iChar = std::getchar();
    return (iChar == EOF) ? 1 : iChar;
  }

  unsigned char c = 0;
  if (read(STDIN_FILENO, &c, 1) != 1)
    return 1;

  if (c == 0x1B && InputReady(30))
  {
    unsigned char buf[16];
    while (InputReady(0) && read(STDIN_FILENO, buf, sizeof(buf)) > 0)
    {
    }
    return 0xE0;
  }

  return c;
}
