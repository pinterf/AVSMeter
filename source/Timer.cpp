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

#include "timer.h"

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
	static unsigned __int64 uiLastSTDTimer;
	unsigned __int64 uiSTDTimer = (unsigned __int64)GetTickCount();
	unsigned __int64 uiTimerWrapComp = (uiSTDTimer < uiLastSTDTimer) ? 4294967296 : 0;
	uiLastSTDTimer = uiSTDTimer;

	return (double)(uiSTDTimer + uiTimerWrapComp) / 1000.0;
}


string CTimer::FormatTimeString(__int64 i_milliseconds, BOOL b_rightaligned)
{
	char buff[512];
	string sTime = "";
	unsigned int hours = (unsigned int)(i_milliseconds / 3600000);
	unsigned int minutes = (unsigned int)(i_milliseconds / 60000) % 60;
	unsigned int seconds = (unsigned int)(i_milliseconds / 1000) % 60;
	unsigned int milliseconds = (unsigned int)i_milliseconds % 1000;

	if (hours < 100)
		_snprintf(buff, sizeof(buff), "   %02d:%02d:%02d.%03d", hours, minutes, seconds, milliseconds);
	else
		_snprintf(buff, sizeof(buff), "%5d:%02d:%02d.%03d", hours, minutes, seconds, milliseconds);

	sTime = buff;

	if (!b_rightaligned)
		sTime.erase(0, sTime.find_first_not_of(" "));

	return sTime;
}

