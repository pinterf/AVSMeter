/*
	This file is part of AVSMeter by Groucho2004.

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
#include "utility.h"

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
		string       CPUSupportedInstructionSets1;
		string       CPUSupportedInstructionSets2;
	} cpudata;

private:
	CUtils         utils;
	string         RemoveMultipleWhiteSpaces(string s);
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
			sOSVersion = utils.StrFormat("Windows XP %s", sArchitecture.c_str());
		else
			sOSVersion = utils.StrFormat("Windows Server 2003 %s", sArchitecture.c_str());
	}

	if (osvi.dwMajorVersion == 6)
	{
		if (osvi.dwMinorVersion == 0)
		{
			if (osvi.wProductType == VER_NT_WORKSTATION)
				sOSVersion = utils.StrFormat("Windows Vista %s", sArchitecture.c_str());
			else
				sOSVersion = utils.StrFormat("Windows Server 2008 %s", sArchitecture.c_str());
		}
		if (osvi.dwMinorVersion == 1)
		{
			if (osvi.wProductType == VER_NT_WORKSTATION)
				sOSVersion = utils.StrFormat("Windows 7 %s", sArchitecture.c_str());
			else
				sOSVersion = utils.StrFormat("Windows Server 2008 R2 %s", sArchitecture.c_str());
		}
		if (osvi.dwMinorVersion == 2)
		{
			if (osvi.wProductType == VER_NT_WORKSTATION)
				sOSVersion = utils.StrFormat("Windows 8 %s", sArchitecture.c_str());
			else
				sOSVersion = utils.StrFormat("Windows Server 2012 %s", sArchitecture.c_str());
		}
		if (osvi.dwMinorVersion == 3)
		{
			if (osvi.wProductType == VER_NT_WORKSTATION)
				sOSVersion = utils.StrFormat("Windows 8.1 %s", sArchitecture.c_str());
			else
				sOSVersion = utils.StrFormat("Windows Server 2012 R2 %s", sArchitecture.c_str());
		}
	}

	if (osvi.dwMajorVersion == 10)
	{
		if (osvi.dwMinorVersion == 0)
			sOSVersion = utils.StrFormat("Windows 10 %s", sArchitecture.c_str());
	}

	if ((osvi.wServicePackMajor > 0) || (osvi.wServicePackMinor > 0))
		sOSAddendum = utils.StrFormat(" Service Pack %u.%u (Build %u)", osvi.wServicePackMajor, osvi.wServicePackMinor, osvi.dwBuildNumber);
	else
		sOSAddendum = utils.StrFormat(" (Build %u)", osvi.dwBuildNumber);

	return sOSVersion + sOSAddendum;
}


string CSysInfo::GetFormattedSystemDateTime()
{
	SYSTEMTIME st;
	GetLocalTime(&st);

	return utils.StrFormat("%04d%02d%02d-%02d%02d%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
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
	cpudata.CPUCodeName = utils.StrFormat("%s", data.cpu_codename);
	cpudata.CPUCodeName = RemoveMultipleWhiteSpaces(cpudata.CPUCodeName);
	cpudata.CPUBrandString = utils.StrFormat("%s", data.brand_str);
	cpudata.CPUBrandString = RemoveMultipleWhiteSpaces(cpudata.CPUBrandString);

	cpudata.CPUSupportedInstructionSets1 = "";
	cpudata.CPUSupportedInstructionSets2 = "";

	if (data.flags[CPU_FEATURE_3DNOW])     cpudata.CPUSupportedInstructionSets1 += "3DNOW, ";
	if (data.flags[CPU_FEATURE_3DNOWEXT])  cpudata.CPUSupportedInstructionSets1 += "3DNOWEXT, ";
	if (data.flags[CPU_FEATURE_MMX])       cpudata.CPUSupportedInstructionSets1 += "MMX, ";
	if (data.flags[CPU_FEATURE_MMXEXT])    cpudata.CPUSupportedInstructionSets1 += "MMXEXT, ";
	if (data.flags[CPU_FEATURE_SSE])       cpudata.CPUSupportedInstructionSets1 += "SSE, ";
	if (data.flags[CPU_FEATURE_SSE2])      cpudata.CPUSupportedInstructionSets1 += "SSE2, ";
	if (data.flags[CPU_FEATURE_PNI])       cpudata.CPUSupportedInstructionSets1 += "SSE3, ";
	if (data.flags[CPU_FEATURE_SSSE3])     cpudata.CPUSupportedInstructionSets1 += "SSSE3, ";
	if (data.flags[CPU_FEATURE_SSE4_1])    cpudata.CPUSupportedInstructionSets1 += "SSE4.1, ";
	if (data.flags[CPU_FEATURE_SSE4_2])    cpudata.CPUSupportedInstructionSets1 += "SSE4.2, ";
	if (data.flags[CPU_FEATURE_SSE4A])     cpudata.CPUSupportedInstructionSets1 += "SSE4A, ";
	if (data.flags[CPU_FEATURE_FMA3])      cpudata.CPUSupportedInstructionSets1 += "FMA3, ";
	if (data.flags[CPU_FEATURE_FMA4])      cpudata.CPUSupportedInstructionSets1 += "FMA4, ";
	if (data.flags[CPU_FEATURE_AVX])       cpudata.CPUSupportedInstructionSets1 += "AVX, ";
	if (data.flags[CPU_FEATURE_AVX2])      cpudata.CPUSupportedInstructionSets1 += "AVX2, ";
	if (data.flags[CPU_FEATURE_AVX512F])   cpudata.CPUSupportedInstructionSets1 += "AVX512F, ";
	if (data.flags[CPU_FEATURE_AVX512DQ])  cpudata.CPUSupportedInstructionSets1 += "AVX512DQ, ";
	if (data.flags[CPU_FEATURE_AVX512PF])  cpudata.CPUSupportedInstructionSets1 += "AVX512PF, ";
	if (data.flags[CPU_FEATURE_AVX512ER])  cpudata.CPUSupportedInstructionSets1 += "AVX512ER, ";
	if (data.flags[CPU_FEATURE_AVX512CD])  cpudata.CPUSupportedInstructionSets1 += "AVX512CD, ";
	if (data.flags[CPU_FEATURE_AVX512BW])  cpudata.CPUSupportedInstructionSets1 += "AVX512BW, ";
	if (data.flags[CPU_FEATURE_AVX512VL])  cpudata.CPUSupportedInstructionSets1 += "AVX512VL, ";


	if (data.flags[CPU_FEATURE_FPU])            cpudata.CPUSupportedInstructionSets2 += "FPU, ";
	if (data.flags[CPU_FEATURE_VME])            cpudata.CPUSupportedInstructionSets2 += "VME, ";
	if (data.flags[CPU_FEATURE_DE])             cpudata.CPUSupportedInstructionSets2 += "DE, ";
	if (data.flags[CPU_FEATURE_PSE])            cpudata.CPUSupportedInstructionSets2 += "PSE, ";
	if (data.flags[CPU_FEATURE_TSC])            cpudata.CPUSupportedInstructionSets2 += "TSC, ";
	if (data.flags[CPU_FEATURE_MSR])            cpudata.CPUSupportedInstructionSets2 += "MSR, ";
	if (data.flags[CPU_FEATURE_PAE])            cpudata.CPUSupportedInstructionSets2 += "PAE, ";
	if (data.flags[CPU_FEATURE_MCE])            cpudata.CPUSupportedInstructionSets2 += "MCE, ";
	if (data.flags[CPU_FEATURE_CX8])            cpudata.CPUSupportedInstructionSets2 += "CX8, ";
	if (data.flags[CPU_FEATURE_APIC])           cpudata.CPUSupportedInstructionSets2 += "APIC, ";
	if (data.flags[CPU_FEATURE_MTRR])           cpudata.CPUSupportedInstructionSets2 += "MTRR, ";
	if (data.flags[CPU_FEATURE_SEP])            cpudata.CPUSupportedInstructionSets2 += "SEP, ";
	if (data.flags[CPU_FEATURE_PGE])            cpudata.CPUSupportedInstructionSets2 += "PGE, ";
	if (data.flags[CPU_FEATURE_MCA])            cpudata.CPUSupportedInstructionSets2 += "MCA, ";
	if (data.flags[CPU_FEATURE_CMOV])           cpudata.CPUSupportedInstructionSets2 += "CMOV, ";
	if (data.flags[CPU_FEATURE_PAT])            cpudata.CPUSupportedInstructionSets2 += "PAT, ";
	if (data.flags[CPU_FEATURE_PSE36])          cpudata.CPUSupportedInstructionSets2 += "PSE36, ";
	if (data.flags[CPU_FEATURE_PN])             cpudata.CPUSupportedInstructionSets2 += "PN, ";
	if (data.flags[CPU_FEATURE_CLFLUSH])        cpudata.CPUSupportedInstructionSets2 += "CLFLUSH, ";
	if (data.flags[CPU_FEATURE_DTS])            cpudata.CPUSupportedInstructionSets2 += "DTS, ";
	if (data.flags[CPU_FEATURE_ACPI])           cpudata.CPUSupportedInstructionSets2 += "ACPI, ";
	if (data.flags[CPU_FEATURE_MMX])            cpudata.CPUSupportedInstructionSets2 += "MMX, ";
	if (data.flags[CPU_FEATURE_FXSR])           cpudata.CPUSupportedInstructionSets2 += "FXSR, ";
	if (data.flags[CPU_FEATURE_SSE])            cpudata.CPUSupportedInstructionSets2 += "SSE, ";
	if (data.flags[CPU_FEATURE_SSE2])           cpudata.CPUSupportedInstructionSets2 += "SSE2, ";
	if (data.flags[CPU_FEATURE_SS])             cpudata.CPUSupportedInstructionSets2 += "SS, ";
	if (data.flags[CPU_FEATURE_HT])             cpudata.CPUSupportedInstructionSets2 += "HT, ";
	if (data.flags[CPU_FEATURE_TM])             cpudata.CPUSupportedInstructionSets2 += "TM, ";
	if (data.flags[CPU_FEATURE_IA64])           cpudata.CPUSupportedInstructionSets2 += "IA64, ";
	if (data.flags[CPU_FEATURE_PBE])            cpudata.CPUSupportedInstructionSets2 += "PBE, ";
	if (data.flags[CPU_FEATURE_PNI])            cpudata.CPUSupportedInstructionSets2 += "PNI, ";
	if (data.flags[CPU_FEATURE_PCLMUL])         cpudata.CPUSupportedInstructionSets2 += "PCLMUL, ";
	if (data.flags[CPU_FEATURE_DTS64])          cpudata.CPUSupportedInstructionSets2 += "DTS64, ";
	if (data.flags[CPU_FEATURE_MONITOR])        cpudata.CPUSupportedInstructionSets2 += "MONITOR, ";
	if (data.flags[CPU_FEATURE_DS_CPL])         cpudata.CPUSupportedInstructionSets2 += "DS_CPL, ";
	if (data.flags[CPU_FEATURE_VMX])            cpudata.CPUSupportedInstructionSets2 += "VMX, ";
	if (data.flags[CPU_FEATURE_SMX])            cpudata.CPUSupportedInstructionSets2 += "SMX, ";
	if (data.flags[CPU_FEATURE_EST])            cpudata.CPUSupportedInstructionSets2 += "EST, ";
	if (data.flags[CPU_FEATURE_TM2])            cpudata.CPUSupportedInstructionSets2 += "TM2, ";
	if (data.flags[CPU_FEATURE_SSSE3])          cpudata.CPUSupportedInstructionSets2 += "SSSE3, ";
	if (data.flags[CPU_FEATURE_CID])            cpudata.CPUSupportedInstructionSets2 += "CID, ";
	if (data.flags[CPU_FEATURE_CX16])           cpudata.CPUSupportedInstructionSets2 += "CX16, ";
	if (data.flags[CPU_FEATURE_XTPR])           cpudata.CPUSupportedInstructionSets2 += "XTPR, ";
	if (data.flags[CPU_FEATURE_PDCM])           cpudata.CPUSupportedInstructionSets2 += "PDCM, ";
	if (data.flags[CPU_FEATURE_DCA])            cpudata.CPUSupportedInstructionSets2 += "DCA, ";
	if (data.flags[CPU_FEATURE_SSE4_1])         cpudata.CPUSupportedInstructionSets2 += "SSE4.1, ";
	if (data.flags[CPU_FEATURE_SSE4_2])         cpudata.CPUSupportedInstructionSets2 += "SSE4.2, ";
	if (data.flags[CPU_FEATURE_SYSCALL])        cpudata.CPUSupportedInstructionSets2 += "SYSCALL, ";
	if (data.flags[CPU_FEATURE_XD])             cpudata.CPUSupportedInstructionSets2 += "XD, ";
	if (data.flags[CPU_FEATURE_MOVBE])          cpudata.CPUSupportedInstructionSets2 += "MOVBE, ";
	if (data.flags[CPU_FEATURE_POPCNT])         cpudata.CPUSupportedInstructionSets2 += "POPCNT, ";
	if (data.flags[CPU_FEATURE_AES])            cpudata.CPUSupportedInstructionSets2 += "AES, ";
	if (data.flags[CPU_FEATURE_XSAVE])          cpudata.CPUSupportedInstructionSets2 += "XSAVE, ";
	if (data.flags[CPU_FEATURE_OSXSAVE])        cpudata.CPUSupportedInstructionSets2 += "OSXSAVE, ";
	if (data.flags[CPU_FEATURE_AVX])            cpudata.CPUSupportedInstructionSets2 += "AVX, ";
	if (data.flags[CPU_FEATURE_MMXEXT])         cpudata.CPUSupportedInstructionSets2 += "MMXEXT, ";
	if (data.flags[CPU_FEATURE_3DNOW])          cpudata.CPUSupportedInstructionSets2 += "3DNOW, ";
	if (data.flags[CPU_FEATURE_3DNOWEXT])       cpudata.CPUSupportedInstructionSets2 += "3DNOWEXT, ";
	if (data.flags[CPU_FEATURE_NX])             cpudata.CPUSupportedInstructionSets2 += "NX, ";
	if (data.flags[CPU_FEATURE_FXSR_OPT])       cpudata.CPUSupportedInstructionSets2 += "FXSR_OPT, ";
	if (data.flags[CPU_FEATURE_RDTSCP])         cpudata.CPUSupportedInstructionSets2 += "RDTSCP, ";
	if (data.flags[CPU_FEATURE_LM])             cpudata.CPUSupportedInstructionSets2 += "LM, ";
	if (data.flags[CPU_FEATURE_LAHF_LM])        cpudata.CPUSupportedInstructionSets2 += "LAHF_LM, ";
	if (data.flags[CPU_FEATURE_CMP_LEGACY])     cpudata.CPUSupportedInstructionSets2 += "CMP_LEGACY], ";
	if (data.flags[CPU_FEATURE_SVM])            cpudata.CPUSupportedInstructionSets2 += "SVM, ";
	if (data.flags[CPU_FEATURE_ABM])            cpudata.CPUSupportedInstructionSets2 += "ABM, ";
	if (data.flags[CPU_FEATURE_MISALIGNSSE])    cpudata.CPUSupportedInstructionSets2 += "MISALIGNSSE, ";
	if (data.flags[CPU_FEATURE_SSE4A])          cpudata.CPUSupportedInstructionSets2 += "SSE4A, ";
	if (data.flags[CPU_FEATURE_3DNOWPREFETCH])  cpudata.CPUSupportedInstructionSets2 += "3DNOWPREFET, ";
	if (data.flags[CPU_FEATURE_OSVW])           cpudata.CPUSupportedInstructionSets2 += "OSVW, ";
	if (data.flags[CPU_FEATURE_IBS])            cpudata.CPUSupportedInstructionSets2 += "IBS, ";
	if (data.flags[CPU_FEATURE_SSE5])           cpudata.CPUSupportedInstructionSets2 += "SSE5, ";
	if (data.flags[CPU_FEATURE_SKINIT])         cpudata.CPUSupportedInstructionSets2 += "SKINIT, ";
	if (data.flags[CPU_FEATURE_WDT])            cpudata.CPUSupportedInstructionSets2 += "WDT, ";
	if (data.flags[CPU_FEATURE_TS])             cpudata.CPUSupportedInstructionSets2 += "TS, ";
	if (data.flags[CPU_FEATURE_FID])            cpudata.CPUSupportedInstructionSets2 += "FID, ";
	if (data.flags[CPU_FEATURE_VID])            cpudata.CPUSupportedInstructionSets2 += "VID, ";
	if (data.flags[CPU_FEATURE_TTP])            cpudata.CPUSupportedInstructionSets2 += "TTP, ";
	if (data.flags[CPU_FEATURE_TM_AMD])         cpudata.CPUSupportedInstructionSets2 += "TM_AMD, ";
	if (data.flags[CPU_FEATURE_STC])            cpudata.CPUSupportedInstructionSets2 += "STC, ";
	if (data.flags[CPU_FEATURE_100MHZSTEPS])    cpudata.CPUSupportedInstructionSets2 += "100MHZSTEPS, ";
	if (data.flags[CPU_FEATURE_HWPSTATE])       cpudata.CPUSupportedInstructionSets2 += "HWPSTATE, ";
	if (data.flags[CPU_FEATURE_CONSTANT_TSC])   cpudata.CPUSupportedInstructionSets2 += "CONSTANT_TS, ";
	if (data.flags[CPU_FEATURE_XOP])            cpudata.CPUSupportedInstructionSets2 += "XOP, ";
	if (data.flags[CPU_FEATURE_FMA3])           cpudata.CPUSupportedInstructionSets2 += "FMA3, ";
	if (data.flags[CPU_FEATURE_FMA4])           cpudata.CPUSupportedInstructionSets2 += "FMA4, ";
	if (data.flags[CPU_FEATURE_TBM])            cpudata.CPUSupportedInstructionSets2 += "TBM, ";
	if (data.flags[CPU_FEATURE_F16C])           cpudata.CPUSupportedInstructionSets2 += "F16C, ";
	if (data.flags[CPU_FEATURE_RDRAND])         cpudata.CPUSupportedInstructionSets2 += "RDRAND, ";
	if (data.flags[CPU_FEATURE_X2APIC])         cpudata.CPUSupportedInstructionSets2 += "X2APIC, ";
	if (data.flags[CPU_FEATURE_CPB])            cpudata.CPUSupportedInstructionSets2 += "CPB, ";
	if (data.flags[CPU_FEATURE_APERFMPERF])     cpudata.CPUSupportedInstructionSets2 += "APERFMPERF, ";
	if (data.flags[CPU_FEATURE_PFI])            cpudata.CPUSupportedInstructionSets2 += "PFI, ";
	if (data.flags[CPU_FEATURE_PA])             cpudata.CPUSupportedInstructionSets2 += "PA, ";
	if (data.flags[CPU_FEATURE_AVX2])           cpudata.CPUSupportedInstructionSets2 += "AVX2, ";
	if (data.flags[CPU_FEATURE_BMI1])           cpudata.CPUSupportedInstructionSets2 += "BMI1, ";
	if (data.flags[CPU_FEATURE_BMI2])           cpudata.CPUSupportedInstructionSets2 += "BMI2, ";
	if (data.flags[CPU_FEATURE_HLE])            cpudata.CPUSupportedInstructionSets2 += "HLE, ";
	if (data.flags[CPU_FEATURE_RTM])            cpudata.CPUSupportedInstructionSets2 += "RTM, ";
	if (data.flags[CPU_FEATURE_AVX512F])        cpudata.CPUSupportedInstructionSets2 += "AVX512F, ";
	if (data.flags[CPU_FEATURE_AVX512DQ])       cpudata.CPUSupportedInstructionSets2 += "AVX512DQ, ";
	if (data.flags[CPU_FEATURE_AVX512PF])       cpudata.CPUSupportedInstructionSets2 += "AVX512PF, ";
	if (data.flags[CPU_FEATURE_AVX512ER])       cpudata.CPUSupportedInstructionSets2 += "AVX512ER, ";
	if (data.flags[CPU_FEATURE_AVX512CD])       cpudata.CPUSupportedInstructionSets2 += "AVX512CD, ";
	if (data.flags[CPU_FEATURE_SHA_NI])         cpudata.CPUSupportedInstructionSets2 += "SHA_NI, ";
	if (data.flags[CPU_FEATURE_AVX512BW])       cpudata.CPUSupportedInstructionSets2 += "AVX512BW, ";
	if (data.flags[CPU_FEATURE_AVX512VL])       cpudata.CPUSupportedInstructionSets2 += "AVX512VL, ";
	if (data.flags[CPU_FEATURE_SGX])            cpudata.CPUSupportedInstructionSets2 += "SGX, ";
	if (data.flags[CPU_FEATURE_RDSEED])         cpudata.CPUSupportedInstructionSets2 += "RDSEED, ";
	if (data.flags[CPU_FEATURE_ADX])            cpudata.CPUSupportedInstructionSets2 += "ADX, ";
	if (data.flags[CPU_FEATURE_AVX512VNNI])     cpudata.CPUSupportedInstructionSets2 += "AVX512VNNI, ";

	cpudata.CPUSupportedInstructionSets1.erase(cpudata.CPUSupportedInstructionSets1.find_last_not_of(", ") + 1);
	cpudata.CPUSupportedInstructionSets2.erase(cpudata.CPUSupportedInstructionSets2.find_last_not_of(", ") + 1);

	bCPUIDSuccess = TRUE;

	return;
}



#endif //_SYSINFO_H
