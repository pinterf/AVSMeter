#if !defined(_GPUINFO_H)
#define _GPUINFO_H

/*
	This file is part of AVSMeter, Copyright(C) Groucho2004.

	AVSMeter is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	AVSMeter is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with AVSMeter. If not, see <http://www.gnu.org/licenses/>.
	GPU-Z shared memory interface by W1zzard from techpowerup
*/

#include "common.h"
#include "exception.h"
#include "utility.h"

#define SHMEM_NAME "GPUZShMem"
#define MAX_RECORDS 128

class CGPUInfo
{
public:
	CGPUInfo();
	virtual ~CGPUInfo();

	struct Sensors
	{
		DWORD GPUClock;
		DWORD MEMClock;
		BYTE  GPULoad;
		BYTE  VPULoad;
		DWORD MemoryUsedGeneral;
		DWORD MemoryUsedDedicated;
		DWORD MemoryUsedDynamic;
	} sensors;

	struct Data
	{
		string GPUZVersion;
		string CardName;
		string GPUName;
		string VPEngineName;
		string MemSize;
		string OpenCLVersion;
		string DriverVersion;
		BOOL   NVVPU;
		BOOL   GeneralMem;
		BOOL   DynamicMem;
		BOOL   DedicatedMem;
	} data;

	void Initialize();
	void MemRelease();
	void ReadSensors();
	void ReadData();
	BOOL bInitialized;
	string sError;

private:
	#pragma pack(push, 1)
	typedef struct GPUZ_RECORD
	{
		WCHAR key[256];
		WCHAR value[256];
	}	GPUZ_RECORD;

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

	HANDLE hMapFile;
	LPVOID pMapAddr;
	map <string, string> mVPUNames;
	CUtils utils;
};


CGPUInfo::CGPUInfo()
{
	bInitialized = FALSE;
	sensors.GPULoad = 0;
	sError = "";
	pMapAddr = NULL;

	mVPUNames.clear();
	mVPUNames["G96"]   = "VP2";
	mVPUNames["G84"]   = "VP2";
	mVPUNames["G86"]   = "VP2";
	mVPUNames["G92"]   = "VP2";
	mVPUNames["GT200"] = "VP2";
	mVPUNames["G94"]   = "VP2";
	mVPUNames["G96"]   = "VP3";
	mVPUNames["C77"]   = "VP3";
	mVPUNames["G98"]   = "VP3";
	mVPUNames["C79"]   = "VP3";
	mVPUNames["GT216"] = "VP4";
	mVPUNames["GT218"] = "VP4";
	mVPUNames["GF106"] = "VP4";
	mVPUNames["GF108"] = "VP4";
	mVPUNames["GF110"] = "VP4";
	mVPUNames["GF100"] = "VP4";
	mVPUNames["GF116"] = "VP4";
	mVPUNames["GF114"] = "VP4";
	mVPUNames["GF104"] = "VP4";
	mVPUNames["GT215"] = "VP4";
	mVPUNames["GF117"] = "VP5";
	mVPUNames["GK104"] = "VP5";
	mVPUNames["GK208"] = "VP5";
	mVPUNames["GK110"] = "VP5";
	mVPUNames["GF119"] = "VP5";
	mVPUNames["GK107"] = "VP5";
	mVPUNames["GK106"] = "VP5";
	mVPUNames["GM204"] = "VP6";
	mVPUNames["GM200"] = "VP6";
	mVPUNames["GM108"] = "VP6";
	mVPUNames["GM107"] = "VP6";
	mVPUNames["GM206"] = "VP7";
	mVPUNames["GP104"] = "VP8";
	mVPUNames["GP107"] = "VP8";
	mVPUNames["GP106"] = "VP8";
	mVPUNames["GP102"] = "VP8";
}

CGPUInfo::~CGPUInfo()
{
}


