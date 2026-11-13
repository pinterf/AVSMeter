#ifndef _UTILS_H
#define _UTILS_H

#include <windows.h>
#include <wintrust.h>
#include <imagehlp.h>
#include <string>
#include <algorithm>

#define AVSDLL_INCOMPATIBLE 0x01
#define CANNOT_LOAD_AVSDLL 0x02
#define NO_AVS_LINKAGE 0x03

using namespace std;

class CUtils
{
public:
	CUtils();
	virtual ~CUtils();
	double GetTimer();
	BOOL TestTimer();
	BOOL FileExists(string cs_file);
	BYTE CheckAvisynth();
	void CursorUp(int iLines);
	unsigned int GetConsoleWidth();
	BOOL GetProductVersion(string &s_version);
	HANDLE hConsole;
};

CUtils::CUtils()
{
	hConsole = ::GetStdHandle(STD_OUTPUT_HANDLE);
}

CUtils::~CUtils()
{
}


void CUtils::CursorUp(int iLines)
{
	COORD pos;
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	::GetConsoleScreenBufferInfo(hConsole, &csbi);
	pos.Y = (short)(csbi.dwCursorPosition.Y - iLines);
	pos.X = 0;
	::SetConsoleCursorPosition(hConsole, pos);

	return;
}


unsigned int CUtils::GetConsoleWidth()
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	::GetConsoleScreenBufferInfo(hConsole, &csbi); 

	return (unsigned int)csbi.dwSize.X;
}


double CUtils::GetTimer()
{
	LARGE_INTEGER liPerfCounter = {0,0};
	LARGE_INTEGER liPerfFreq = {0,0};

	DWORD_PTR dwpOldMask = ::SetThreadAffinityMask(::GetCurrentThread(), 0x01);
	Sleep(0);

	::QueryPerformanceFrequency(&liPerfFreq);
	::QueryPerformanceCounter(&liPerfCounter);

	::SetThreadAffinityMask(::GetCurrentThread(), dwpOldMask);
	Sleep(0);

	return (double)liPerfCounter.QuadPart / (double)liPerfFreq.QuadPart;
}


BOOL CUtils::TestTimer()
{
	LARGE_INTEGER liPerfCounter = {0,0};
	LARGE_INTEGER liPerfFreq = {0,0};

	BOOL bRet = TRUE;

	DWORD_PTR dwpOldMask = ::SetThreadAffinityMask(::GetCurrentThread(), 0x01);
	Sleep(0);

	if ((::QueryPerformanceFrequency(&liPerfFreq) == 0) || (::QueryPerformanceCounter(&liPerfCounter) == 0))
		bRet = FALSE;

	::SetThreadAffinityMask(::GetCurrentThread(), dwpOldMask);
	Sleep(0);

	return bRet;
}


BOOL CUtils::FileExists(string cs_file)
{
	DWORD dwAttrib;
	dwAttrib = ::GetFileAttributes(cs_file.c_str());
	if (dwAttrib == 0xFFFFFFFF)
		return FALSE;

	if (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)
		return FALSE;	

	return TRUE;
}


BYTE CUtils::CheckAvisynth()
{
	BYTE bRet = 0;

	try
	{
		LOADED_IMAGE li;
		BOOL bLoaded = MapAndLoad("avisynth", NULL, &li,	TRUE,	TRUE);
		if (!bLoaded)
			return CANNOT_LOAD_AVSDLL;

		DWORD expVA = li.FileHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
		PIMAGE_EXPORT_DIRECTORY pExp = (PIMAGE_EXPORT_DIRECTORY)ImageRvaToVa(li.FileHeader, li.MappedAddress, expVA, NULL);

		DWORD rvaNames = pExp->AddressOfNames;
		DWORD *prvaNames = (DWORD*)ImageRvaToVa(li.FileHeader, li.MappedAddress, rvaNames, NULL);
		string sName = "";
		int iPos = 0;

		BYTE bTestLinkage = NO_AVS_LINKAGE;
		for (DWORD i = 0; i < pExp->NumberOfNames; ++i)
		{
			DWORD rvaName = prvaNames[i];
			sName = (char *)ImageRvaToVa(li.FileHeader, li.MappedAddress, rvaName, NULL);
			iPos = (int)sName.find("AVS_Linkage") + (int)sName.find("AVS_linkage");
			if (iPos >= 0)
			{
				bTestLinkage = 0;
				break;
			}
		}

		bRet = bTestLinkage;

		UnMapAndLoad(&li);
	}
	catch (...)
	{
		return AVSDLL_INCOMPATIBLE;
	}

	return bRet;
}


BOOL CUtils::GetProductVersion(string &s_version)
{
	BOOL bRet = FALSE;
	char szFilename[MAX_PATH + 1] = {0};

	if (GetModuleFileName(NULL, szFilename, MAX_PATH) == 0)
		return bRet;

	DWORD dummy;
	DWORD dwSize = GetFileVersionInfoSize(szFilename, &dummy);
	if (dwSize == 0)
		return bRet;

	void* pVersion = malloc(dwSize);
	if (pVersion == NULL)
		return bRet;

	if (!GetFileVersionInfo(szFilename, NULL, dwSize, pVersion))
	{
		free(pVersion);
		return bRet;
	}

	LPVOID pvProductVersion = NULL;
	unsigned int iProductVersionLen = 0;

	if (!VerQueryValue(pVersion, "\\StringFileInfo\\080904b0\\ProductVersion", &pvProductVersion, &iProductVersionLen))
		return bRet;

	s_version = (char*)pvProductVersion;
	bRet = TRUE;

	return bRet;
}


#endif //_UTILS_H

