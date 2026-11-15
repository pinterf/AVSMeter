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


#include "platform/single_instance.h"

#include <cstdlib>
#include <string>

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>


// Per-user lock, like the per-session Windows mutex: flock on a file in $XDG_RUNTIME_DIR
// (or /tmp with the uid in the name). The kernel drops the lock when the process exits,
// also after a crash. If the lock file is unusable the instance is allowed to run.
bool platform::AcquireSingleInstance(const char *psz_name)
{
  std::string sPath;
  const char *pszRuntimeDir = getenv("XDG_RUNTIME_DIR");
  if (pszRuntimeDir && pszRuntimeDir[0] == '/')
    sPath = std::string(pszRuntimeDir) + "/" + psz_name + ".lock";
  else
    sPath = std::string("/tmp/") + psz_name + "." + std::to_string((unsigned long)getuid()) + ".lock";

  int iFd = open(sPath.c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
  if (iFd < 0)
    return true;

  if (flock(iFd, LOCK_EX | LOCK_NB) == 0)
    return true; // the descriptor stays open for the process lifetime

  close(iFd);
  return false;
}
