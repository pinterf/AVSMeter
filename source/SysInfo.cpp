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


#include "SysInfo.h"
#include "platform/os_info.h"

#include <ctime>


CSysInfo::CSysInfo()
{
}

CSysInfo::~CSysInfo()
{
}


string CSysInfo::GetOSVersion()
{
  return platform::OsVersion();
}


string CSysInfo::GetFormattedSystemDateTime()
{
  std::time_t t = std::time(nullptr);
  std::tm tmLocal = {};
#if defined(_WIN32)
  localtime_s(&tmLocal, &t);
#else
  localtime_r(&t, &tmLocal);
#endif

  return utils.StrFormat("%04d%02d%02d-%02d%02d%02d", tmLocal.tm_year + 1900, tmLocal.tm_mon + 1, tmLocal.tm_mday, tmLocal.tm_hour, tmLocal.tm_min, tmLocal.tm_sec);
}


string CSysInfo::RemoveMultipleWhiteSpaces(string s)
{
  string search = "  ";
  size_t index;
  while((index = s.find(search)) != string::npos) s.erase(index,1);

  return s;
}


void CSysInfo::GetCPUID()
{
  if (!cpuid_present())
    return;

  struct cpu_raw_data_t raw;
  struct cpu_id_t data;

  if (cpuid_get_raw_data(&raw) < 0)
    return;

  if (cpu_identify(&raw, &data) < 0)
    return;

  if ((cpudata.CPUTotalLogicalCores > cpudata.CPULogicalCores) && (cpudata.CPULogicalCores > 0))
    cpudata.NumCPUs = data.total_logical_cpus / data.num_logical_cpus;
  else
    cpudata.NumCPUs = 1;

  cpudata.CPUCores = data.num_cores * cpudata.NumCPUs;
  cpudata.CPULogicalCores = data.num_logical_cpus * cpudata.NumCPUs;
  cpudata.CPUCodeName = utils.StrFormat("%s", data.cpu_codename);
  cpudata.CPUCodeName = RemoveMultipleWhiteSpaces(cpudata.CPUCodeName);
  cpudata.CPUBrandString = utils.StrFormat("%s", data.brand_str);
  cpudata.CPUBrandString = RemoveMultipleWhiteSpaces(cpudata.CPUBrandString);

  cpudata.CPUSupportedInstructionSets = "";

  if (data.flags[CPU_FEATURE_3DNOW])       cpudata.CPUSupportedInstructionSets += "3DNOW, ";
  if (data.flags[CPU_FEATURE_3DNOWEXT])    cpudata.CPUSupportedInstructionSets += "3DNOWEXT, ";
  if (data.flags[CPU_FEATURE_MMX])         cpudata.CPUSupportedInstructionSets += "MMX, ";
  if (data.flags[CPU_FEATURE_MMXEXT])      cpudata.CPUSupportedInstructionSets += "MMXEXT, ";
  if (data.flags[CPU_FEATURE_SSE])         cpudata.CPUSupportedInstructionSets += "SSE, ";
  if (data.flags[CPU_FEATURE_SSE2])        cpudata.CPUSupportedInstructionSets += "SSE2, ";
  if (data.flags[CPU_FEATURE_PNI])         cpudata.CPUSupportedInstructionSets += "SSE3, ";
  if (data.flags[CPU_FEATURE_SSSE3])       cpudata.CPUSupportedInstructionSets += "SSSE3, ";
  if (data.flags[CPU_FEATURE_SSE4_1])      cpudata.CPUSupportedInstructionSets += "SSE4.1, ";
  if (data.flags[CPU_FEATURE_SSE4_2])      cpudata.CPUSupportedInstructionSets += "SSE4.2, ";
  if (data.flags[CPU_FEATURE_SSE4A])       cpudata.CPUSupportedInstructionSets += "SSE4A, ";
  if (data.flags[CPU_FEATURE_FMA3])        cpudata.CPUSupportedInstructionSets += "FMA3, ";
  if (data.flags[CPU_FEATURE_FMA4])        cpudata.CPUSupportedInstructionSets += "FMA4, ";
  if (data.flags[CPU_FEATURE_RDSEED])      cpudata.CPUSupportedInstructionSets += "RDSEED, ";
  if (data.flags[CPU_FEATURE_ADX])         cpudata.CPUSupportedInstructionSets += "ADX, ";
  if (data.flags[CPU_FEATURE_AVX])         cpudata.CPUSupportedInstructionSets += "AVX, ";
  if (data.flags[CPU_FEATURE_AVX2])        cpudata.CPUSupportedInstructionSets += "AVX2, ";
  if (data.flags[CPU_FEATURE_AVX512F])     cpudata.CPUSupportedInstructionSets += "AVX512F, ";
  if (data.flags[CPU_FEATURE_AVX512DQ])    cpudata.CPUSupportedInstructionSets += "AVX512DQ, ";
  if (data.flags[CPU_FEATURE_AVX512PF])    cpudata.CPUSupportedInstructionSets += "AVX512PF, ";
  if (data.flags[CPU_FEATURE_AVX512ER])    cpudata.CPUSupportedInstructionSets += "AVX512ER, ";
  if (data.flags[CPU_FEATURE_AVX512CD])    cpudata.CPUSupportedInstructionSets += "AVX512CD, ";
  if (data.flags[CPU_FEATURE_AVX512BW])    cpudata.CPUSupportedInstructionSets += "AVX512BW, ";
  if (data.flags[CPU_FEATURE_AVX512VL])    cpudata.CPUSupportedInstructionSets += "AVX512VL, ";
  if (data.flags[CPU_FEATURE_AVX512VNNI])  cpudata.CPUSupportedInstructionSets += "AVX512VNNI, ";
  if (data.flags[CPU_FEATURE_AVX512VBMI])  cpudata.CPUSupportedInstructionSets += "AVX512VBMI, ";
  if (data.flags[CPU_FEATURE_AVX512VBMI2]) cpudata.CPUSupportedInstructionSets += "AVX512VBMI2, ";

  cpudata.CPUSupportedInstructionSets.erase(cpudata.CPUSupportedInstructionSets.find_last_not_of(", ") + 1);

  bCPUIDSuccess = true;

  return;
}
