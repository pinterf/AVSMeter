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

#if !defined(_EXCEPTION_H)
#define _EXCEPTION_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "common.h"

//test STATUS_FLOAT_DIVIDE_BY_ZERO:
//	int a = 0;
//	int b = 100;
//	volatile float c = b/a;

//test STATUS_ACCESS_VIOLATION
//	volatile int *pInt = 0x00000000;
//	*pInt = 20;

string GetWOW64FilePath(string s_filepath);
string GetExceptionModule(LPVOID address);
void SE_Translator(unsigned int, EXCEPTION_POINTERS* pExcept);

string GetWOW64FilePath(string s_filepath)
{
	string sRealFilePath = s_filepath;

	BOOL bWoW64Process = FALSE;
	typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
	LPFN_ISWOW64PROCESS fnIsWow64Process;

	HMODULE hKernel32 = GetModuleHandle("kernel32.dll");
	if (hKernel32 == NULL) return sRealFilePath;

	fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(hKernel32, "IsWow64Process");
	if (fnIsWow64Process != NULL)
		fnIsWow64Process(GetCurrentProcess(), &bWoW64Process);
	else
		return sRealFilePath;

	if (bWoW64Process)
	{
		char pszWinDir[1026];
		GetWindowsDirectory(pszWinDir, 1024);
		string sTestSystem32(pszWinDir);
		sTestSystem32 = sTestSystem32 + "\\system32\\";
		transform(sTestSystem32.begin(), sTestSystem32.end(), sTestSystem32.begin(), ::tolower);
		string sTempDLLPath = s_filepath;
		transform(sTempDLLPath.begin(), sTempDLLPath.end(), sTempDLLPath.begin(), ::tolower);
		size_t slen = sTestSystem32.length();
		if (sTempDLLPath.substr(0, slen) == sTestSystem32)
		{
			char szSysWOW64Directory[1026];
			typedef UINT (WINAPI *LPFN_GETSYSTEMWOW64DIRECTORY)(LPTSTR, UINT);
			LPFN_GETSYSTEMWOW64DIRECTORY getSystemWow64Directory;

			getSystemWow64Directory = (LPFN_GETSYSTEMWOW64DIRECTORY)GetProcAddress(hKernel32, "GetSystemWow64DirectoryA");
			if (getSystemWow64Directory == NULL) return sRealFilePath;

			if (getSystemWow64Directory(szSysWOW64Directory, sizeof(szSysWOW64Directory)) > 1)
			{
				string sSysWOW64Dir(szSysWOW64Directory);
				sRealFilePath = sSysWOW64Dir + "\\" + s_filepath.substr(slen);
			}
		}
	}

	return sRealFilePath;
}


string GetExceptionModule(LPVOID address)
{
	char modulename[2048];
	HMODULE hModules[2048];
	DWORD dwSizeNeeded = 0;
	if(!EnumProcessModules(GetCurrentProcess(), hModules, sizeof(hModules), &dwSizeNeeded) || dwSizeNeeded < sizeof(HMODULE))
		return "";

	int iCurModule = -1;
	for (unsigned int i = 0; i < (dwSizeNeeded / sizeof(HMODULE)); i++)
	{
		if (hModules[i] < address)
		{
			if (iCurModule == -1)
				iCurModule = i;
			else
			{
				if (hModules[iCurModule] < hModules[i])
					iCurModule = i;
			}
		}
	}

	if (iCurModule == -1) return "";

	if (!GetModuleFileName(hModules[iCurModule], modulename, MAX_PATH)) return "";

	string sModule(modulename);
	sModule = GetWOW64FilePath(sModule);

	return sModule;
}


void SE_Translator(unsigned int, EXCEPTION_POINTERS* pExcept)
{
  char msg[2048];
	string sModule = "";
	string sExceptionName = "Unknown system exception";
	string sExceptionModule = GetExceptionModule(pExcept->ExceptionRecord->ExceptionAddress);

	if (sExceptionModule == "") sExceptionModule = "Cannot determine module";

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


#endif //_EXCEPTION_H
