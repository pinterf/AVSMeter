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


#include "GPUInfo.h"


CGPUInfo::CGPUInfo()
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

CGPUInfo::~CGPUInfo()
{
}


void CGPUInfo::GPUZInit()
{
  sError = "";
  string sTemp = "";
  bInitialized = false;
  if ((hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, false, SHMEM_NAME)) == 0)
  {
    sError = utils.SysErrorMessage();
    DWORD dwLastError = GetLastError();
    if (sError == "")
      sError = "Error initializing GPU-Z memory interface";
    else
    {
      if (dwLastError == ERROR_FILE_NOT_FOUND)
        sError += "\nMake sure that GPU-Z is running.\n\nGPU-Z can be downloaded here:\nhttps://www.techpowerup.com/download/gpu-z/";
    }

    return;
  }

  if ((pMapAddr = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0)) == 0)
  {
    sError = utils.SysErrorMessage();
    if (sError == "")
      sError = "Error initializing GPU-Z memory interface";
    if (hMapFile)
      CloseHandle(hMapFile);
    hMapFile = NULL;
    return;
  }

  bInitialized = true;
  ReadData();

  return;
}


void CGPUInfo::GPUZRelease()
{
  if (!bInitialized)
    return;

  if (pMapAddr)
    UnmapViewOfFile(pMapAddr);
  if (hMapFile)
    CloseHandle(hMapFile);

  pMapAddr = NULL;
  hMapFile = NULL;
  bInitialized = false;

  return;
}


void CGPUInfo::ReadData()
{
  LPGPUZ_SH_MEM lpMem = (LPGPUZ_SH_MEM)pMapAddr;

  BYTE nCUDA = 0;
  double dCUDACaps = 0.0;
  data.NVVPU = false;
  data.GeneralMem = false;
  data.DedicatedMem = false;
  data.DynamicMem = false;

  for (int i = 0; i < MAX_RECORDS; i++)
  {
    char DefChar = ' ';
    char szkey[512];
    char szvalue[512];
    WideCharToMultiByte(CP_ACP, 0, lpMem->data[i].key, -1, szkey, 511, &DefChar, NULL);
    WideCharToMultiByte(CP_ACP, 0, lpMem->data[i].value, -1, szvalue, 511, &DefChar, NULL);
    string sKey(szkey);
    string sValue(szvalue);
    sKey.erase(sKey.find_last_not_of(" \t\n\r") + 1);
    sKey.erase(0, sKey.find_first_not_of(" \t\n\r"));
    sValue.erase(sValue.find_last_not_of(" \t\n\r") + 1);
    sValue.erase(0, sValue.find_first_not_of(" \t\n\r"));
    transform(sKey.begin(), sKey.end(), sKey.begin(), ::toupper);

    if ((sValue == "") || (sValue == "0"))
      sValue = "n/a";

    if (sKey == "CARDNAME")        data.CardName = sValue;
    if (sKey == "GPUNAME")         data.GPUName = sValue;
    if (sKey == "MEMSIZE")         data.MemSize = sValue;
    if (sKey == "DRIVERVERSION")   data.DriverVersion = sValue;
    if (sKey == "OPENCL_VERSION")  data.OpenCLVersion = sValue;
    if (sKey == "CUDA")            nCUDA = (BYTE)atol(sValue.c_str());
    if (sKey == "CUDA_CAPABILITY") dCUDACaps = atof(sValue.c_str());
  }

  if ((nCUDA != 0) && (dCUDACaps >= 1.0))
    data.NVVPU = true;

  return;
}


void CGPUInfo::ReadSensors()
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
      sensors.GPULoad = (BYTE)(lpMem->sensors[i].value + 0.5);
      continue;
    }
    if ((data.NVVPU) && (_wcsicmp(lpMem->sensors[i].name, L"Video Engine Load") == 0))
    {
      sensors.VPULoad = (BYTE)(lpMem->sensors[i].value + 0.5);
      continue;
    }
    if (_wcsicmp(lpMem->sensors[i].name, L"Memory Used") == 0)
    {
      data.GeneralMem = true;
      sensors.MemoryUsedGeneral = (DWORD)(lpMem->sensors[i].value + 0.5);
      continue;
    }
    if (_wcsicmp(lpMem->sensors[i].name, L"Memory Usage (Dedicated)") == 0)
    {
      data.DedicatedMem = true;
      sensors.MemoryUsedDedicated = (DWORD)(lpMem->sensors[i].value + 0.5);
      continue;
    }
    if (_wcsicmp(lpMem->sensors[i].name, L"Memory Usage (Dynamic)") == 0)
    {
      data.DynamicMem = true;
      sensors.MemoryUsedDynamic = (DWORD)(lpMem->sensors[i].value + 0.5);
    }
  }

  return;
}
