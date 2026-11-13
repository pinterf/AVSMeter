/*
	This file is part of AVSMeter,
	Copyright(C) Groucho2004.

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


#ifndef _UTILS_H
#define _UTILS_H

#include "stdafx.h"

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
	string         OSErrorMessage();
	void           CursorUp(unsigned int iLines);
	void           SetRGB(WORD wAttributes, BOOL bSet);
	void           ResetRGB(BOOL bSet);
	string         GetFileVersion(string s_file);

	//string functions
	void           StrTrimLeft(string &s_string);
	void           StrTrimRight(string &s_string);
	void           StrTrim(string &s_string);
	void           StrToUC(string &s_string);
	void           StrToLC(string &s_string);
	string         StrFormat(const char *fmt, ...);
	string         StrPad(string s_line);
	string         StrFormatFPS(double d_fps);
	BOOL           IsNumeric(string s_string);

private:
	HANDLE         hConsole;
	WORD           wSaveAttributes;
	int            iConsoleWidth;
};

CUtils::CUtils()
{
	hConsole = ::GetStdHandle(STD_OUTPUT_HANDLE);

	//save attributes
	CONSOLE_SCREEN_BUFFER_INFO csbi; 
	GetConsoleScreenBufferInfo(hConsole, &csbi); 
	wSaveAttributes = csbi.wAttributes;
	iConsoleWidth = (int)csbi.dwSize.X;
}

CUtils::~CUtils()
{
	//retrieve saved attributes
	SetConsoleTextAttribute(hConsole, wSaveAttributes);
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

	unsigned int bufsize = 16384;
	unsigned int length = 256;
	char *buffer = NULL;
	int result = -1;

	va_list args;
	va_start(args, fmt);
	while (result == -1)
	{
		if (buffer)	delete [] buffer;
		buffer = new char[length];
		memset(buffer, ' ', sizeof(buffer));
		result = _vsnprintf(buffer, length, fmt, args);
		length *= 2;
		if (length >= bufsize)
		{
			delete [] buffer;
			va_end(args);
			return "";
		}
	}
	va_end(args);

	string str(buffer);
	delete [] buffer;

	return str;
}


string CUtils::StrPad(string s_line)
{
	int iPadLen = iConsoleWidth - (int)s_line.length();

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


string CUtils::GetFileVersion(string s_file)
{
	string sFileVersion = "0.0.0.0";
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
		return sFileVersion;

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

	sFileVersion = StrFormat("%u.%u.%u.%u", HIWORD(pFixedInfo->dwFileVersionMS), LOWORD(pFixedInfo->dwFileVersionMS), HIWORD(pFixedInfo->dwFileVersionLS), LOWORD(pFixedInfo->dwFileVersionLS));
	delete [] VersionInfoBuffer;

	return sFileVersion;
}


string CUtils::OSErrorMessage()
{
	DWORD dwLastError = GetLastError();
	char *szMsg;

	if (dwLastError == ERROR_SUCCESS)
		return "";
		
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
								NULL,
								dwLastError,
								MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
								(LPTSTR)&szMsg,
								0,
								NULL);

	string sMsg(szMsg);

	return sMsg;
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


#endif //_UTILS_H

