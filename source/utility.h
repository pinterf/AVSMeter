// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#ifndef _UTILS_H
#define _UTILS_H

#include <windows.h>
#include <wintrust.h>
#include <imagehlp.h>
#include <string>
#include <algorithm>

#define AVSDLL_SUCCESS 0x00
#define AVSDLL_INCOMPATIBLE 0x01
#define CANNOT_LOAD_AVSDLL 0x02
#define NO_AVS_LINKAGE 0x03

using namespace std;

class CUtils
{
public:
	CUtils();
	virtual      ~CUtils();
	double       GetTimer();
	BOOL         TestTimer();
	BOOL         FileExists(string s_file);
	BYTE         CheckAvisynth();
	void         CursorUp(int iLines);
	unsigned int GetConsoleWidth();
	HANDLE       hConsole;

	//string functions
	void   StrTrimLeft(string &s_string);
	void   StrTrimRight(string &s_string);
	void   StrTrim(string &s_string);
	void   StrToUC(string &s_string);
	void   StrToLC(string &s_string);
	string StrFormat(const char *fmt, ...);
	string StrFormatTime(unsigned int ui_milliseconds);
	string StrPad(string s_line, unsigned int ui_consolewidth);
	string StrFormatFPS(double d_fps);
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


BYTE CUtils::CheckAvisynth()
{
	BYTE bRet = AVSDLL_SUCCESS;

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
		bRet = NO_AVS_LINKAGE;

		for (DWORD dwName = 0; dwName < pExp->NumberOfNames; ++dwName)
		{
			DWORD rvaName = prvaNames[dwName];
			sName = (char *)ImageRvaToVa(li.FileHeader, li.MappedAddress, rvaName, NULL);
			StrToLC(sName);
			iPos = (int)sName.find("avs_linkage");
			if (iPos >= 0)
			{
				bRet = AVSDLL_SUCCESS;
				break;
			}
		}

		UnMapAndLoad(&li);
	}
	catch (...)
	{
		return AVSDLL_INCOMPATIBLE;
	}

	return bRet;
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


string CUtils::StrFormat(const char *fmt, ...)
{
	if (!fmt)	return "";

	#define MAX_LEN 2048
	va_list args;
	va_start(args, fmt);
	unsigned int length = 256;
	char *buffer = NULL;
	int result = -1;

	while (result == -1)
	{
		if (buffer)
			delete [] buffer;

		buffer = new char [length];
		memset(buffer, ' ', sizeof(buffer));
		result = _vsnprintf(buffer, length, fmt, args);
		length *= 2;
		if (length > MAX_LEN)
		{
			delete [] buffer;
			va_end(args);
			return "";
		}
	}

	string str(buffer);
	delete [] buffer;
	va_end(args);

	return str;
}


string CUtils::StrPad(string s_line, unsigned int ui_consolewidth)
{
	int iPadLen = (int)ui_consolewidth - (int)s_line.length();

	if ((iPadLen <= 0) || (iPadLen > 1000))
		return s_line;

	string s_padding((iPadLen - 1), ' ');

	return (s_line + s_padding);
}


string CUtils::StrFormatTime(unsigned int ui_milliseconds)
{
	unsigned int hours = ui_milliseconds / 3600000;
	unsigned int minutes = (ui_milliseconds / 60000) % 60;
	unsigned int seconds = (ui_milliseconds / 1000) % 60;
	unsigned int milliseconds = ui_milliseconds % 1000;

	return StrFormat("%03d:%02d:%02d.%03d", hours, minutes, seconds, milliseconds);
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


#endif //_UTILS_H

