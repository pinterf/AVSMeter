#if !defined(_AVSMETER_H)
#define _AVSMETER_H

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

#include "common.h"
#include "exception.h"
#include "AvisynthInfo.h"
#include "Utility.h"
#include "SysInfo.h"
#include "ProcessInfo.h"
#include "GPUInfo.h"
#include "Timer.h"


#define COLOR_DEFAULT           0
#define COLOR_AVSM_VERSION      FG_HRED | BG_BLACK
#define COLOR_AVISYNTH_VERSION  FG_HGREEN | BG_BLACK
#define COLOR_EMPHASIS          FG_WHITE | BG_BLACK
#define COLOR_ERROR             FG_HCYAN | BG_BLACK

#define REFRESH_INTERVAL 0.333333

class CSettings
{
public:
	string    sLogDirectory;
	string    sAVSDLL;
	BOOL      bDisplayFPS;
	BOOL      bDisplayTPF;
	BOOL      bDisplayEfficiencyIndex;
	BOOL      bCreateLog;
	BOOL      bCreateCSV;
	BOOL      bPauseBeforeExit;
	__int64   iStartFrame;
	__int64   iStopFrame;
	string    sFrameRange;
	__int64   iTimeLimit;
	BOOL      bInvokeDistributor;
	BOOL      bAllowOnlyOneInstance;
	BOOL      bUseColor;
	BOOL      bMonitorGPULoad;
	BYTE      nProcessPriority;
	BOOL      bConUseStdOut;
	BOOL      bLogEstimatedTime;
	BOOL      bAutoCompleteExtension;
	string    sSystemTime;
	BOOL      bUseUTC;
	BOOL      bLogFileDateTimeSuffix;
	BOOL      bSimpleMode;
};


class CPerformanceData
{
public:
	unsigned int  current_frame;
	float         fps_current;
	float         fps_average;
	BYTE          cpu_usage;
	BYTE          gpu_usage;
	BYTE          vpu_usage;
	DWORD         phys_mem;
	DWORD         virt_mem;
	WORD          num_threads;
};


CAvisynthInfo AvisynthInfo;
CUtils utils;
CSysInfo sys;
CTimer timer;
CSettings Settings;

unsigned int CalculateFrameInterval(string &s_avsfile, string &s_error);
string       CreateLogFile(string &s_avsfile, string &s_logbuffer, string &s_gpuinfo, vector<CPerformanceData> &cs_pdata, string &s_avserror, BOOL bNVVP, BOOL bOmitPerfData);
string       CreateCSVFile(string &s_avsfile, vector<CPerformanceData> &cs_pdata, BOOL bNVVP);
string       ParseINIFile();
BOOL         WriteINIFile(string &s_inifile);
void         PrintUsage();
void         PreExit();
void         PrintConsole(BOOL bUseStdOut, WORD wAttributes, const char *fmt, ...);

#endif //_AVSMETER_H
