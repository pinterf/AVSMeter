// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#ifndef _TIMER_H
#define _TIMER_H

#include "stdafx.h"

class CTimer
{
public:
	CTimer();
	virtual      ~CTimer();
	double       GetTimer();
	double       GetSTDTimer();
	BOOL         TestTimer();
	string       FormatTimeString(__int64 i_milliseconds, BOOL b_rightaligned);
	string       StrFormat(const char *fmt, ...);
private:
	unsigned __int64  uiSTDTimer;
	unsigned __int64  uiLastSTDTimer;
	unsigned __int64  uiTimerWrapCompensation;
};

CTimer::CTimer()
{
	uiLastSTDTimer = 0;
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

	double start = GetTimer();
	::Sleep(500);
	double dtesttime = GetTimer() - start;
		
	if ((dtesttime > 0.4) && (dtesttime < 0.6))
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


double CTimer::GetSTDTimer()
{
	uiSTDTimer = (unsigned __int64)GetTickCount();
	uiTimerWrapCompensation = (uiSTDTimer < uiLastSTDTimer) ? 4294967296 : 0;
	uiLastSTDTimer = uiSTDTimer;
	uiSTDTimer += uiTimerWrapCompensation;

	return (double)uiSTDTimer / 1000.0;
}


string CTimer::FormatTimeString(__int64 i_milliseconds, BOOL b_rightaligned)
{
	string sTime = "";
	unsigned int hours = (unsigned int)(i_milliseconds / 3600000);
	unsigned int minutes = (unsigned int)(i_milliseconds / 60000) % 60;
	unsigned int seconds = (unsigned int)(i_milliseconds / 1000) % 60;
	unsigned int milliseconds = (unsigned int)i_milliseconds % 1000;

	if (hours < 100)
		sTime = StrFormat("   %02d:%02d:%02d.%03d", hours, minutes, seconds, milliseconds);
	else
		sTime = StrFormat("%5d:%02d:%02d.%03d", hours, minutes, seconds, milliseconds);

	if (!b_rightaligned)
		sTime.erase(0, sTime.find_first_not_of(" "));

	return sTime;
}


string CTimer::StrFormat(const char *fmt, ...)
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


#endif //_TIMER_H

