#ifndef _UTILITY_H
#define _UTILITY_H

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

#define FG_BLACK    0
#define FG_RED			FOREGROUND_RED
#define FG_GREEN		FOREGROUND_GREEN
#define FG_BLUE			FOREGROUND_BLUE
#define FG_YELLOW		FOREGROUND_RED | FOREGROUND_GREEN
#define FG_MAGENTA	FOREGROUND_RED | FOREGROUND_BLUE
#define FG_CYAN			FOREGROUND_GREEN | FOREGROUND_BLUE
#define FG_HRED			FOREGROUND_RED | FOREGROUND_INTENSITY
#define FG_HGREEN		FOREGROUND_GREEN | FOREGROUND_INTENSITY
#define FG_HBLUE		FOREGROUND_BLUE | FOREGROUND_INTENSITY
#define FG_HYELLOW	FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY
#define FG_HMAGENTA	FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY
#define FG_HCYAN		FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY
#define FG_DARKGREY	FOREGROUND_INTENSITY
#define FG_GREY			FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED
#define FG_WHITE		FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY

#define BG_BLACK    0
#define BG_RED			BACKGROUND_RED
#define BG_GREEN		BACKGROUND_GREEN
#define BG_BLUE			BACKGROUND_BLUE
#define BG_YELLOW		BACKGROUND_RED | BACKGROUND_GREEN
#define BG_MAGENTA	BACKGROUND_RED | BACKGROUND_BLUE
#define BG_CYAN			BACKGROUND_GREEN | BACKGROUND_BLUE
#define BG_HRED			BACKGROUND_RED | BACKGROUND_INTENSITY
#define BG_HGREEN		BACKGROUND_GREEN | BACKGROUND_INTENSITY
#define BG_HBLUE		BACKGROUND_BLUE | BACKGROUND_INTENSITY
#define BG_HYELLOW	BACKGROUND_GREEN | BACKGROUND_RED | BACKGROUND_INTENSITY
#define BG_HMAGENTA	BACKGROUND_RED | BACKGROUND_BLUE | BACKGROUND_INTENSITY
#define BG_HCYAN		BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY
#define BG_DARKGREY	BACKGROUND_INTENSITY
#define BG_GREY			BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_RED
#define BG_WHITE		BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_RED | BACKGROUND_INTENSITY


class CUtils
{
public:
	CUtils();
	virtual        ~CUtils();
	BOOL           FileExists(string s_file);
	BOOL           DirectoryExists(string s_dir);
	void           CursorUp(unsigned int iLines);
	int            GetConsoleWidth();
	void           SetRGB(WORD wAttributes, BOOL bSet);
	void           ResetRGB(BOOL bSet);
	string         GetFileVersion(string s_file);
	string         GetFormattedSystemTime();
	string         GetOSVersion();

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

	//string functions
	inline string  StrFormat(char const *fmt, ...);
	void           StrTrimLeft(string &s_string);
	void           StrTrimRight(string &s_string);
	void           StrTrim(string &s_string);
	void           StrToUC(string &s_string);
	void           StrToLC(string &s_string);
	string         StrPad(string s_line);
	string         StrFormatFPS(double d_fps);
	string         StrFormatFrameTime(double d_frametime);
	string         StrAnsiToOEM(string s_string);
	BOOL           IsNumeric(string s_string);

private:
	HANDLE         hConsole;
	WORD           wSaveAttributes;
};

CUtils::CUtils()
{
	hConsole = ::GetStdHandle(STD_OUTPUT_HANDLE);

	//save attributes
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hConsole, &csbi);
	wSaveAttributes = csbi.wAttributes;
	bCPUIDSuccess = FALSE;
}

CUtils::~CUtils()
{
	//retrieve saved attributes
	SetConsoleTextAttribute(hConsole, wSaveAttributes);
}


int CUtils::GetConsoleWidth()
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hConsole, &csbi);

	return (int)csbi.dwSize.X;
}


void CUtils::CursorUp(unsigned int iLines)
{
	COORD pos;
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	::GetConsoleScreenBufferInfo(hConsole, &csbi);
	pos.Y = (short)(csbi.dwCursorPosition.Y - iLines);
	pos.X = 0;
	::SetConsoleCursorPosition(hConsole, pos);

	return;
}


void CUtils::SetRGB(WORD wAttributes, BOOL bSet)
{
	if (bSet)
		SetConsoleTextAttribute(hConsole, wAttributes);

	return;
}


void CUtils::ResetRGB(BOOL bSet)
{
	if (bSet)
		SetConsoleTextAttribute(hConsole, wSaveAttributes);

	return;
}


BOOL CUtils::FileExists(string s_file)
{
	DWORD dwAttrib;
	dwAttrib = ::GetFileAttributes(s_file.c_str());
	if (dwAttrib == 0xFFFFFFFF)
		return FALSE;

	if (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)
		return FALSE;

	return TRUE;
}


BOOL CUtils::DirectoryExists(string s_dir)
{
	DWORD dwAttrib;
	dwAttrib = ::GetFileAttributes(s_dir.c_str());
	if (dwAttrib == 0xFFFFFFFF)
		return FALSE;

	if (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)
		return TRUE;

	return FALSE;
}


string CUtils::GetFormattedSystemTime()
{
	SYSTEMTIME st;
	GetLocalTime(&st);

	return StrFormat("%04d%02d%02d-%02d%02d%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}


inline string CUtils::StrFormat(char const *fmt, ...)
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
			return "\nPrint buffer overflow";
	}
	buffer[buffer.size() - 1] = '\0';
	string sRet = &buffer[0];
	va_end(args);

	return sRet;
}


