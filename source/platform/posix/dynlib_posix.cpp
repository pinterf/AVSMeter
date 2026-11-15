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


#include "platform/dynlib.h"
#include "platform/posix/last_error_posix.h"

#include <dlfcn.h>


platform::LibHandle platform::LoadLib(const char *psz_name)
{
  void *hLib = dlopen(psz_name, RTLD_NOW | RTLD_LOCAL);
  const char *pszError = hLib ? nullptr : dlerror();
  posix::SetLastDlError(pszError ? pszError : "");

  return hLib;
}


void *platform::GetLibSymbol(LibHandle h_lib, const char *psz_symbol)
{
  dlerror();
  void *pSymbol = dlsym(h_lib, psz_symbol);
  const char *pszError = dlerror();
  posix::SetLastDlError(pszError ? pszError : "");

  return pSymbol;
}


bool platform::FreeLib(LibHandle h_lib)
{
  return dlclose(h_lib) == 0;
}
