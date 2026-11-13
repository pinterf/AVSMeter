// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#ifndef _GPUINFO_H
#define _GPUINFO_H

#include "stdafx.h"

using namespace std;

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
	} sensors;

	BOOL ReadSensors();
	BOOL bGPUZIsRunning;
	string sError;

private:
	string OSErrorMessage();
	DWORD dwLastTimeRun;

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
};

CGPUInfo::CGPUInfo()
{
	EnumWindows(EnumWindowsProc, NULL);
	Sleep(100);
	bGPUZIsRunning = bGPUZRunning;
	sensors.GPULoad = 0;
	dwLastTimeRun = GetTickCount();
	sError = "";
}

CGPUInfo::~CGPUInfo()
{
}


BOOL CGPUInfo::ReadSensors()
{
	sError = "";
	//Min. update interval 500ms
	if ((::GetTickCount() - dwLastTimeRun) < 500)
		return TRUE;

	HANDLE hMapFile;
	LPVOID pMapAddr;
	LPGPUZ_SH_MEM lpMem;

	sensors.GPULoad = 0;
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

	lpMem = (LPGPUZ_SH_MEM)pMapAddr;

	for (int i = 0; i <128; i++)
	{
		if (wcscmp(lpMem->sensors[i].name, L"GPU Load") == 0)
		{
			sensors.GPULoad = (WORD)(lpMem->sensors[i].value + 0.5);	
			break;
		}
	}

	if (hMapFile)
		CloseHandle(hMapFile);

	lpMem = NULL;
	pMapAddr = NULL;
	hMapFile = NULL;

	dwLastTimeRun = GetTickCount();

	return TRUE;
}


string CGPUInfo::OSErrorMessage()
{
	LPVOID lpf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),	(LPTSTR)&lpf, 0, NULL);
	
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


#endif //_GPUINFO_H

