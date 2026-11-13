#if !defined(_EXCEPTION_H)
#define _EXCEPTION_H

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

#include "common.h"

static string GetExceptionModule(LPVOID address);

//test STATUS_FLOAT_DIVIDE_BY_ZERO:
//	int a = 0;
//	int b = 100;
//	volatile float c = b/a;

//test STATUS_ACCESS_VIOLATION
//	volatile int *pInt = 0x00000000;
//	*pInt = 20;


static void SE_Translator(unsigned int, EXCEPTION_POINTERS* pExcept)
{
  char msg[2048];
	string sModule = "";
	string sExceptionName = "Unknown system exception";
	string sExceptionModule = GetExceptionModule(pExcept->ExceptionRecord->ExceptionAddress);

	if (sExceptionModule == "")
		sExceptionModule = "Cannot determine module";

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


static string GetExceptionModule(LPVOID address)
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

	if (iCurModule == -1)
		return "";

	if (!GetModuleFileName(hModules[iCurModule], modulename, MAX_PATH))
		return "";

	string sModule(modulename);

	#ifndef AVS64
	SYSTEM_INFO stInfo;
	GetNativeSystemInfo(&stInfo);
	if ((stInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) || (stInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64))
	{
		HANDLE hFile = CreateFile(sModule.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		if (hFile != INVALID_HANDLE_VALUE)
		{
			char pszFilename[2048];
			HANDLE hFileMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 1, NULL);;
			if (hFileMap)
			{
				void* pMem = MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 1);
				if (pMem)
				{
					if (GetMappedFileName(GetCurrentProcess(), pMem, pszFilename, 2046))
					{
						char pszWinDir[2048];
						GetWindowsDirectory(pszWinDir, 2046);
						string sWinDir(pszWinDir);
						sWinDir = sWinDir.substr(2);
						string sTestSystem32 = sWinDir + "\\system32\\";
						string sTestSysWOW64 = sWinDir + "\\syswow64\\";
						transform(sTestSystem32.begin(), sTestSystem32.end(), sTestSystem32.begin(), ::tolower);
						transform(sTestSysWOW64.begin(), sTestSysWOW64.end(), sTestSysWOW64.begin(), ::tolower);
						string sTempDLLPath = sModule;
						string sRealDLLPath(pszFilename);
						transform(sRealDLLPath.begin(), sRealDLLPath.end(), sRealDLLPath.begin(), ::tolower);
						transform(sTempDLLPath.begin(), sTempDLLPath.end(), sTempDLLPath.begin(), ::tolower);
						if ((sRealDLLPath.find(sTestSysWOW64) != std::string::npos) && (sTempDLLPath.find(sTestSystem32) != std::string::npos))
						{
							size_t spos = sTempDLLPath.find("\\system32\\");
							if (spos > 2)
								sModule = sModule.substr(0, spos) + "\\SysWOW64\\" + sModule.substr(spos + 10);
						}
					}
				}
				UnmapViewOfFile(pMem);
			}
			CloseHandle(hFileMap);
		}
		CloseHandle(hFile);
	}
	#endif //AVS64

	return sModule;
}


static string SysErrorMessage()
{
	DWORD dwLastError = GetLastError();
	LPTSTR msgBuf;

	if (dwLastError == ERROR_SUCCESS)
		return "";

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
								NULL,
								dwLastError,
								MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
								(LPTSTR)&msgBuf,
								0,
								NULL);

	string sRet(msgBuf);
	LocalFree(msgBuf);
	sRet.erase(sRet.find_last_not_of(" \t\n\r") + 1);
	sRet.erase(0, sRet.find_first_not_of(" \t\n\r"));

	return sRet;
}

#endif //_EXCEPTION_H

