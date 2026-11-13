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

#include "sysinfo.h"


CSysInfo::CSysInfo()
{
	bCPUIDSuccess = FALSE;
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
	SYSTEM_INFO stInfo;
	GetNativeSystemInfo(&stInfo);
	if ((stInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) || (stInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64))
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

	if (osvi.wServicePackMajor > 0)
		sOSAddendum = StrFormat(" Service Pack %u (Build %u)", osvi.wServicePackMajor, osvi.dwBuildNumber);
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

	int clk = cpu_clock_by_ic(100, 15);
	if (clk > 100)
		cpudata.CPURealClock = (unsigned int)clk;
	else
		cpudata.CPURealClock = 0;

	cpudata.CPUSupportedInstructionSets = "";
	if (data.flags[CPU_FEATURE_3DNOW])
		cpudata.CPUSupportedInstructionSets += "3DNOW, ";
	if (data.flags[CPU_FEATURE_3DNOWEXT])
		cpudata.CPUSupportedInstructionSets += "3DNOWEXT, ";
	if (data.flags[CPU_FEATURE_MMX])
		cpudata.CPUSupportedInstructionSets += "MMX, ";
	if (data.flags[CPU_FEATURE_MMXEXT])
		cpudata.CPUSupportedInstructionSets += "MMXEXT, ";
	if (data.flags[CPU_FEATURE_SSE])
		cpudata.CPUSupportedInstructionSets += "SSE, ";
	if (data.flags[CPU_FEATURE_SSE2])
		cpudata.CPUSupportedInstructionSets += "SSE2, ";
	if (data.flags[CPU_FEATURE_PNI])
		cpudata.CPUSupportedInstructionSets += "SSE3, ";
	if (data.flags[CPU_FEATURE_SSSE3])
		cpudata.CPUSupportedInstructionSets += "SSSE3, ";
	if (data.flags[CPU_FEATURE_SSE4_1])
		cpudata.CPUSupportedInstructionSets += "SSE4.1, ";
	if (data.flags[CPU_FEATURE_SSE4_2])
		cpudata.CPUSupportedInstructionSets += "SSE4.2, ";
	if (data.flags[CPU_FEATURE_SSE4A])
		cpudata.CPUSupportedInstructionSets += "SSE4A, ";
	if (data.flags[CPU_FEATURE_FMA3])
		cpudata.CPUSupportedInstructionSets += "FMA3, ";
	if (data.flags[CPU_FEATURE_FMA4])
		cpudata.CPUSupportedInstructionSets += "FMA4, ";
	if (data.flags[CPU_FEATURE_AVX])
		cpudata.CPUSupportedInstructionSets += "AVX, ";
	if (data.flags[CPU_FEATURE_AVX2])
		cpudata.CPUSupportedInstructionSets += "AVX2, ";
	if (data.flags[CPU_FEATURE_AVX512F])
		cpudata.CPUSupportedInstructionSets += "AVX512F, ";
	if (data.flags[CPU_FEATURE_AVX512DQ])
		cpudata.CPUSupportedInstructionSets += "AVX512DQ, ";
	if (data.flags[CPU_FEATURE_AVX512PF])
		cpudata.CPUSupportedInstructionSets += "AVX512PF, ";
	if (data.flags[CPU_FEATURE_AVX512ER])
		cpudata.CPUSupportedInstructionSets += "AVX512ER, ";
	if (data.flags[CPU_FEATURE_AVX512CD])
		cpudata.CPUSupportedInstructionSets += "AVX512CD, ";
	if (data.flags[CPU_FEATURE_AVX512BW])
		cpudata.CPUSupportedInstructionSets += "AVX512BW, ";
	if (data.flags[CPU_FEATURE_AVX512VL])
		cpudata.CPUSupportedInstructionSets += "AVX512VL, ";

	cpudata.CPUSupportedInstructionSets.erase(cpudata.CPUSupportedInstructionSets.find_last_not_of(", ") + 1);

	bCPUIDSuccess = TRUE;

	return;
}


string CSysInfo::StrFormat(char const *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	vector<char> buffer(256);
	int iRet = -1;
	while (iRet == -1)
	{
		iRet = _vsnprintf(&buffer[0], buffer.size() - 1, fmt, args);
		if (iRet == -1)
			buffer.resize(buffer.size() * 2);
		if (buffer.size() > 16384)
			return "buffer overflow";
	}
	buffer[buffer.size() - 1] = '\0';
	string sRet = &buffer[0];
	va_end(args);

	return sRet;
}


string CSysInfo::RemoveMultipleWhiteSpaces(string s)
{
	string search = "  ";
	size_t index;
	while((index = s.find(search)) != string::npos )
		s.erase(index,1);

	return s;
}

