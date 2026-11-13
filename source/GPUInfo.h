// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#ifndef _GPUINFO_H
#define _GPUINFO_H

#include "stdafx.h"

#define SHMEM_NAME "GPUZShMem"
#define MAX_RECORDS 128

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
static BOOL bGPUZRunning = FALSE;

class CGPUInfo
{
public:
	CGPUInfo();
	virtual ~CGPUInfo();

	struct Sensors
	{
		WORD GPULoad;
		WORD VPULoad;
		DWORD MemoryUsed;
	} sensors;

	struct Data
	{
		string CardName;
		string GPUName;
		string MemSize;
		string OpenCLVersion;
		string DriverVersion;
		BOOL   NVVP;
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
	unsigned __int64 uiLastSTDTimer;
	string OSErrorMessage();
	unsigned __int64 uiLastTimeRun;

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
		UINT32 version;			// Version number, 1 for the struct here
		volatile LONG busy;		// Is data being accessed?
		UINT32 lastUpdate;		// GetTickCount() of last update
		GPUZ_RECORD data[MAX_RECORDS];
		GPUZ_SENSOR_RECORD sensors[MAX_RECORDS];
	} GPUZ_SH_MEM, *LPGPUZ_SH_MEM;

	#pragma pack(pop)

	HANDLE hMapFile;
	LPVOID pMapAddr;
};

CGPUInfo::CGPUInfo()
{
	EnumWindows(EnumWindowsProc, NULL);
	Sleep(100);
	bGPUZIsRunning = bGPUZRunning;
	bInitialized = FALSE;
	sensors.GPULoad = 0;
	uiLastSTDTimer = 0;
	uiLastTimeRun = 0;
	sError = "";
	pMapAddr = NULL;
}

CGPUInfo::~CGPUInfo()
{
}


BOOL CGPUInfo::MemInit()
{
	sError = "";

	if ((hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, SHMEM_NAME)) == 0)
	{
		sError = OSErrorMessage();
		return FALSE;
	}

	if ((pMapAddr = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0)) == 0)
	{
		sError = OSErrorMessage();
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
	data.NVVP = FALSE;

	for (int i = 0; i < 128; i++)
	{
		char DefChar = ' ';
		char szvalue[260];
		WideCharToMultiByte(CP_ACP, 0, lpMem->data[i].value, -1, szvalue, 260, &DefChar, NULL);
		string sValue(szvalue);
		sValue.erase(sValue.find_last_not_of(" \t\n\r") + 1);
		sValue.erase(0, sValue.find_first_not_of(" \t\n\r"));
		
		if (wcscmp(lpMem->data[i].key, L"CardName") == 0)
			data.CardName = sValue;
		if (wcscmp(lpMem->data[i].key, L"GPUName") == 0)
			data.GPUName = sValue;
		if (wcscmp(lpMem->data[i].key, L"MemSize") == 0)
			data.MemSize = sValue;
		if (wcscmp(lpMem->data[i].key, L"DriverVersion") == 0)
			data.DriverVersion = sValue;
		if (wcscmp(lpMem->data[i].key, L"OpenCL_Version") == 0)
			data.OpenCLVersion = sValue;
		if (wcscmp(lpMem->data[i].key, L"CUDA") == 0)
			nCUDA = (BYTE)atol(sValue.c_str());		
		if (wcscmp(lpMem->data[i].key, L"CUDA_Capability") == 0)
			dCUDACaps = atof(sValue.c_str());		
	}
	
	if ((nCUDA != 0) && (dCUDACaps >= 1.0))
		data.NVVP = TRUE;

	return;
}


void CGPUInfo::ReadSensors()
{
	//Min. update interval 100ms
	if ((GetSTDTimer() - uiLastTimeRun) < 100)
		return;

	LPGPUZ_SH_MEM lpMem = (LPGPUZ_SH_MEM)pMapAddr;

	WORD bDone = 0x0000;
	for (int i = 0; i <128; i++)
	{
		if (wcscmp(lpMem->sensors[i].name, L"GPU Load") == 0)
		{
			sensors.GPULoad = (WORD)(lpMem->sensors[i].value + 0.5);
			bDone |= 0x000F;
		}
		if (wcscmp(lpMem->sensors[i].name, L"Memory Used") == 0)
		{
			sensors.MemoryUsed = (DWORD)(lpMem->sensors[i].value + 0.5);	
			bDone |= 0x00F0;
		}
		if ((data.NVVP) && (wcscmp(lpMem->sensors[i].name, L"Video Engine Load") == 0))
		{
			sensors.VPULoad = (WORD)(lpMem->sensors[i].value + 0.5);
			bDone |= 0x0F00;
		}

		if (bDone == 0x0FFF)
			break;
	}

	uiLastTimeRun = GetSTDTimer();

	return;
}


string CGPUInfo::OSErrorMessage()
{
	LPVOID lpf;
	DWORD dwLastError = GetLastError();

	if (dwLastError == ERROR_SUCCESS)
		return "";
		
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwLastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),	(LPTSTR)&lpf, 0,	NULL);
	
	return (LPCTSTR)lpf;
}


static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
	char lpAppTitle[256];
	GetWindowText(hwnd, lpAppTitle, sizeof(lpAppTitle));	
	string sAppTitle(lpAppTitle);

	for (unsigned int i = 0; i < sAppTitle.length(); i++)
		sAppTitle[i] = tolower(sAppTitle[i]);

	int iPos = (int)sAppTitle.find("gpu-z");
	
	if (iPos >= 0)
		bGPUZRunning = TRUE;

	return TRUE;
}


unsigned __int64 CGPUInfo::GetSTDTimer()
{
	unsigned __int64 uiSTDTimer = (unsigned __int64)GetTickCount();
	unsigned __int64 uiTimerWrapComp = (uiSTDTimer < uiLastSTDTimer) ? 4294967296 : 0;
	uiLastSTDTimer = uiSTDTimer;

	return (uiSTDTimer + uiTimerWrapComp);
}


#endif //_GPUINFO_H