void CUtils::StrTrimLeft(string &s_string)
{
	s_string.erase(0, s_string.find_first_not_of(" \t\n\r"));

	return;
}


void CUtils::StrTrimRight(string &s_string)
{
	s_string.erase(s_string.find_last_not_of(" \t\n\r") + 1);

	return;
}


void CUtils::StrTrim(string &s_string)
{
	s_string.erase(s_string.find_last_not_of(" \t\n\r") + 1);
	s_string.erase(0, s_string.find_first_not_of(" \t\n\r"));

	return;
}


void CUtils::StrToUC(string &s_string)
{
	for (unsigned int i = 0; i < s_string.length(); i++)
		s_string[i] = toupper(s_string[i]);

	return;
}


void CUtils::StrToLC(string &s_string)
{
	for (unsigned int i = 0; i < s_string.length(); i++)
		s_string[i] = tolower(s_string[i]);

	return;
}


string CUtils::StrPad(string s_line)
{
	int iPadLen = GetConsoleWidth() - (int)s_line.length();

	if ((iPadLen <= 0) || (iPadLen > 1000))
		return s_line;

	s_line.append(iPadLen - 1, ' ');

	return s_line;
}


string CUtils::StrFormatFPS(double d_fps)
{
	string sRet = "";

	if (d_fps < 9.999)
		sRet = StrFormat("%.3f", d_fps);
	else if (d_fps < 99.99)
		sRet = StrFormat("%.2f", d_fps);
	else if (d_fps < 999.9)
		sRet = StrFormat("%.1f", d_fps);
	else
		sRet = StrFormat("%u", (unsigned int)(d_fps + 0.5));

	return sRet;
}


string CUtils::StrFormatFrameTime(double d_frametime)
{
	string sRet = "";

	if (d_frametime < 1.0)
		sRet = StrFormat("%.4f", d_frametime);
	else if (d_frametime < 10.0)
		sRet = StrFormat("%.3f", d_frametime);
	else if (d_frametime < 100.0)
		sRet = StrFormat("%.2f", d_frametime);
	else if (d_frametime < 1000.0)
		sRet = StrFormat("%.1f", d_frametime);
	else
		sRet = StrFormat("%u", (unsigned int)(d_frametime + 0.5));

	return sRet;
}


string CUtils::GetFileVersion(string s_file)
{
	string sFileVersion = "n/a";
	string sProductVersion = "n/a";
	char szFilename[MAX_PATH + 1] = {0};

	if (s_file == "")
	{
		if (GetModuleFileName(NULL, szFilename, MAX_PATH) == 0)
			return sFileVersion;
	}
	else
		sprintf(szFilename, "%s", s_file.c_str());

	DWORD dummy;
	DWORD dwSize = GetFileVersionInfoSize(szFilename, &dummy);
	if (dwSize == 0)
		return "n/a";

	BYTE* VersionInfoBuffer = new BYTE[dwSize];
	if (!GetFileVersionInfo(szFilename, 0, dwSize, (VOID*)VersionInfoBuffer))
	{
		delete [] VersionInfoBuffer;
		return sFileVersion;
	}

	UINT uiLen;
	VS_FIXEDFILEINFO* pFixedInfo = 0;
	if (!VerQueryValue(VersionInfoBuffer, "\\", (LPVOID*)(&pFixedInfo), &uiLen))
	{
		delete [] VersionInfoBuffer;
		return sFileVersion;
	}

	sFileVersion =    StrFormat("%u.%u.%u.%u", HIWORD(pFixedInfo->dwFileVersionMS), LOWORD(pFixedInfo->dwFileVersionMS), HIWORD(pFixedInfo->dwFileVersionLS), LOWORD(pFixedInfo->dwFileVersionLS));
	sProductVersion = StrFormat("%u.%u.%u.%u", HIWORD(pFixedInfo->dwProductVersionMS), LOWORD(pFixedInfo->dwProductVersionMS), HIWORD(pFixedInfo->dwProductVersionLS), LOWORD(pFixedInfo->dwProductVersionLS));
	delete [] VersionInfoBuffer;

	if (sFileVersion != "0.0.0.0")
		return sFileVersion;

	if (sProductVersion != "0.0.0.0")
		return sProductVersion;

	return "n/a";
}


BOOL CUtils::IsNumeric(string s_string)
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
			case '-':
				continue;
			default:
				return FALSE;
		}
	}

	return TRUE;
}


string CUtils::StrAnsiToOEM(string s_string)
{
	char *buffer = NULL;
	buffer = new char[s_string.length() + 1];
	memset(buffer, ' ', sizeof(buffer));
	CharToOem(s_string.c_str(), buffer);
	string sOEM(buffer);

	return sOEM;
}


string CUtils::GetOSVersion()
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


void CUtils::GetCPUID()
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
		cpudata.NumCPUs = (BYTE)data.total_logical_cpus / (BYTE)data.num_logical_cpus;
	else
		cpudata.NumCPUs = 1;

	cpudata.CPUCores = (BYTE)data.num_cores * cpudata.NumCPUs;
	cpudata.CPULogicalCores = (BYTE)data.num_logical_cpus * cpudata.NumCPUs;
	cpudata.CPUCodeName = StrFormat("%s", data.cpu_codename);
	cpudata.CPUBrandString = StrFormat("%s", data.brand_str);

	int clk = cpu_clock_by_ic(100, 15);
	if (clk > 100)
		cpudata.CPURealClock = (unsigned int)clk;
	else
		cpudata.CPURealClock = 0;

	bCPUIDSuccess = TRUE;

	return;
}


#endif //_UTILITY_H