void CGPUInfo::Initialize()
{
	sError = "";
	string sTemp = "";
	bInitialized = FALSE;
	if ((hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, SHMEM_NAME)) == 0)
	{
		sError = utils.SysErrorMessage();
		DWORD dwLastError = GetLastError();
		if (sError == "")
			sError = "Error initializing GPU-Z memory interface";
		else
		{
			if (dwLastError == ERROR_FILE_NOT_FOUND)
				sError += "\nMake sure that GPU-Z is running.\n";
		}

		return;
	}

	Sleep(100);

	if ((pMapAddr = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0)) == 0)
	{
		sError = utils.SysErrorMessage();
		if (sError == "")
			sError = "Error initializing GPU-Z memory interface";

		if (hMapFile)
			CloseHandle(hMapFile);

		return;
	}

	Sleep(100);
	bInitialized = TRUE;
	ReadData();

	return;
}


void CGPUInfo::MemRelease()
{
	if (!bInitialized)
		return;

	if (hMapFile)
		CloseHandle(hMapFile);

	pMapAddr = NULL;
	hMapFile = NULL;

	return;
}


void CGPUInfo::ReadData()
{
	LPGPUZ_SH_MEM lpMem = (LPGPUZ_SH_MEM)pMapAddr;

	BYTE nCUDA = 0;
	double dCUDACaps = 0.0;
	data.NVVPU = FALSE;
	data.GeneralMem = FALSE;
	data.DedicatedMem = FALSE;
	data.DynamicMem = FALSE;

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

		if (sKey == "")
			continue;

		if (sValue == "")
			continue;

		if (sKey == "CARDNAME")	       data.CardName = sValue;
		if (sKey == "GPUNAME")         data.GPUName = sValue;
		if (sKey == "MEMSIZE")         data.MemSize = sValue;
		if (sKey == "DRIVERVERSION")   data.DriverVersion = sValue;
		if (sKey == "OPENCL_VERSION")  data.OpenCLVersion = sValue;
		if (sKey == "CUDA")	           nCUDA = (BYTE)atol(sValue.c_str());
		if (sKey == "CUDA_CAPABILITY") dCUDACaps = atof(sValue.c_str());
	}

	if ((nCUDA != 0) && (dCUDACaps >= 1.0))
		data.NVVPU = TRUE;

	if (mVPUNames.find(data.GPUName) == mVPUNames.end())
		data.VPEngineName = "";
	else
		data.VPEngineName = mVPUNames.find(data.GPUName)->second;

	return;
}


void CGPUInfo::ReadSensors()
{
	LPGPUZ_SH_MEM lpMem = (LPGPUZ_SH_MEM)pMapAddr;

	sensors.GPULoad = 0;
	sensors.MemoryUsedGeneral = 0;
	sensors.MemoryUsedDedicated = 0;
	sensors.MemoryUsedDynamic = 0;
	sensors.VPULoad = 0;

	for (int i = 0; i < MAX_RECORDS; i++)
	{
		if (wcslen(lpMem->sensors[i].name) < 2)
			continue;

		if (_wcsicmp(lpMem->sensors[i].name, L"GPU Core Clock") == 0)
		{
			sensors.GPUClock = (WORD)(lpMem->sensors[i].value + 0.5);
			continue;
		}
		if (_wcsicmp(lpMem->sensors[i].name, L"GPU Memory Clock") == 0)
		{
			sensors.MEMClock = (WORD)(lpMem->sensors[i].value + 0.5);
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
			data.GeneralMem = TRUE;
			sensors.MemoryUsedGeneral = (DWORD)(lpMem->sensors[i].value + 0.5);
			continue;
		}
		if (_wcsicmp(lpMem->sensors[i].name, L"Memory Usage (Dedicated)") == 0)
		{
			data.DedicatedMem = TRUE;
			sensors.MemoryUsedDedicated = (DWORD)(lpMem->sensors[i].value + 0.5);	
			continue;
		}
		if (_wcsicmp(lpMem->sensors[i].name, L"Memory Usage (Dynamic)") == 0)
		{
			data.DynamicMem = TRUE;
			sensors.MemoryUsedDynamic = (DWORD)(lpMem->sensors[i].value + 0.5);	
		}
	}

	return;
}


#endif //_GPUINFO_H

