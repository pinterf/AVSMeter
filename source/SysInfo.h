/*
	This file is part of AVSMeter, Copyright(C) Groucho2004.

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


#if !defined(_SYSINFO_H)
#define _SYSINFO_H

#include "common.h"
#include "cpuid\libcpuid.h"

class CSysInfo
{
public:
	CSysInfo();
	virtual        ~CSysInfo();
	string         GetOSVersion();
	string         GetFormattedSystemDateTime();
	BOOL           Is64BitOS();

	//CPUID
	void           GetCPUID();
	BOOL           bCPUIDSuccess;
	struct CPUData
	{
		string       CPUBrandString;
		string       CPUCodeName;
		int32_t      CPUCores;
		int32_t      CPULogicalCores;
		int32_t      NumCPUs;
		int32_t      CPUTotalLogicalCores;
		string       CPUSupportedInstructionSets;
	} cpudata;

private:
	string         RemoveMultipleWhiteSpaces(string s);
	string         StrFormat(char const *fmt, ...);
};


CSysInfo::CSysInfo()
{
}

CSysInfo::~CSysInfo()
{
}


string CSysInfo::GetOSVersion()
{
	typedef LONG NTSTATUS, *PNTSTATUS;
	typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOEXW);
	string sOSVersion = "Unknown OS Version";
	string sOSAddendum = "";

	string sArchitecture = "(x86)";
	if (Is64BitOS())
		sArchitecture = "(x64)";

	HMODULE hMod = ::GetModuleHandle("ntdll.dll");
	if (!hMod)
		return "Cannot get module handle to ntdll.dll";

	RtlGetVersionPtr rgvPtr = (RtlGetVersionPtr)::GetProcAddress(hMod, "RtlGetVersion");
	if (rgvPtr == NULL)
		return "Cannot invoke \"RtlGetVersion\"";

	RTL_OSVERSIONINFOEXW osvi = {0};
	osvi.dwOSVersionInfoSize = sizeof(osvi);
	if (rgvPtr(&osvi) != 0)
		return "Cannot invoke \"RtlGetVersion\"";

	if (osvi.dwMajorVersion == 5)
	{
		if (osvi.wProductType == VER_NT_WORKSTATION)
			sOSVersion = StrFormat("Windows XP %s", sArchitecture.c_str());
		else
			sOSVersion = StrFormat("Windows Server 2003 %s", sArchitecture.c_str());
	}

	if (osvi.dwMajorVersion == 6)
	{
		if (osvi.dwMinorVersion == 0)
		{
			if (osvi.wProductType == VER_NT_WORKSTATION)
				sOSVersion = StrFormat("Windows Vista %s", sArchitecture.c_str());
			else
				sOSVersion = StrFormat("Windows Server 2008 %s", sArchitecture.c_str());
		}
		if (osvi.dwMinorVersion == 1)
		{
			if (osvi.wProductType == VER_NT_WORKSTATION)
				sOSVersion = StrFormat("Windows 7 %s", sArchitecture.c_str());
			else
				sOSVersion = StrFormat("Windows Server 2008 R2 %s", sArchitecture.c_str());
		}
		if (osvi.dwMinorVersion == 2)
		{
			if (osvi.wProductType == VER_NT_WORKSTATION)
				sOSVersion = StrFormat("Windows 8 %s", sArchitecture.c_str());
			else
				sOSVersion = StrFormat("Windows Server 2012 %s", sArchitecture.c_str());
		}
		if (osvi.dwMinorVersion == 3)
		{
			if (osvi.wProductType == VER_NT_WORKSTATION)
				sOSVersion = StrFormat("Windows 8.1 %s", sArchitecture.c_str());
			else
				sOSVersion = StrFormat("Windows Server 2012 R2 %s", sArchitecture.c_str());
		}
	}

	if (osvi.dwMajorVersion == 10)
	{
		if (osvi.dwMinorVersion == 0)
			sOSVersion = StrFormat("Windows 10 %s", sArchitecture.c_str());
	}

	if ((osvi.wServicePackMajor > 0) || (osvi.wServicePackMinor > 0))
		sOSAddendum = StrFormat(" Service Pack %u.%u (Build %u)", osvi.wServicePackMajor, osvi.wServicePackMinor, osvi.dwBuildNumber);
	else
		sOSAddendum = StrFormat(" (Build %u)", osvi.dwBuildNumber);

	return sOSVersion + sOSAddendum;
}


string CSysInfo::GetFormattedSystemDateTime()
{
	SYSTEMTIME st;
	GetLocalTime(&st);

	return StrFormat("%04d%02d%02d-%02d%02d%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}


string CSysInfo::RemoveMultipleWhiteSpaces(string s)
{
	string search = "  ";
	size_t index;
	while((index = s.find(search)) != string::npos) s.erase(index,1);

	return s;
}


BOOL CSysInfo::Is64BitOS()
{
	if (sizeof(void*) == 8)
		return TRUE; //64 on 64

	BOOL bWoW64Process = FALSE;
	typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
	LPFN_ISWOW64PROCESS fnIsWow64Process;
	HMODULE hKernel32 = GetModuleHandle("kernel32.dll");
	if (hKernel32 == NULL)
		return FALSE;

	fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(hKernel32, "IsWow64Process");
	if (fnIsWow64Process != NULL)
		fnIsWow64Process(GetCurrentProcess(), &bWoW64Process);
	if (bWoW64Process)
		return TRUE;

	return FALSE;
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
	cpudata.CPUCodeName = StrFormat("%s", data.cpu_codename);
	cpudata.CPUCodeName = RemoveMultipleWhiteSpaces(cpudata.CPUCodeName);
	cpudata.CPUBrandString = StrFormat("%s", data.brand_str);
	cpudata.CPUBrandString = RemoveMultipleWhiteSpaces(cpudata.CPUBrandString);

	cpudata.CPUSupportedInstructionSets = "";
	if (data.flags[CPU_FEATURE_3DNOW])     cpudata.CPUSupportedInstructionSets += "3DNOW, ";
	if (data.flags[CPU_FEATURE_3DNOWEXT])  cpudata.CPUSupportedInstructionSets += "3DNOWEXT, ";
	if (data.flags[CPU_FEATURE_MMX])       cpudata.CPUSupportedInstructionSets += "MMX, ";
	if (data.flags[CPU_FEATURE_MMXEXT])    cpudata.CPUSupportedInstructionSets += "MMXEXT, ";
	if (data.flags[CPU_FEATURE_SSE])       cpudata.CPUSupportedInstructionSets += "SSE, ";
	if (data.flags[CPU_FEATURE_SSE2])      cpudata.CPUSupportedInstructionSets += "SSE2, ";
	if (data.flags[CPU_FEATURE_PNI])       cpudata.CPUSupportedInstructionSets += "SSE3, ";
	if (data.flags[CPU_FEATURE_SSSE3])     cpudata.CPUSupportedInstructionSets += "SSSE3, ";
	if (data.flags[CPU_FEATURE_SSE4_1])    cpudata.CPUSupportedInstructionSets += "SSE4.1, ";
	if (data.flags[CPU_FEATURE_SSE4_2])    cpudata.CPUSupportedInstructionSets += "SSE4.2, ";
	if (data.flags[CPU_FEATURE_SSE4A])     cpudata.CPUSupportedInstructionSets += "SSE4A, ";
	if (data.flags[CPU_FEATURE_FMA3])      cpudata.CPUSupportedInstructionSets += "FMA3, ";
	if (data.flags[CPU_FEATURE_FMA4])      cpudata.CPUSupportedInstructionSets += "FMA4, ";
	if (data.flags[CPU_FEATURE_AVX])       cpudata.CPUSupportedInstructionSets += "AVX, ";
	if (data.flags[CPU_FEATURE_AVX2])      cpudata.CPUSupportedInstructionSets += "AVX2, ";
	if (data.flags[CPU_FEATURE_F16C])      cpudata.CPUSupportedInstructionSets += "F16C, ";
	if (data.flags[CPU_FEATURE_SHA_NI])    cpudata.CPUSupportedInstructionSets += "SHA1/SHA256, ";
	if (data.flags[CPU_FEATURE_AVX512F])   cpudata.CPUSupportedInstructionSets += "AVX512F, ";
	if (data.flags[CPU_FEATURE_AVX512DQ])  cpudata.CPUSupportedInstructionSets += "AVX512DQ, ";
	if (data.flags[CPU_FEATURE_AVX512PF])  cpudata.CPUSupportedInstructionSets += "AVX512PF, ";
	if (data.flags[CPU_FEATURE_AVX512ER])  cpudata.CPUSupportedInstructionSets += "AVX512ER, ";
	if (data.flags[CPU_FEATURE_AVX512CD])  cpudata.CPUSupportedInstructionSets += "AVX512CD, ";
	if (data.flags[CPU_FEATURE_AVX512BW])  cpudata.CPUSupportedInstructionSets += "AVX512BW, ";
	if (data.flags[CPU_FEATURE_AVX512VL])  cpudata.CPUSupportedInstructionSets += "AVX512VL, ";
	if (data.flags[CPU_FEATURE_ABM])       cpudata.CPUSupportedInstructionSets += "LZCNT, ";
	if (data.flags[CPU_FEATURE_BMI1])      cpudata.CPUSupportedInstructionSets += "BMI1, ";
	if (data.flags[CPU_FEATURE_BMI2])      cpudata.CPUSupportedInstructionSets += "BMI2, ";

	cpudata.CPUSupportedInstructionSets.erase(cpudata.CPUSupportedInstructionSets.find_last_not_of(", ") + 1);

	bCPUIDSuccess = TRUE;

	return;
}


string CSysInfo::StrFormat(char const *fmt, ...)
{
	#define MIN_BUF_SIZE   256
	#define MAX_BUF_SIZE 16384
	va_list args;
	va_start(args, fmt);
	vector<char> buffer(MIN_BUF_SIZE);
	int iRet = -1;

	for (;;)
	{
		iRet = _vsnprintf(&buffer[0], buffer.size() - 1, fmt, args);
		if (iRet == -1)
			buffer.resize(buffer.size() * 2);
		else
			break;

		if (buffer.size() > MAX_BUF_SIZE)
		{
			va_end(args);
			return "buffer overflow";
		}
	}

	buffer[buffer.size() - 1] = '\0';
	string sRet = &buffer[0];
	va_end(args);

	return sRet;
}


#endif //_SYSINFO_H
