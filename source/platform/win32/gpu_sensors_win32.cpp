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


#include "platform/gpu_sensors.h"
#include "Utility.h"
#include "platform/win32/utf_win32.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cwchar>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


namespace
{
  // GPU-Z shared memory layout, version 1
  const char *SHMEM_NAME = "GPUZShMem";
  const int MAX_RECORDS = 128;

  #pragma pack(push, 1)

  typedef struct GPUZ_RECORD
  {
    WCHAR key[256];
    WCHAR value[256];
  } GPUZ_RECORD;

  typedef struct GPUZ_SENSOR_RECORD
  {
    WCHAR name[256];
    WCHAR unit[8];
    UINT32 digits;
    double value;
  } GPUZ_SENSOR_RECORD;

  typedef struct GPUZ_SH_MEM
  {
    UINT32 version;      // Version number, 1 for the struct here
    volatile LONG busy;  // Is data being accessed?
    UINT32 lastUpdate;   // GetTickCount() of last update
    GPUZ_RECORD data[MAX_RECORDS];
    GPUZ_SENSOR_RECORD sensors[MAX_RECORDS];
  } GPUZ_SH_MEM, *LPGPUZ_SH_MEM;

  #pragma pack(pop)
}


platform::GpuSensors::GpuSensors()
{
  sensors.GPULoad = 0;
  sensors.VPULoad = 0;
  sensors.PowerConsumption = 0.0;
  sensors.MemoryUsedGeneral = 0;
  sensors.MemoryUsedDedicated = 0;
  sensors.MemoryUsedDynamic = 0;
  sensors.ReadError = false;
  sError = "";
  bInitialized = false;
  pMapAddr = NULL;
  hMapFile = NULL;
}

platform::GpuSensors::~GpuSensors()
{
  Release();
}


void platform::GpuSensors::Init()
{
  sError = "";
  bInitialized = false;
  if ((hMapFile = OpenFileMappingA(FILE_MAP_ALL_ACCESS, false, SHMEM_NAME)) == 0)
  {
    DWORD dwLastError = GetLastError();
    sError = CUtils().SysErrorMessage();
    if (sError == "")
      sError = "Error initializing GPU-Z memory interface";
    else
    {
      if (dwLastError == ERROR_FILE_NOT_FOUND)
        sError += "\nMake sure that GPU-Z is running.\n\nGPU-Z can be downloaded here:\nhttps://www.techpowerup.com/download/gpu-z/";
    }

    sError = "Cannot initialize GPU-Z shared memory interface:\n" + sError;
    return;
  }

  if ((pMapAddr = MapViewOfFile((HANDLE)hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0)) == 0)
  {
    sError = CUtils().SysErrorMessage();
    if (sError == "")
      sError = "Error initializing GPU-Z memory interface";
    sError = "Cannot initialize GPU-Z shared memory interface:\n" + sError;
    if (hMapFile)
      CloseHandle((HANDLE)hMapFile);
    hMapFile = NULL;
    return;
  }

  bInitialized = true;
  ReadData();

  return;
}


void platform::GpuSensors::Release()
{
  if (!bInitialized)
    return;

  if (pMapAddr)
    UnmapViewOfFile(pMapAddr);
  if (hMapFile)
    CloseHandle((HANDLE)hMapFile);

  pMapAddr = NULL;
  hMapFile = NULL;
  bInitialized = false;

  return;
}


