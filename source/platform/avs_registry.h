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


#if !defined(_PLATFORM_AVS_REGISTRY_H)
#define _PLATFORM_AVS_REGISTRY_H

#include <string>
#include <vector>

namespace platform
{
  struct RegisteredPluginDir
  {
    std::string valueName; // "PluginDir2_5" or "PluginDir+"
    std::string hive;      // "HKCU" or "HKLM"
    std::string path;
  };

  // Plugin directories registered by the Avisynth installers under Software\Avisynth:
  // PluginDir2_5 and PluginDir+, HKCU first, then HKLM. Empty where there is no registry.
  std::vector<RegisteredPluginDir> RegisteredPluginDirs();
}


#endif //_PLATFORM_AVS_REGISTRY_H
