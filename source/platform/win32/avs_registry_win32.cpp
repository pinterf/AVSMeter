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
#include "platform/win32/utf_win32.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


std::vector<platform::RegisteredPluginDir> platform::RegisteredPluginDirs()
{
  std::vector<RegisteredPluginDir> vDirs;

  const struct { HKEY hRoot; const char *pszHive; } roots[] = { { HKEY_CURRENT_USER, "HKCU" }, { HKEY_LOCAL_MACHINE, "HKLM" } };
  const struct { const wchar_t *pwszValue; const char *pszValue; } values[] = { { L"PluginDir2_5", "PluginDir2_5" }, { L"PluginDir+", "PluginDir+" } };

  for (const auto &root : roots)
  {
    HKEY hKey = NULL;
    if (::RegOpenKeyExW(root.hRoot, L"Software\\Avisynth", 0, KEY_READ, &hKey) != ERROR_SUCCESS)
      continue;

    for (const auto &value : values)
    {
      wchar_t wszDir[2050] = L"";
      DWORD dwBytes = 2048 * sizeof(wchar_t);
      if (::RegQueryValueExW(hKey, value.pwszValue, NULL, NULL, (LPBYTE)wszDir, &dwBytes) == ERROR_SUCCESS)
        vDirs.push_back({ value.pszValue, root.pszHive, win32::WideToUtf8(wszDir) });
    }

    ::RegCloseKey(hKey);
  }

  return vDirs;
}
