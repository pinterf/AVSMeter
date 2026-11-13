// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#ifndef _UTILS_H
#define _UTILS_H

#include "stdafx.h"
#include "VerInfo.h"

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


using namespace std;

class CUtils
{
public:
	CUtils();
	virtual      ~CUtils();
	BOOL         IsUserAdmin();
	BOOL         FileExists(string s_file);
	string       OSErrorMessage();
	void         CursorUp(int iLines);
	unsigned int GetConsoleWidth();
	void SetRGB(WORD wAttributes, BOOL bSet);
	void ResetRGB(BOOL bSet);
	BOOL GetProductVersion(string &s_version);
	BOOL GetResourceInfo(string s_file);

	struct ResourceInfo
	{
		string sLanguage;
		string sFixedFileVersion;
		string sFixedProductVersion;
		string sComments;         // VI_STR_COMMENTS
		string sCompanyName;      // VI_STR_COMPANYNAME
		string sFileDescription;  // VI_STR_FILEDESCRIPTION
		string sFileVersion;      // VI_STR_FILEVERSION
		string sInternalName;     // VI_STR_INTERNALNAME
		string sLegalCopyright;   // VI_STR_LEGALCOPYRIGHT
		string sLegalTrademarks;  // VI_STR_LEGALTRADEMARKS
		string sOriginalFilename; // VI_STR_ORIGINALFILENAME
		string sPrivateBuild;     // VI_STR_PRIVATEBUILD
		string sProductName;      // VI_STR_PRODUCTNAME
		string sProductVersion;   // VI_STR_PRODUCTVERSION
		string sSpecialBuild;     // VI_STR_SPECIALBUILD
	} resinfo;

	//string functions
	void   StrTrimLeft(string &s_string);
	void   StrTrimRight(string &s_string);
	void   StrTrim(string &s_string);
	void   StrToUC(string &s_string);
	void   StrToLC(string &s_string);
	string StrFormat(const char *fmt, ...);
	string StrPad(string s_line, unsigned int ui_consolewidth);
	string StrFormatFPS(double d_fps);
	BOOL   IsNumeric(string s_string);

private:
	HANDLE hConsole;
	WORD   wSaveAttributes;
};

CUtils::CUtils()
{
	hConsole = ::GetStdHandle(STD_OUTPUT_HANDLE);

	//save attributes
	CONSOLE_SCREEN_BUFFER_INFO csbi; 
	GetConsoleScreenBufferInfo(hConsole, &csbi); 
	wSaveAttributes = csbi.wAttributes;
}

CUtils::~CUtils()
{
	//retrieve saved attributes
	SetConsoleTextAttribute(hConsole, wSaveAttributes);
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


BOOL CUtils::GetResourceInfo(string s_file)
{
	resinfo.sLanguage = "";
	resinfo.sFixedFileVersion = "";
	resinfo.sFixedProductVersion = "";
	resinfo.sComments = "";
	resinfo.sCompanyName = "";
	resinfo.sFileDescription = "";
	resinfo.sFileVersion = "";
	resinfo.sInternalName = "";
	resinfo.sLegalCopyright = "";
	resinfo.sLegalTrademarks = "";
	resinfo.sOriginalFilename = "";
	resinfo.sPrivateBuild = "";
	resinfo.sProductName = "";
	resinfo.sProductVersion = "";
	resinfo.sSpecialBuild = "";

	CFileVersionInfo fvi;

	if (fvi.Open(s_file.c_str()))
	{
		if (fvi.GetTransCount() > 0)
		{
			char szLID[56] = {0};
			fvi.GetLIDName(fvi.GetLIDByIndex(0), szLID, 56);
			resinfo.sLanguage = StrFormat("%s", szLID);
		}
		else
		{
			fvi.Close();
			return FALSE;
		}

		char szVer[1024] = {0};
		resinfo.sFixedFileVersion = StrFormat("%d.%d.%d.%d", fvi.GetFileVersionMajor(), fvi.GetFileVersionMinor(), fvi.GetFileVersionBuild(), fvi.GetFileVersionQFE());
		resinfo.sFixedProductVersion = StrFormat("%d.%d.%d.%d", fvi.GetProductVersionMajor(), fvi.GetProductVersionMinor(), fvi.GetProductVersionBuild(), fvi.GetProductVersionQFE());

		if (fvi.QueryStringValue(VI_STR_COMMENTS, szVer, 1024))
			resinfo.sComments = StrFormat("%s", szVer);
		if (fvi.QueryStringValue(VI_STR_COMPANYNAME, szVer, 1024))
			resinfo.sCompanyName = StrFormat("%s", szVer);
		if (fvi.QueryStringValue(VI_STR_FILEDESCRIPTION, szVer, 1024))
			resinfo.sFileDescription = StrFormat("%s", szVer);
		if (fvi.QueryStringValue(VI_STR_FILEVERSION, szVer, 1024))
			resinfo.sFileVersion = StrFormat("%s", szVer);
		if (fvi.QueryStringValue(VI_STR_INTERNALNAME, szVer, 1024))
			resinfo.sInternalName = StrFormat("%s", szVer);
		if (fvi.QueryStringValue(VI_STR_LEGALCOPYRIGHT, szVer, 1024))
			resinfo.sLegalCopyright = StrFormat("%s", szVer);
		if (fvi.QueryStringValue(VI_STR_LEGALTRADEMARKS, szVer, 1024))
			resinfo.sLegalTrademarks = StrFormat("%s", szVer);
		if (fvi.QueryStringValue(VI_STR_ORIGINALFILENAME, szVer, 1024))
			resinfo.sOriginalFilename = StrFormat("%s", szVer);
		if (fvi.QueryStringValue(VI_STR_PRIVATEBUILD, szVer, 1024))
			resinfo.sPrivateBuild = StrFormat("%s", szVer);
		if (fvi.QueryStringValue(VI_STR_PRODUCTNAME, szVer, 1024))
			resinfo.sProductName = StrFormat("%s", szVer);
		if (fvi.QueryStringValue(VI_STR_PRODUCTVERSION, szVer, 1024))
			resinfo.sProductVersion = StrFormat("%s", szVer);
		if (fvi.QueryStringValue(VI_STR_SPECIALBUILD, szVer, 1024))
			resinfo.sSpecialBuild = StrFormat("%s", szVer);

		fvi.Close();

		return TRUE;
	}

	return FALSE; //no version resource
}


string CUtils::OSErrorMessage()
{
	LPVOID lpf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),	(LPTSTR)&lpf, 0,	NULL);
	
	return (LPCTSTR)lpf;
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


BOOL CUtils::IsUserAdmin()
{
	BOOL bAdmin;
	SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
	PSID AdministratorsGroup; 
	bAdmin = AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &AdministratorsGroup); 
	if (bAdmin) 
	{
		if (!CheckTokenMembership(NULL, AdministratorsGroup, &bAdmin))
			bAdmin = FALSE;

    FreeSid(AdministratorsGroup); 
	}

	return(bAdmin);
}


#endif //_UTILS_H

