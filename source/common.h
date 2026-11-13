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
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#pragma once

#define BUFSIZE 2048

#include <windows.h>
#include <process.h>
#include <string>
#include <algorithm>
#include <psapi.h>
#include <tlhelp32.h>
#include <wintrust.h>
#include <imagehlp.h>
#include <conio.h>
#include <time.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <set>
#include <map>
#include <eh.h>

using namespace std;

HMODULE GetExceptionModule(LPVOID address, LPSTR moduleName);
BOOL GetFileNameFromHandle(HANDLE hFile, string &sFileName);

struct StrCaseInsensitiveCompare
{
	bool operator() (const string& a, const string& b) const
	{
		return _stricmp(a.c_str(), b.c_str()) < 0;
	}
};

#include "avs_headers\avisynth.h"
const AVS_Linkage *AVS_linkage = 0;

//test STATUS_FLOAT_DIVIDE_BY_ZERO:
//		int a = 0;
//		int b = 100;
//		volatile float c = b/a;

static void SE_Translator(unsigned int u, EXCEPTION_POINTERS* pExcept)
{
  char msg[1024];
	char module[1024];
	string sExceptionName = "Unknown system exception";
	string sTempModulePath = "";
	string sExceptionModule = "Cannot determine module";

	if (GetExceptionModule(pExcept->ExceptionRecord->ExceptionAddress, module))
	{
		sTempModulePath = module;

		HANDLE hFile = CreateFile(sTempModulePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		if (hFile != INVALID_HANDLE_VALUE)
		{
			if (!GetFileNameFromHandle(hFile, sExceptionModule))
				sExceptionModule = sTempModulePath;
		}
		CloseHandle(hFile);
	}

	switch (pExcept->ExceptionRecord->ExceptionCode)
	{
		case STATUS_GUARD_PAGE_VIOLATION:
			sExceptionName = "STATUS_GUARD_PAGE_VIOLATION";
			break;
		case STATUS_DATATYPE_MISALIGNMENT:
			sExceptionName = "STATUS_DATATYPE_MISALIGNMENT";
			break;
		case STATUS_BREAKPOINT:
			sExceptionName = "STATUS_BREAKPOINT";
			break;
		case STATUS_SINGLE_STEP:
			sExceptionName = "STATUS_SINGLE_STEP";
			break;
		case STATUS_ACCESS_VIOLATION:
			sExceptionName = "STATUS_ACCESS_VIOLATION";
			break;
		case STATUS_IN_PAGE_ERROR:
			sExceptionName = "STATUS_IN_PAGE_ERROR";
			break;
		case STATUS_INVALID_HANDLE:
			sExceptionName = "STATUS_INVALID_HANDLE";
			break;
		case STATUS_NO_MEMORY:
			sExceptionName = "STATUS_NO_MEMORY";
			break;
		case STATUS_ILLEGAL_INSTRUCTION:
			sExceptionName = "STATUS_ILLEGAL_INSTRUCTION";
			break;
		case STATUS_NONCONTINUABLE_EXCEPTION:
			sExceptionName = "STATUS_NONCONTINUABLE_EXCEPTION";
			break;
		case STATUS_INVALID_DISPOSITION:
			sExceptionName = "STATUS_INVALID_DISPOSITION";
			break;
		case STATUS_ARRAY_BOUNDS_EXCEEDED:
			sExceptionName = "STATUS_ARRAY_BOUNDS_EXCEEDED";
			break;
		case STATUS_FLOAT_DENORMAL_OPERAND:
			sExceptionName = "STATUS_FLOAT_DENORMAL_OPERAND";
			break;
		case STATUS_FLOAT_DIVIDE_BY_ZERO:
			sExceptionName = "STATUS_FLOAT_DIVIDE_BY_ZERO";
			break;
		case STATUS_FLOAT_INEXACT_RESULT:
			sExceptionName = "STATUS_FLOAT_INEXACT_RESULT";
			break;
		case STATUS_FLOAT_INVALID_OPERATION:
			sExceptionName = "STATUS_FLOAT_INVALID_OPERATION";
			break;
		case STATUS_FLOAT_OVERFLOW:
			sExceptionName = "STATUS_FLOAT_OVERFLOW";
			break;
		case STATUS_FLOAT_STACK_CHECK:
			sExceptionName = "STATUS_FLOAT_STACK_CHECK";
			break;
		case STATUS_FLOAT_UNDERFLOW:
			sExceptionName = "STATUS_FLOAT_UNDERFLOW";
			break;
		case STATUS_INTEGER_DIVIDE_BY_ZERO:
			sExceptionName = "STATUS_INTEGER_DIVIDE_BY_ZERO";
			break;
		case STATUS_INTEGER_OVERFLOW:
			sExceptionName = "STATUS_INTEGER_OVERFLOW";
			break;
		case STATUS_PRIVILEGED_INSTRUCTION:
			sExceptionName = "STATUS_PRIVILEGED_INSTRUCTION";
			break;
		case STATUS_STACK_OVERFLOW:
			sExceptionName = "STATUS_STACK_OVERFLOW";
			break;
		case 0xC0000135:
			sExceptionName = "STATUS_DLL_NOT_FOUND";
			break;
		case 0xC0000142:
			sExceptionName = "STATUS_DLL_INIT_FAILED";
			break;
		case 0XC06D007E:
			sExceptionName = "Delay-load module not found";
			break;
		case 0xC06d007F:
			sExceptionName = "Delay-load procedure not found";
			break;
		default:
			sExceptionName = "Unknown system exception";
  }

	_snprintf(msg, sizeof(msg), "Exception 0x%08X [%s]\nModule:   %s\nAddress:  0x%08p", pExcept->ExceptionRecord->ExceptionCode, sExceptionName.c_str(), sExceptionModule.c_str(), pExcept->ExceptionRecord->ExceptionAddress);

  throw exception(msg);
}


HMODULE GetExceptionModule(LPVOID address, LPSTR moduleName)
{
	HMODULE moduleList[1024];
	DWORD sizeNeeded = 0;
	if(FALSE == EnumProcessModules(GetCurrentProcess(), moduleList, 1024, &sizeNeeded) || sizeNeeded < sizeof(HMODULE))
		return NULL;

	int curModule = -1;
	for (unsigned int i = 0; i < (sizeNeeded / sizeof(HMODULE)); ++i)
	{
		if ((DWORD)moduleList[i] < (DWORD)address)
		{
			if (curModule == -1)
				curModule = i;
			else
			{
				if ((DWORD)moduleList[curModule] < (DWORD)moduleList[i])
					curModule = i;
			}
		}
	}

	if (curModule == -1)
		return NULL;

	if (!GetModuleFileName(moduleList[curModule], moduleName, MAX_PATH))
		return NULL;

 return moduleList[curModule];
}


BOOL GetFileNameFromHandle(HANDLE hFile, string &sFileName)
{
	BOOL bSuccess = FALSE;
	char msg[1024];
	char pszFilename[MAX_PATH + 1];
	HANDLE hFileMap;

	hFileMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 1, NULL);

	if (hFileMap)
	{
		void* pMem = MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 1);
		if (pMem)
		{
			if (GetMappedFileName (GetCurrentProcess(), pMem, pszFilename, MAX_PATH))
			{
				char szTemp[BUFSIZE];
				szTemp[0] = '\0';
				if (GetLogicalDriveStrings(BUFSIZE - 1, szTemp))
				{
					char szName[MAX_PATH];
					char szDrive[3] = TEXT(" :");
					BOOL bFound = FALSE;
					char* p = szTemp;

					do
					{
						*szDrive = *p;
						if (QueryDosDevice(szDrive, szName, MAX_PATH))
						{
							size_t uNameLen = strlen(szName);
							if (uNameLen < MAX_PATH)
							{
								bFound = _strnicmp(pszFilename, szName, uNameLen) == 0 && *(pszFilename + uNameLen) == '\\';
								if (bFound)
								{
									_snprintf(msg, sizeof(msg), "%s%s", szDrive, pszFilename+uNameLen);
									sFileName = msg;
								}
							}
						}

						while (*p++);
					} while (!bFound && *p); // end of string
				}
			}
			bSuccess = TRUE;
			UnmapViewOfFile(pMem);
		}
		CloseHandle(hFileMap);
	}
	else
	  return FALSE;

  return (bSuccess);
}


static string SysErrorMessage()
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

	string sRet(szMsg);
	sRet.erase(sRet.find_last_not_of(" \t\n\r") + 1);
	sRet.erase(0, sRet.find_first_not_of(" \t\n\r"));

	return sRet;
}

