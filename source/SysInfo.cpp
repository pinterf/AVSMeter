/*
  This file is part of AVSMeter, (c) Groucho2004, 2026 pinterf.

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


std::string CSysInfo::GetOSVersion()
{
  return platform::OsVersion();
}


std::string CSysInfo::GetFormattedSystemDateTime()
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


std::string CSysInfo::RemoveMultipleWhiteSpaces(std::string s)
{
  std::string search = "  ";
  size_t index;
  while((index = s.find(search)) != std::string::npos) s.erase(index,1);

  return s;
}


void CSysInfo::GetCPUID()
{
  if (!cpuid_present())
  {
    GetOSCPUData();
    return;
  }

  // Zeroed: on ARM cpuid_get_raw_data does not clear the x86 fields, and cpu_identify
  // takes any nonzero basic_cpuid[0] as an x86 CPU
  struct cpu_raw_data_t raw = {};
  struct cpu_id_t data = {};

  if ((cpuid_get_raw_data(&raw) < 0) || (cpu_identify(&raw, &data) < 0))
  {
    GetOSCPUData();
    return;
  }

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

  if (data.architecture == ARCHITECTURE_ARM)
  {
    GetARMCoreTypes();

    // Gotcha: SVE and SME versions are not separate feature bits but version number fields
    // (ID_AA64ZFR0_EL1.SVEver: 0 = SVE, 1 = SVE2, 2 = SVE2.1; ID_AA64PFR1_EL1.SME: 1 = SME,
    // 2 = SME2; ID_AA64SMFR0_EL1.SMEver: 0 = SME, 1 = SME2, 2 = SME2.1). A higher version
    // includes the lower ones, but libcpuid 0.8.2 matches each field with '==', so e.g. an
    // SVE2.1 CPU reports SVE2P1 without SVE2 (seen with qemu -cpu max). Restore the implied flags.
    if (data.flags[CPU_FEATURE_SVE2P1])      data.flags[CPU_FEATURE_SVE2] = 1;
    if (data.flags[CPU_FEATURE_SME2P1])      data.flags[CPU_FEATURE_SME2] = 1;
    if (data.flags[CPU_FEATURE_SME2])        data.flags[CPU_FEATURE_SME] = 1;

    if (data.flags[CPU_FEATURE_ADVSIMD])     cpudata.CPUSupportedInstructionSets += "NEON, ";
    if (data.flags[CPU_FEATURE_FP16])        cpudata.CPUSupportedInstructionSets += "FP16, ";
    if (data.flags[CPU_FEATURE_DOTPROD])     cpudata.CPUSupportedInstructionSets += "DOTPROD, ";
    if (data.flags[CPU_FEATURE_I8MM])        cpudata.CPUSupportedInstructionSets += "I8MM, ";
    if (data.flags[CPU_FEATURE_BF16])        cpudata.CPUSupportedInstructionSets += "BF16, ";
    if (data.flags[CPU_FEATURE_SVE])         cpudata.CPUSupportedInstructionSets += "SVE, ";
    if (data.flags[CPU_FEATURE_SVE2])        cpudata.CPUSupportedInstructionSets += "SVE2, ";
    if (data.flags[CPU_FEATURE_SVE2P1])      cpudata.CPUSupportedInstructionSets += "SVE2.1, ";
    if (data.flags[CPU_FEATURE_SME])         cpudata.CPUSupportedInstructionSets += "SME, ";
    if (data.flags[CPU_FEATURE_SME2])        cpudata.CPUSupportedInstructionSets += "SME2, ";

    cpudata.CPUSupportedInstructionSets.erase(cpudata.CPUSupportedInstructionSets.find_last_not_of(", ") + 1);
    bCPUIDSuccess = true;
    return;
  }

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


// libcpuid cannot identify the CPU (e.g. Windows on ARM64): what the OS tells, no code name.
void CSysInfo::GetOSCPUData()
{
  platform::OsCpuInfo info;
  if (!platform::GetOsCpuInfo(info))
    return;

  cpudata.CPUBrandString = RemoveMultipleWhiteSpaces(info.brand);
  cpudata.CPUCodeName = "";
  cpudata.CPUCores = info.cores;
  cpudata.CPULogicalCores = info.logical_cores;
  cpudata.CPUSupportedInstructionSets = info.instruction_sets;
  bCPUIDSuccess = true;
}


// ARM: cpu_identify reports no core counts and only the type of the core it ran on.
// Identify all cores, list each core type (big.LITTLE: "4x Cortex-A76 + 4x Cortex-A55").
void CSysInfo::GetARMCoreTypes()
{
  struct cpu_raw_data_array_t raw_all = {};
  struct system_id_t system = {};

  if (cpuid_get_all_raw_data(&raw_all) < 0)
    return;

  if ((cpu_identify_all(&raw_all, &system) == 0) && (system.num_cpu_types > 0))
  {
    std::string sBrand, sCodeName;
    int32_t iCores = 0, iLogical = 0;
    for (uint8_t i = 0; i < system.num_cpu_types; i++)
    {
      const struct cpu_id_t &type = system.cpu_types[i];
      iCores += type.num_cores;
      iLogical += type.num_logical_cpus;
      if (i > 0)
      {
        sBrand += " + ";
        sCodeName += " + ";
      }
      if (system.num_cpu_types > 1)
        sBrand += utils.StrFormat("%dx ", type.num_cores);
      sBrand += RemoveMultipleWhiteSpaces(type.brand_str);
      sCodeName += RemoveMultipleWhiteSpaces(type.cpu_codename);
    }
    cpudata.CPUBrandString = sBrand;
    cpudata.CPUCodeName = sCodeName;
    cpudata.CPUCores = iCores;
    cpudata.CPULogicalCores = iLogical;
  }

  cpuid_free_system_id(&system);
  cpuid_free_raw_data_array(&raw_all);
}
