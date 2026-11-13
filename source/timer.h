// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#ifndef _TIMER_H
#define _TIMER_H

#include "stdafx.h"

using namespace std;

class CTimer
{
public:
	CTimer();
	virtual      ~CTimer();
	double       GetTimer();
	double       GetMMTimer();
	BOOL         TestTimer();
	string       FormatTimeString(unsigned int ui_milliseconds);
	string       StrFormat(const char *fmt, ...);
};

CTimer::CTimer()
{
}

CTimer::~CTimer()
{
}


BOOL CTimer::TestTimer()
{
	LARGE_INTEGER liPerfCounter = {0,0};
	LARGE_INTEGER liPerfFreq = {0,0};

	if ((::QueryPerformanceFrequency(&liPerfFreq) == 0) || (::QueryPerformanceCounter(&liPerfCounter) == 0))
		return FALSE;

	TIMECAPS tc;
	if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR) 
		return FALSE;

	double start = GetTimer();
	::Sleep(1000);
	double dtesttime = GetTimer() - start;
		
	if ((dtesttime > 0.7) && (dtesttime < 1.3))
		return TRUE;

	return FALSE;
}


double CTimer::GetTimer()
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


double CTimer::GetMMTimer()
{
	return (double)timeGetTime() / 1000.0;
}


string CTimer::FormatTimeString(unsigned int ui_milliseconds)
{
	unsigned int hours = ui_milliseconds / 3600000;
	unsigned int minutes = (ui_milliseconds / 60000) % 60;
	unsigned int seconds = (ui_milliseconds / 1000) % 60;
	unsigned int milliseconds = ui_milliseconds % 1000;

	return StrFormat("%03d:%02d:%02d.%03d", hours, minutes, seconds, milliseconds);
}


string CTimer::StrFormat(const char *fmt, ...)
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


#endif //_TIMER_H

