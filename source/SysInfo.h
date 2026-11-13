#if !defined(_SYSINFO_H)
#define _SYSINFO_H

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
*/

#include "common.h"
#include "cpuid\libcpuid.h"

class CSysInfo
{
public:
	CSysInfo();
	virtual        ~CSysInfo();
	DWORD          GetTotalPhysicalMemory(); //MB
	string         GetOSVersion();
	string         GetFormattedSystemTime();

	//CPUID
	void           GetCPUID();
	BOOL           bCPUIDSuccess;
	struct CPUData
	{
		string       CPUBrandString;
		string       CPUCodeName;
		BYTE         CPUCores;
		BYTE         CPULogicalCores;
		BYTE         NumCPUs;
		BYTE         CPUTotalLogicalCores;
		unsigned int CPURealClock; //MHz
	} cpudata;

private:
	string         RemoveMultipleWhiteSpaces(string s);
	string         StrFormat(char const *fmt, ...);
};


#endif //_SYSINFO_H

