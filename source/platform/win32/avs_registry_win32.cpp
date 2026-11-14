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


#include "platform/avs_registry.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


std::vector<platform::RegisteredPluginDir> platform::RegisteredPluginDirs()
{
  std::vector<RegisteredPluginDir> vDirs;

  const struct { HKEY hRoot; const char *pszHive; } roots[] = { { HKEY_CURRENT_USER, "HKCU" }, { HKEY_LOCAL_MACHINE, "HKLM" } };
  const char *values[] = { "PluginDir2_5", "PluginDir+" };

  for (const auto &root : roots)
  {
    HKEY hKey = NULL;
    if (::RegOpenKeyExA(root.hRoot, "Software\\Avisynth", 0, KEY_READ, &hKey) != ERROR_SUCCESS)
      continue;

    for (const char *pszValue : values)
    {
      // ANSI: the plugin scan still uses ANSI paths
      char szDir[2050] = "";
      DWORD dwBytes = 2048;
      if (::RegQueryValueExA(hKey, pszValue, NULL, NULL, (LPBYTE)szDir, &dwBytes) == ERROR_SUCCESS)
        vDirs.push_back({ pszValue, root.pszHive, szDir });
    }

    ::RegCloseKey(hKey);
  }

  return vDirs;
}
