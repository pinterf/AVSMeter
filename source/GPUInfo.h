#ifndef _GPUINFO_H
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

#define SHMEM_NAME "GPUZShMem"
#define MAX_RECORDS 128

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
static BOOL bGPUZRunning = FALSE;
static string GPUZ_Version = "Unknown";

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

	BOOL MemInit();
	void MemRelease();
	void ReadSensors();
	void ReadData();
	BOOL bGPUZIsRunning;
	BOOL bInitialized;
	string sError;

private:
	unsigned __int64 GetSTDTimer();
	void StrToUC(string &s_string);
	BOOL IsValidVersionNumber(string s_string);

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
};

CGPUInfo::CGPUInfo()
{
	EnumWindows(EnumWindowsProc, NULL);
	Sleep(100);
	bGPUZIsRunning = bGPUZRunning;
	bInitialized = FALSE;
	sensors.GPULoad = 0;
	sError = "";
	pMapAddr = NULL;

	mVPUNames.clear();
	mVPUNames["G86"]   = "VP2";
	mVPUNames["G84"]   = "VP2";
	mVPUNames["G92"]   = "VP2";
	mVPUNames["G94"]   = "VP2";
	mVPUNames["GT200"] = "VP2";
	mVPUNames["G96"]   = "VP2";
	mVPUNames["G98"]   = "VP3";
	mVPUNames["C77"]   = "VP3";
	mVPUNames["G96"]   = "VP3";
	mVPUNames["G98"]   = "VP3";
	mVPUNames["C79"]   = "VP3";
	mVPUNames["GT218"] = "VP4";
	mVPUNames["GT216"] = "VP4";
	mVPUNames["GT215"] = "VP4";
	mVPUNames["GF100"] = "VP4";
	mVPUNames["GF104"] = "VP4";
	mVPUNames["GF108"] = "VP4";
	mVPUNames["GF106"] = "VP4";
	mVPUNames["GF110"] = "VP4";
	mVPUNames["GF114"] = "VP4";
	mVPUNames["GF116"] = "VP4";
	mVPUNames["GT218"] = "VP4";
	mVPUNames["GF119"] = "VP5";
	mVPUNames["GF117"] = "VP5";
	mVPUNames["GK107"] = "VP5";
	mVPUNames["GK104"] = "VP5";
	mVPUNames["GK106"] = "VP5";
	mVPUNames["GK110"] = "VP5";
	mVPUNames["GK208"] = "VP5";
	mVPUNames["GM107"] = "VP6";
	mVPUNames["GM108"] = "VP6";
	mVPUNames["GM204"] = "VP6";
	mVPUNames["GM200"] = "VP6";
	mVPUNames["GM206"] = "VP7";
	mVPUNames["GP104"] = "VP8";
	mVPUNames["GP106"] = "VP8";
	mVPUNames["GP102"] = "VP8";
}

CGPUInfo::~CGPUInfo()
{
}


BOOL CGPUInfo::MemInit()
{
	sError = "";

	if ((hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, SHMEM_NAME)) == 0)
	{
		sError = SysErrorMessage();
		return FALSE;
	}

	if ((pMapAddr = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0)) == 0)
	{
		sError = SysErrorMessage();
		if (hMapFile)
			CloseHandle(hMapFile);

		return FALSE;
	}

	bInitialized = TRUE;
	ReadData();

	return TRUE;
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

	if (IsValidVersionNumber(GPUZ_Version))
		data.GPUZVersion = GPUZ_Version;
	else
		data.GPUZVersion = "Unknown";

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
		StrToUC(sKey);

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
	static unsigned __int64 uiLastTimeRun;
	if ((GetSTDTimer() - uiLastTimeRun) < 200)
		return;

	LPGPUZ_SH_MEM lpMem = (LPGPUZ_SH_MEM)pMapAddr;

	sensors.GPULoad = 0;
	sensors.MemoryUsedGeneral = 0;
	sensors.MemoryUsedDedicated = 0;
	sensors.MemoryUsedDynamic = 0;
	sensors.VPULoad = 0;

	for (int i = 0; i < MAX_RECORDS; i++)
	{
		char DefChar = ' ';
		char szSensor[512];
		WideCharToMultiByte(CP_ACP, 0, lpMem->sensors[i].name, -1, szSensor, 511, &DefChar, NULL);
		string sSensor(szSensor);
		sSensor.erase(sSensor.find_last_not_of(" \t\n\r") + 1);
		sSensor.erase(0, sSensor.find_first_not_of(" \t\n\r"));
		StrToUC(sSensor);

		if (sSensor == "GPU CORE CLOCK")
			sensors.GPUClock = (WORD)(lpMem->sensors[i].value + 0.5);
		if (sSensor == "GPU MEMORY CLOCK")
			sensors.MEMClock = (WORD)(lpMem->sensors[i].value + 0.5);
		if (sSensor == "GPU LOAD")
			sensors.GPULoad = (BYTE)(lpMem->sensors[i].value + 0.5);
		if ((data.NVVPU) && (sSensor == "VIDEO ENGINE LOAD"))
			sensors.VPULoad = (BYTE)(lpMem->sensors[i].value + 0.5);
		if (sSensor == "MEMORY USED")
		{
			data.GeneralMem = TRUE;
			sensors.MemoryUsedGeneral = (DWORD)(lpMem->sensors[i].value + 0.5);
		}
		if (sSensor == "MEMORY USAGE (DEDICATED)")
		{
			data.DedicatedMem = TRUE;
			sensors.MemoryUsedDedicated = (DWORD)(lpMem->sensors[i].value + 0.5);
		}
		if (sSensor == "MEMORY USAGE (DYNAMIC)")
		{
			data.DynamicMem = TRUE;
			sensors.MemoryUsedDynamic = (DWORD)(lpMem->sensors[i].value + 0.5);
		}
	}

	uiLastTimeRun = GetSTDTimer();

	return;
}


static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
	char szAppTitle[64];
	GetWindowText(hwnd, szAppTitle, sizeof(szAppTitle));
	string sAppTitle(szAppTitle);

	for (unsigned int i = 0; i < sAppTitle.length(); i++)
		sAppTitle[i] = tolower(sAppTitle[i]);

	int iPos = (int)sAppTitle.find("gpu-z");

	if (iPos >= 0)
	{
		bGPUZRunning = TRUE;
		GPUZ_Version = sAppTitle.substr(sAppTitle.length() - 5);
	}

	return TRUE;
}


unsigned __int64 CGPUInfo::GetSTDTimer()
{
	static unsigned __int64 uiLastSTDTimer;
	unsigned __int64 uiSTDTimer = (unsigned __int64)GetTickCount();
	unsigned __int64 uiTimerWrapComp = (uiSTDTimer < uiLastSTDTimer) ? 4294967296 : 0;
	uiLastSTDTimer = uiSTDTimer;

	return (uiSTDTimer + uiTimerWrapComp);
}


BOOL CGPUInfo::IsValidVersionNumber(string s_string)
{
	if (s_string == "")
		return FALSE;

	for (unsigned int ipos = 0; ipos < s_string.length(); ipos++)
	{
		switch (s_string[ipos])
		{
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case '.':
				continue;
			default:
				return FALSE;
		}
	}

	return TRUE;
}


void CGPUInfo::StrToUC(string &s_string)
{
	for (unsigned int i = 0; i < s_string.length(); i++)
		s_string[i] = toupper(s_string[i]);

	return;
}

#endif //_GPUINFO_H
