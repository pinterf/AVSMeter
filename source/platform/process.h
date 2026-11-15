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


#if !defined(_PLATFORM_PROCESS_H)
#define _PLATFORM_PROCESS_H

#include <string>
#include <vector>

namespace platform
{
  bool SetHighPriority();

  // No system error dialogs (Windows: missing disk, failed DLL dependencies); errors go to the caller.
  void SuppressErrorDialogs();

  // Message for the last OS error of the calling thread, UTF-8; empty if there is none.
  std::string LastErrorMessage();

  // Command line arguments as UTF-8, argv[0] included (Windows: from the wide command line).
  std::vector<std::string> CommandLineArgs(int argc, char *argv[]);
}


#endif //_PLATFORM_PROCESS_H
