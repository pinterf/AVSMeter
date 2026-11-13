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
#define WIN32_LEAN_AND_MEAN

#pragma once

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
#include <eh.h>

using namespace std;

#include "avisynth.h"
const AVS_Linkage *AVS_linkage = 0;

class SE_Exception
{
private:
	unsigned int nSE;
public:
	SE_Exception() {}
	SE_Exception(unsigned int n) : nSE(n) {}
	~SE_Exception() {}
	unsigned int GetCode() {return nSE;}
};

void SE_Translator(unsigned int u, _EXCEPTION_POINTERS* pExp) {throw SE_Exception(u);}

//test STATUS_FLOAT_DIVIDE_BY_ZERO:
//		int a = 0;
//		int b = 100;
//		volatile float c = b/a;

string SE_ErrorMessage(unsigned int code)
{
	string sRet = "Unknown system exception";

	switch (code)
	{
		case STATUS_GUARD_PAGE_VIOLATION:
			sRet = "STATUS_GUARD_PAGE_VIOLATION";
			break;
		case STATUS_DATATYPE_MISALIGNMENT:
			sRet = "STATUS_DATATYPE_MISALIGNMENT";
			break;
		case STATUS_BREAKPOINT:
			sRet = "STATUS_BREAKPOINT";
			break;
		case STATUS_SINGLE_STEP:
			sRet = "STATUS_SINGLE_STEP";
			break;
		case STATUS_ACCESS_VIOLATION:
			sRet = "STATUS_ACCESS_VIOLATION";
			break;
		case STATUS_IN_PAGE_ERROR:
			sRet = "STATUS_IN_PAGE_ERROR";
			break;
		case STATUS_INVALID_HANDLE:
			sRet = "STATUS_INVALID_HANDLE";
			break;
		case STATUS_NO_MEMORY:
			sRet = "STATUS_NO_MEMORY";
			break;
		case STATUS_ILLEGAL_INSTRUCTION:
			sRet = "STATUS_ILLEGAL_INSTRUCTION";
			break;
		case STATUS_NONCONTINUABLE_EXCEPTION:
			sRet = "STATUS_NONCONTINUABLE_EXCEPTION";
			break;
		case STATUS_INVALID_DISPOSITION:
			sRet = "STATUS_INVALID_DISPOSITION";
			break;
		case STATUS_ARRAY_BOUNDS_EXCEEDED:
			sRet = "STATUS_ARRAY_BOUNDS_EXCEEDED";
			break;
		case STATUS_FLOAT_DENORMAL_OPERAND:
			sRet = "STATUS_FLOAT_DENORMAL_OPERAND";
			break;
		case STATUS_FLOAT_DIVIDE_BY_ZERO:
			sRet = "STATUS_FLOAT_DIVIDE_BY_ZERO";
			break;
		case STATUS_FLOAT_INEXACT_RESULT:
			sRet = "STATUS_FLOAT_INEXACT_RESULT";
			break;
		case STATUS_FLOAT_INVALID_OPERATION:
			sRet = "STATUS_FLOAT_INVALID_OPERATION";
			break;
		case STATUS_FLOAT_OVERFLOW:
			sRet = "STATUS_FLOAT_OVERFLOW";
			break;
		case STATUS_FLOAT_STACK_CHECK:
			sRet = "STATUS_FLOAT_STACK_CHECK";
			break;
		case STATUS_FLOAT_UNDERFLOW:
			sRet = "STATUS_FLOAT_UNDERFLOW";
			break;
		case STATUS_INTEGER_DIVIDE_BY_ZERO:
			sRet = "STATUS_INTEGER_DIVIDE_BY_ZERO";
			break;
		case STATUS_INTEGER_OVERFLOW:
			sRet = "STATUS_INTEGER_OVERFLOW";
			break;
		case STATUS_PRIVILEGED_INSTRUCTION:
			sRet = "STATUS_PRIVILEGED_INSTRUCTION";
			break;
		case STATUS_STACK_OVERFLOW:
			sRet = "STATUS_STACK_OVERFLOW";
			break;
		case 0xC0000135:
			sRet = "STATUS_DLL_NOT_FOUND";
			break;
		case 0xC0000142:
			sRet = "STATUS_DLL_INIT_FAILED";
			break;
		case 0XC06D007E:
			sRet = "Delay-load module not found";
			break;
		case 0xC06d007F:
			sRet = "Delay-load procedure not found";
			break;
		default:
			sRet = "Unknown system exception";
  }

	char buff[512];
  _snprintf(buff, sizeof(buff), "Exception 0x%08X\n%s", code, sRet.c_str());
  sRet = buff;

	return sRet;
}


string SysErrorMessage()
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