void platform::GpuSensors::ReadData()
{
  LPGPUZ_SH_MEM lpMem = (LPGPUZ_SH_MEM)pMapAddr;

  uint8_t nCUDA = 0;
  double dCUDACaps = 0.0;
  data.NVVPU = false;
  data.GeneralMem = false;
  data.DedicatedMem = false;
  data.DynamicMem = false;

  for (int i = 0; i < MAX_RECORDS; i++)
  {
    std::wstring wsKey(lpMem->data[i].key, wcsnlen(lpMem->data[i].key, 256));
    std::wstring wsValue(lpMem->data[i].value, wcsnlen(lpMem->data[i].value, 256));
    std::string sKey = platform::win32::WideToUtf8(wsKey.c_str());
    std::string sValue = platform::win32::WideToUtf8(wsValue.c_str());
    sKey.erase(sKey.find_last_not_of(" \t\n\r") + 1);
    sKey.erase(0, sKey.find_first_not_of(" \t\n\r"));
    sValue.erase(sValue.find_last_not_of(" \t\n\r") + 1);
    sValue.erase(0, sValue.find_first_not_of(" \t\n\r"));
    std::transform(sKey.begin(), sKey.end(), sKey.begin(), [](unsigned char c) { return (char)toupper(c); });

    if ((sValue == "") || (sValue == "0"))
      sValue = "n/a";

    if (sKey == "CARDNAME")        data.CardName = sValue;
    if (sKey == "GPUNAME")         data.GPUName = sValue;
    if (sKey == "MEMSIZE")         data.MemSize = sValue;
    if (sKey == "DRIVERVERSION")   data.DriverVersion = sValue;
    if (sKey == "OPENCL_VERSION")  data.OpenCLVersion = sValue;
    if (sKey == "CUDA")            nCUDA = (uint8_t)atol(sValue.c_str());
    if (sKey == "CUDA_CAPABILITY") dCUDACaps = atof(sValue.c_str());
  }

  if ((nCUDA != 0) && (dCUDACaps >= 1.0))
    data.NVVPU = true;

  return;
}


void platform::GpuSensors::ReadSensors()
{
  sensors.GPULoad = 0;
  sensors.VPULoad = 0;
  sensors.PowerConsumption = 0.0;
  sensors.MemoryUsedGeneral = 0;
  sensors.MemoryUsedDedicated = 0;
  sensors.MemoryUsedDynamic = 0;


  LPGPUZ_SH_MEM lpMem = (LPGPUZ_SH_MEM)pMapAddr;
  if (lpMem->version == 0)
    sensors.ReadError = true;


  for (int i = 0; i < MAX_RECORDS; i++)
  {
    if (wcslen(lpMem->sensors[i].name) < 2)
      continue;

    if (_wcsicmp(lpMem->sensors[i].name, L"Board Power Draw") == 0)
    {
      sensors.PowerConsumption = (double)(lpMem->sensors[i].value + 0.5);
      continue;
    }
    if (_wcsicmp(lpMem->sensors[i].name, L"Power Consumption (W)") == 0)
    {
      sensors.PowerConsumption = (double)(lpMem->sensors[i].value + 0.5);
      continue;
    }
    if (_wcsicmp(lpMem->sensors[i].name, L"GPU Load") == 0)
    {
      sensors.GPULoad = (uint8_t)(lpMem->sensors[i].value + 0.5);
      continue;
    }
    if ((data.NVVPU) && (_wcsicmp(lpMem->sensors[i].name, L"Video Engine Load") == 0))
    {
      sensors.VPULoad = (uint8_t)(lpMem->sensors[i].value + 0.5);
      continue;
    }
    if (_wcsicmp(lpMem->sensors[i].name, L"Memory Used") == 0)
    {
      data.GeneralMem = true;
      sensors.MemoryUsedGeneral = (uint32_t)(lpMem->sensors[i].value + 0.5);
      continue;
    }
    if (_wcsicmp(lpMem->sensors[i].name, L"Memory Usage (Dedicated)") == 0)
    {
      data.DedicatedMem = true;
      sensors.MemoryUsedDedicated = (uint32_t)(lpMem->sensors[i].value + 0.5);
      continue;
    }
    if (_wcsicmp(lpMem->sensors[i].name, L"Memory Usage (Dynamic)") == 0)
    {
      data.DynamicMem = true;
      sensors.MemoryUsedDynamic = (uint32_t)(lpMem->sensors[i].value + 0.5);
    }
  }

  return;
}
