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
};


#endif //_GPUINFO_H

