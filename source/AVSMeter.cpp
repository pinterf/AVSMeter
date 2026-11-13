// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include "stdafx.h"
#include "utility.h"
#include "ProcessInfo.h"
#include "GPUInfo.h"
#include "timer.h"

using namespace std;

#ifdef AVS64
	#include "avisynth26.h"
	const AVS_Linkage *AVS_linkage = 0;
#else
	#include "avisynth25.h"
#endif

#define COLOR_AVSM_VERSION      FG_HRED | BG_BLACK
#define COLOR_AVISYNTH_VERSION  FG_HGREEN | BG_BLACK
#define COLOR_EMPHASIS          FG_WHITE | BG_BLACK
#define COLOR_ERROR             FG_HCYAN | BG_BLACK


struct sAvisynthInfo
{
	string sDLLPath;
	string sErrorMsg;
	int    iInterfaceVersion;
	string sVersion;
	string sVersionString;
	BOOL   bAVSLinkage;
	BOOL   bIsCompatible;
	BOOL   bIsMTVersion;
	BOOL   bLoadError;
} AvisynthInfo;


struct sINISettings
{
	BOOL    bCreateLog;
	BOOL    bPauseBeforeExit;
	__int64 iStartFrame;
	__int64 iStopFrame;
	string  sFrameRange;
	BOOL    bInvokeDistributor;
	BOOL    bAllowOnlyOneInstance;
	BOOL    bLogEstimatedTime;
	BOOL    bUseColor;
	BOOL    bMonitorGPULoad;
	WORD    wReadTimeout;
} INISettings;

typedef struct sPerformanceData
{
	unsigned int frame_low;
	unsigned int frame_high;
	double fps;
	WORD  cpu_usage;
	WORD  gpu_usage;
	DWORD phys_mem;
	DWORD virt_mem;
	WORD num_threads;
} perfdata;

typedef struct ThreadParameters
{
	string sAVSFile;
	unsigned int uiFrameInterval;
	string sAVSError;
	unsigned int uiCurFrame;
	BOOL bFinished;
} ThreadParams;


CUtils utils;
CTimer timer;

void CalculateFrameInterval(PVOID pParam);
string CreateLogFile(string &s_avsfile, string &s_logbuffer, perfdata * cs_pdata, unsigned int ui_index, string &s_avserror, unsigned int ui_frameinterval);
string ParseINIFile();
BOOL GetAvisynthInfo();
void PrintUsage();
void Pause();
void printcon(WORD wAttributes, const char *fmt, ...);

int main(int argc, char* argv[])
{
	if (!utils.IsUserAdmin())
	{
		INISettings.bPauseBeforeExit = TRUE;
		printcon(COLOR_ERROR, "\nYou must run AVSMeter with administrator privileges.\n");
		Pause();
		return -1;
	}

	int iRet = 0;
	string sAVSMVersion = "";
	if (!utils.GetProductVersion(sAVSMVersion))
	{
		printcon(COLOR_ERROR, "\nCannot retrieve product version:\n%s\n", utils.OSErrorMessage().c_str());
		Pause();
		return -1;
	}

	#ifdef AVS64
		sAVSMVersion += " (x64)";
	#else
		sAVSMVersion += " (x86)";
	#endif

	string sRet = ParseINIFile();
	if (sRet != "")
	{
		printcon(COLOR_ERROR, sRet.c_str());
		Pause();
		return -1;
	}

	printcon(COLOR_AVSM_VERSION, "\nAVSMeter %s by Groucho2004\n", sAVSMVersion.c_str());

	string sConsoleOut = "";
	unsigned int uiConsoleWidth = utils.GetConsoleWidth();
	string sArg = "";
	string sArgTest = "";
	string sAVSFile = "";
	string sLogBuffer = "";
	perfdata * PerformanceData = 0;
	unsigned int uiPDIndex = 0;
	BOOL bEarlyExit = TRUE;
	BOOL bInfoOnly = FALSE;
	string sAVSError = "";
	unsigned int uiFrameInterval = 0;

	BOOL bRet = GetAvisynthInfo();

	if (!bRet)
	{
		if (AvisynthInfo.bLoadError)
		{
			printcon(COLOR_ERROR, "\nCannot load avisynth.dll:\n%s\n", utils.OSErrorMessage().c_str());
			Pause();
			return -1;
		}

		if (!AvisynthInfo.bIsCompatible)
		{
			#ifdef AVS64
				printcon(COLOR_ERROR, "\nCannot load avisynth.dll.\nMake sure you are using a 64 Bit version.\n");
			#else
				printcon(COLOR_ERROR, "\nCannot load avisynth.dll.\nMake sure you are using a 32 Bit version.\n");
			#endif

			Pause();
			return -1;
		}

		if (AvisynthInfo.sErrorMsg != "")
		{
			printcon(COLOR_ERROR, "\n%s\n", AvisynthInfo.sErrorMsg.c_str());
			Pause();
			return -1;
		}
	}


	if (!AvisynthInfo.bAVSLinkage)
	{
		#ifdef AVS64
			printcon(COLOR_ERROR, "\nAvisynth 2.6 x64 is required.\n");
			Pause();
			return -1;
		#endif
	}

	printcon(COLOR_AVISYNTH_VERSION, "\r%s", AvisynthInfo.sVersionString.c_str());
	if (AvisynthInfo.bIsMTVersion)
		printcon(COLOR_EMPHASIS, " (%s) (MT)\n", AvisynthInfo.sVersion.c_str());
	else
		printcon(COLOR_EMPHASIS, " (%s)\n", AvisynthInfo.sVersion.c_str());

	if (INISettings.bAllowOnlyOneInstance)
	{
		CreateMutex(NULL, TRUE, "__avsmeter__single__instance__lock__");
		if (GetLastError() == ERROR_ALREADY_EXISTS)
		{
			printcon(COLOR_ERROR, "\nOne instance of AVSMeter is already running\n");
			Pause();
			return -1;
		}
	}

	if (argc < 2)
	{
		PrintUsage();
		Pause();
		return -1;
	}

	for (int iArg = 1; iArg < argc; iArg++)
	{
		sArg = argv[iArg];
		sArgTest = sArg;
		utils.StrToLC(sArgTest);
		utils.StrTrim(sArgTest);

		if ((sArgTest == "-l") || (sArgTest == "-log"))
		{
			INISettings.bCreateLog = TRUE;
			continue;
		}

		if ((sArgTest == "-i") || (sArgTest == "-info"))
		{
			bInfoOnly = TRUE;
			continue;
		}

		if (sArgTest == "-gpu")
		{
			INISettings.bMonitorGPULoad = TRUE;
			continue;
		}

		if (sArgTest.length() > 4)
		{
			if (sArgTest.substr(sArgTest.length() - 4) == ".avs")
			{
				LPTSTR lpPart;
				char szOut[2050];
				if (::GetFullPathName(argv[iArg], 2048, szOut, &lpPart))
				{
					sAVSFile = szOut;
					if (!utils.FileExists(sAVSFile))
					{
						sConsoleOut = utils.StrFormat("\nFile not found: \"%s\"\n", sArg.c_str());
						printcon(COLOR_ERROR, "%s", sConsoleOut.c_str());
						Pause();
						return -1;
					}
				}
			}
			continue;
		}

		printcon(COLOR_ERROR, "\nInvalid argument: \"%s\"\n", sArg.c_str());
		Pause();
		return -1;
	}

	if (sAVSFile == "")
	{
		printcon(COLOR_ERROR, "\nNo script file specified\n");
		Pause();
		return -1;
	}

	CGPUInfo gpuinfo;

	if (!bInfoOnly)
	{
		if (INISettings.bMonitorGPULoad)
		{
			if (!gpuinfo.bGPUZIsRunning)
			{
				printcon(COLOR_ERROR, "\nIn order to measure GPU usage, GPU-Z must be running.\nStart GPU-Z and try again.\n");
				Pause();
				return -1;
			}
		}

		sAVSError = "";

		ThreadParams* p = new ThreadParams;
		p->bFinished = FALSE;
		p->sAVSError = sAVSError;
		p->sAVSFile = sAVSFile;

		_beginthread(CalculateFrameInterval, 0, p);

		DWORD dwT0 = GetTickCount();
		DWORD dwT1 = dwT0;

		for (;;)
		{
			dwT1 = GetTickCount();
			if ((dwT1 - dwT0) > (DWORD)(INISettings.wReadTimeout * 1000))
			{
				printf("\r%s", utils.StrPad("", uiConsoleWidth).c_str());
				printcon(COLOR_ERROR, "\nAvisynth seems to be stuck on reading frame %u.\nTimeout of %u seconds exceeded.\n", p->uiCurFrame, INISettings.wReadTimeout);
				Pause();
				return -1;
			}

			printf("\r%s\r", utils.StrPad("Analysing script", uiConsoleWidth).c_str());
			if (p->bFinished)
				break;
			Sleep(250);

			printf("\r%s\r", utils.StrPad("Analysing script.", uiConsoleWidth).c_str());
			if (p->bFinished)
				break;
			Sleep(250);

			printf("\r%s\r", utils.StrPad("Analysing script..", uiConsoleWidth).c_str());
			if (p->bFinished)
				break;
			Sleep(250);

			printf("\r%s\r", utils.StrPad("Analysing script...", uiConsoleWidth).c_str());
			if (p->bFinished)
				break;
			Sleep(250);

			printf("\r%s\r", utils.StrPad("Analysing script....", uiConsoleWidth).c_str());
			if (p->bFinished)
				break;
			Sleep(250);

			printf("\r%s\r", utils.StrPad("Analysing script.....", uiConsoleWidth).c_str());
			if (p->bFinished)
				break;
			Sleep(250);
		}

		sAVSError = p->sAVSError;
		uiFrameInterval = p->uiFrameInterval;

		printf("\r%s", utils.StrPad("", uiConsoleWidth).c_str());
		if ((uiFrameInterval == 0) || (sAVSError != ""))
		{
			printcon(COLOR_ERROR, "\n%s\n", sAVSError.c_str());
			Pause();
			return -1;
		}
	}


	HINSTANCE hDLL = ::LoadLibrary("avisynth");
	if (!hDLL)
	{
		printcon(COLOR_ERROR, "\nCannot load avisynth.dll\n");
		Pause();
		return -1;
	}

	try
	{
		IScriptEnvironment *(__stdcall *CreateEnvironment)(int) = (IScriptEnvironment *(__stdcall *)(int))::GetProcAddress(hDLL, "CreateScriptEnvironment");
		if (!CreateEnvironment)
		{
			printcon(COLOR_ERROR, "\nFailed to load CreateScriptEnvironment()\n");
			Pause();
			return -1;
		}

		IScriptEnvironment *AVS_env = CreateEnvironment(AVISYNTH_INTERFACE_VERSION);
		if (!AVS_env)
		{
			printcon(COLOR_ERROR, "\nCould not create IScriptenvironment\n");
			Pause();
			return -1;
		}

		#ifdef AVS64
			AVS_linkage = AVS_env->GetAVSLinkage();
		#endif

		AVSValue AVS_main;
		AVSValue AVS_temp;
		PClip AVS_clip;
		VideoInfo	AVS_vidinfo;

		AVS_main = AVS_env->Invoke("Import", sAVSFile.c_str());

		if (!AVS_main.IsClip())
		{
			sConsoleOut = utils.StrFormat("\"%s\":\nScript did not return a video clip", sAVSFile.c_str());
			AVS_env->ThrowError(sConsoleOut.c_str());
		}

		BOOL bIsMTVersion = TRUE;
		int iMTMode = 0;
		try
		{
			AVS_temp = AVS_env->Invoke("GetMTMode", false);
			iMTMode = AVS_temp.IsInt() ? AVS_temp.AsInt() : 0;
			if ((iMTMode > 0) && (iMTMode < 5) && INISettings.bInvokeDistributor)
				AVS_main = AVS_env->Invoke("Distributor", AVS_main);
		}
		catch (IScriptEnvironment::NotFound)
		{
			bIsMTVersion = FALSE;
		}

		AVS_clip = AVS_main.AsClip();
		AVS_vidinfo = AVS_clip->GetVideoInfo();
		unsigned int uiFrames = (unsigned int)AVS_vidinfo.num_frames;
		__int64 iMilliSeconds = (__int64)((((double)uiFrames * (double)AVS_vidinfo.fps_denominator * 1000.0) / (double)AVS_vidinfo.fps_numerator) + 0.5);

		sLogBuffer += "[General info]\n";
		sConsoleOut = utils.StrFormat("Log file created with:   AVSMeter %s", sAVSMVersion.c_str());
		sLogBuffer += sConsoleOut + "\n";

		sConsoleOut = utils.StrFormat("Avisynth version:        %s", AvisynthInfo.sVersionString.c_str());
		if (AvisynthInfo.bIsMTVersion)
			sConsoleOut += utils.StrFormat(" (%s) (MT)", AvisynthInfo.sVersion.c_str());
		else
			sConsoleOut += utils.StrFormat(" (%s)", AvisynthInfo.sVersion.c_str());

		sLogBuffer += sConsoleOut + "\n";

		printf("\r%s", utils.StrPad("", uiConsoleWidth).c_str());

		if (bIsMTVersion)
		{
			sConsoleOut = utils.StrFormat("Active MT Mode: %d", iMTMode);
			printf("\r%s\n", sConsoleOut.c_str());
			sLogBuffer += "                         " + sConsoleOut + "\n";
		}

		sLogBuffer += "\n\n[Clip info]\n";

		sConsoleOut = utils.StrFormat("Number of frames:          %11u", AVS_vidinfo.num_frames);
		printf("\n%s", sConsoleOut.c_str());
		sLogBuffer += sConsoleOut + "\n";

		sConsoleOut = utils.StrFormat("Length (hh:mm:ss.ms):  %s", timer.FormatTimeString(iMilliSeconds, TRUE).c_str());
		printf("\n%s", sConsoleOut.c_str());
		sLogBuffer += sConsoleOut + "\n";

		sConsoleOut = utils.StrFormat("Frame width:               %11u", AVS_vidinfo.width);
		printf("\n%s", sConsoleOut.c_str());
		sLogBuffer += sConsoleOut + "\n";

		sConsoleOut = utils.StrFormat("Frame height:              %11u", AVS_vidinfo.height);
		printf("\n%s", sConsoleOut.c_str());
		sLogBuffer += sConsoleOut + "\n";

		sConsoleOut = utils.StrFormat("Framerate:                 %11.3f (%u/%u)", (double)AVS_vidinfo.fps_numerator / (double)AVS_vidinfo.fps_denominator, AVS_vidinfo.fps_numerator, AVS_vidinfo.fps_denominator);
		printf("\n%s", sConsoleOut.c_str());
		sLogBuffer += sConsoleOut + "\n";

		sConsoleOut = "Colorspace:                    ";
		printf("\n%s", sConsoleOut.c_str());
		sLogBuffer += sConsoleOut;

		if (AVS_vidinfo.IsYV411())
			sConsoleOut = "  YV411";
		else if (AVS_vidinfo.IsYV24())
			sConsoleOut = "   YV24";
		else if (AVS_vidinfo.IsYV16())
			sConsoleOut = "   YV16";
		else if (AVS_vidinfo.IsY8())
			sConsoleOut = "     Y8";
		else if (AVS_vidinfo.IsYV12())
			sConsoleOut = "   YV12";
		else if (AVS_vidinfo.IsYUY2())
			sConsoleOut = "   YUY2";
		else if (AVS_vidinfo.IsRGB24())
			sConsoleOut = "  RGB24";
		else if (AVS_vidinfo.IsRGB32())
			sConsoleOut = "  RGB32";
		else
			sConsoleOut = "Unknown";

		printf("%s\n", sConsoleOut.c_str());
		sLogBuffer += sConsoleOut + "\n";

		sConsoleOut = "Audio channels:    ";
		sLogBuffer += sConsoleOut;
		if (AVS_vidinfo.AudioChannels())
			sConsoleOut = utils.StrFormat("%19d", AVS_vidinfo.AudioChannels());
		else
			sConsoleOut = "                n/a";

		sLogBuffer += sConsoleOut + "\n";

		sConsoleOut = "Audio bits/sample: ";
		sLogBuffer += sConsoleOut;
		if (AVS_vidinfo.SampleType())
			sConsoleOut = utils.StrFormat("%19d", AVS_vidinfo.SampleType());
		else
			sConsoleOut = "                n/a";

		sLogBuffer += sConsoleOut + "\n";

		sConsoleOut = "Audio sample rate: ";
		sLogBuffer += sConsoleOut;
		if (AVS_vidinfo.SamplesPerSecond())
			sConsoleOut = utils.StrFormat("%19d", AVS_vidinfo.SamplesPerSecond());
		else
			sConsoleOut = "                n/a";

		sLogBuffer += sConsoleOut + "\n";

		sConsoleOut = "Audio samples:     ";
		sLogBuffer += sConsoleOut;
		if (AVS_vidinfo.num_audio_samples)
			sConsoleOut = utils.StrFormat("%19I64d", AVS_vidinfo.num_audio_samples);
		else
			sConsoleOut = "                n/a";

		sLogBuffer += sConsoleOut + "\n";


		if (bInfoOnly)
		{
			int IVersion = AVS_clip->GetVersion();
			AVS_clip = 0;
			AVS_main = 0;
			AVS_temp = 0;

			if (AvisynthInfo.iInterfaceVersion >= 5)
				AVS_env->DeleteScriptEnvironment();
			else
				AVS_env->~IScriptEnvironment();

			if (INISettings.bCreateLog)
			{
				PerformanceData = new perfdata[1];
				string sLogRet = CreateLogFile(sAVSFile, sLogBuffer, PerformanceData, uiPDIndex, sAVSError, uiFrameInterval);
				if (sLogRet != "")
					printcon(COLOR_ERROR, sLogRet.c_str());

				delete[] PerformanceData;
			}

			#ifdef AVS64
				AVS_linkage = 0;
			#endif
	
			::FreeLibrary(hDLL);

			Pause();
			return 0;
		}

		printf("\n");

		unsigned int uiFramesToProcess = 0;
		unsigned int uiFirstFrame = 0;
		unsigned int uiLastFrame = uiFrames - 1;

		if (INISettings.iStopFrame == -1)
			INISettings.iStopFrame = (__int64)uiFrames - 1;

		if ((INISettings.iStartFrame >= 0) && (INISettings.iStartFrame <= INISettings.iStopFrame) && (INISettings.iStopFrame < (__int64)uiFrames))
		{
			uiFirstFrame = (unsigned int)INISettings.iStartFrame;
			uiLastFrame = (unsigned int)INISettings.iStopFrame;
			uiFramesToProcess = uiLastFrame - uiFirstFrame + 1;
			printf("\r%s\r", utils.StrPad("Reading frame(s)...", uiConsoleWidth).c_str());
		}
		else
		{
			printf("\r%s", utils.StrPad("", uiConsoleWidth).c_str());
			printcon(COLOR_EMPHASIS, "\rInvalid frame range specified in INI file:\n", sConsoleOut.c_str());

			sConsoleOut = utils.StrFormat("\"%s\"", INISettings.sFrameRange.c_str());
			printcon(COLOR_ERROR, "\r%s\n", sConsoleOut.c_str());

			sConsoleOut = utils.StrFormat("Proceed with default (%u,%u)? [y/n]", uiFirstFrame, uiLastFrame);
			printcon(COLOR_EMPHASIS, "\r%s", sConsoleOut.c_str());

			int iKey = 0;
			for (;;)
			{
				Sleep(100);
				if (_kbhit())
				{
					printf("\r%s", utils.StrPad("", uiConsoleWidth).c_str());
					utils.CursorUp(1);
					printf("\r%s", utils.StrPad("", uiConsoleWidth).c_str());
					utils.CursorUp(1);
					printf("\r%s", utils.StrPad("", uiConsoleWidth).c_str());

					iKey = _getch();
					if ((iKey == 0x59) || (iKey == 0x79))
					{
						printf("\r%s\r", utils.StrPad("Reading frame(s)...", uiConsoleWidth).c_str());
						break;
					}
					else
					{
						printf("\r%s\r", utils.StrPad("", uiConsoleWidth).c_str());
						int IVersion = AVS_clip->GetVersion();
						AVS_clip = 0;
						AVS_main = 0;
						AVS_temp = 0;
			
						if (IVersion >= 5)
							AVS_env->DeleteScriptEnvironment();
						else
							AVS_env->~IScriptEnvironment();
			
						if (INISettings.bCreateLog)
						{
							PerformanceData = new perfdata[1];
							string sLogRet = CreateLogFile(sAVSFile, sLogBuffer, PerformanceData, uiPDIndex, sAVSError, uiFrameInterval);
							if (sLogRet != "")
								printf(sLogRet.c_str());
			
							delete[] PerformanceData;
						}
			
						#ifdef AVS64
							AVS_linkage = 0;
						#endif
				
						::FreeLibrary(hDLL);
			
						Pause();
						return 0;
					}
				}
			}

			uiFirstFrame = 0;
			uiLastFrame = uiFrames - 1;
			uiFramesToProcess = uiFrames;
		}

		while ((uiFramesToProcess / uiFrameInterval) > 1000000)
		{
			if (uiFrameInterval == 50000)
				uiFrameInterval = 100000;
			if (uiFrameInterval == 20000)
				uiFrameInterval = 50000;
			if (uiFrameInterval == 10000)
				uiFrameInterval = 20000;
			if (uiFrameInterval == 5000)
				uiFrameInterval = 10000;
			if (uiFrameInterval == 2000)
				uiFrameInterval = 5000;
			if (uiFrameInterval == 1000)
				uiFrameInterval = 2000;
			if (uiFrameInterval == 500)
				uiFrameInterval = 1000;
			if (uiFrameInterval == 200)
				uiFrameInterval = 500;
			if (uiFrameInterval == 100)
				uiFrameInterval = 200;
			if (uiFrameInterval == 50)
				uiFrameInterval = 100;
			if (uiFrameInterval == 20)
				uiFrameInterval = 50;
			if (uiFrameInterval == 10)
				uiFrameInterval = 20;
			if (uiFrameInterval == 1)
				uiFrameInterval = 10;
		}

		unsigned int uiArraySize = (uiFramesToProcess / uiFrameInterval) + 2;

		PerformanceData = new perfdata[uiArraySize];
		for (unsigned int x = 0; x < uiArraySize; x++)
		{
			PerformanceData[x].frame_low = 0;
			PerformanceData[x].frame_high = 0;
			PerformanceData[x].fps = 0.0;
			PerformanceData[x].cpu_usage = 0;
			PerformanceData[x].gpu_usage = 0;
			PerformanceData[x].phys_mem = 0;
			PerformanceData[x].virt_mem = 0;
			PerformanceData[x].num_threads = 0;
		}

		bEarlyExit = FALSE;
		unsigned int n = 0;
		BOOL bFirstScr = TRUE;
		__int64 iElapsedMS = 0;
		__int64 iEstimatedMS = 0;

		CProcessInfo process_info;
		DWORD dwPhysicalMemPeakMB = 0;
		DWORD dwVirtualMemPeakMB = 0;
		DWORD dwPhysicalMemCurrentMB = 0;
		DWORD dwVirtualMemCurrentMB = 0;
		unsigned int uiCPUUsageAcc = 0;
		unsigned int uiCPUUsageAvg = 0;
		unsigned int uiGPUUsageAcc = 0;
		unsigned int uiGPUUsageAvg = 0;
		unsigned int uiCounter = 0;

		unsigned int uiCurrentFrame = 0;
		double dFPSAverage = 0.0;
		double dFPSCurrent = 0.0;
		double dFPSMin = 1.0e+20;
		double dFPSMax = 0.0;

		double dStartTime = timer.GetTimer();
		double dCurrentTime = dStartTime;
		double dLastDisplayTime = dStartTime;
		double dLastIntervalTime = dStartTime;

		if (INISettings.bMonitorGPULoad)
		{
			if (!gpuinfo.ReadSensors())
				INISettings.bMonitorGPULoad = FALSE;
		}

		process_info.Update();

		for (uiCurrentFrame = uiFirstFrame; uiCurrentFrame <= uiLastFrame; uiCurrentFrame++)
		{
			PVideoFrame src_frame = AVS_clip->GetFrame(uiCurrentFrame, AVS_env);
			++n;

			if (((n % uiFrameInterval) == 0) || (n == uiFramesToProcess))
			{
				dCurrentTime = timer.GetTimer();
				process_info.Update();
				++uiCounter;

				iElapsedMS = (__int64)(((dCurrentTime - dStartTime) * 1000.0) + 0.5);
				iEstimatedMS = (__int64)((double)uiFramesToProcess * (double)iElapsedMS / (double)n);

				uiCPUUsageAcc += (unsigned int)process_info.wCPUUsage;
				uiCPUUsageAvg = (unsigned int)(((double)uiCPUUsageAcc / (double)uiCounter) + 0.5);

				if (INISettings.bMonitorGPULoad)
				{
					if (gpuinfo.ReadSensors())
					{
						uiGPUUsageAcc += (unsigned int)gpuinfo.sensors.GPULoad;
						uiGPUUsageAvg = (unsigned int)(((double)uiGPUUsageAcc / (double)uiCounter) + 0.5);
					}
					else
					{
						uiGPUUsageAcc = 0;
						uiGPUUsageAvg = 0;
						INISettings.bMonitorGPULoad = FALSE;
					}
				}

				dwPhysicalMemCurrentMB = process_info.dwPhysicalMemMB;
				dwVirtualMemCurrentMB = process_info.dwVirtualMemMB;

				if (dwPhysicalMemCurrentMB > dwPhysicalMemPeakMB)
					dwPhysicalMemPeakMB = dwPhysicalMemCurrentMB;
				if (dwVirtualMemCurrentMB > dwVirtualMemPeakMB)
					dwVirtualMemPeakMB = dwVirtualMemCurrentMB;

				dFPSAverage = (double)n / (dCurrentTime - dStartTime);

				if ((n % uiFrameInterval) == 0)
				{
					if ((dCurrentTime - dLastIntervalTime) > 0.0000001)
						dFPSCurrent = (double)uiFrameInterval / (dCurrentTime - dLastIntervalTime);
					else
						dFPSCurrent = (double)uiFrameInterval / 0.0000001;

					if (dFPSCurrent > dFPSMax)
						dFPSMax = dFPSCurrent;
					if (dFPSCurrent < dFPSMin)
						dFPSMin = dFPSCurrent;

					PerformanceData[uiPDIndex].frame_low = (uiCurrentFrame + 1) - uiFrameInterval;
					PerformanceData[uiPDIndex].frame_high = uiCurrentFrame;
					PerformanceData[uiPDIndex].fps = dFPSCurrent;
					PerformanceData[uiPDIndex].cpu_usage = process_info.wCPUUsage;
					if (INISettings.bMonitorGPULoad)
						PerformanceData[uiPDIndex].gpu_usage = gpuinfo.sensors.GPULoad;
					else
						PerformanceData[uiPDIndex].gpu_usage = 0;
					PerformanceData[uiPDIndex].num_threads = process_info.wThreadCount;
					PerformanceData[uiPDIndex].phys_mem = dwPhysicalMemCurrentMB;
					PerformanceData[uiPDIndex].virt_mem = dwVirtualMemCurrentMB;
					++uiPDIndex;

					dLastIntervalTime = dCurrentTime;
				}

				if ((dCurrentTime - dLastDisplayTime) >= 0.25)
				{
					uiConsoleWidth = utils.GetConsoleWidth();
					dLastDisplayTime = dCurrentTime;

					if (!bFirstScr)
						utils.CursorUp(10);

					bFirstScr = FALSE;
					utils.SetRGB(COLOR_EMPHASIS, INISettings.bUseColor);
					sConsoleOut = utils.StrFormat("Frame (current | last):         %u | %u", uiCurrentFrame, uiLastFrame);
					printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
					sConsoleOut = utils.StrFormat("FPS (cur | min | max | avg):    %s | %s | %s | %s", utils.StrFormatFPS(dFPSCurrent).c_str(), utils.StrFormatFPS(dFPSMin).c_str(), utils.StrFormatFPS(dFPSMax).c_str(), utils.StrFormatFPS(dFPSAverage).c_str());
					printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
					sConsoleOut = utils.StrFormat("CPU usage (current | average):  %u%% | %u%%", process_info.wCPUUsage, uiCPUUsageAvg);
					printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());

					if (INISettings.bMonitorGPULoad)
						sConsoleOut = utils.StrFormat("GPU usage (current | average):  %u%% | %u%%", gpuinfo.sensors.GPULoad, uiGPUUsageAvg);
					else
						sConsoleOut = utils.StrFormat("GPU usage:                      GPU monitor disabled");

					printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());

					sConsoleOut = utils.StrFormat("Thread count:                   %u", process_info.wThreadCount);
					printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
					sConsoleOut = utils.StrFormat("Memory usage (phys | virt):     %u | %u MB", dwPhysicalMemCurrentMB, dwVirtualMemCurrentMB);
					printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
					sConsoleOut = utils.StrFormat("Time (elapsed | estimated):     %s | %s", timer.FormatTimeString(iElapsedMS, FALSE).c_str(), timer.FormatTimeString(iEstimatedMS, FALSE).c_str());
					printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
					utils.ResetRGB(INISettings.bUseColor);

					printf("\nPress \'Esc\' to cancel the process...\n\n");

					if (_kbhit())
					{
						if (_getch() == 0x1B) //ESC
						{
							uiLastFrame = uiCurrentFrame;
							break;
						}
					}
				}
			}
		}

		process_info.CloseProcess();

		AVS_clip = 0;
		AVS_main = 0;
		AVS_temp = 0;

		if (AvisynthInfo.iInterfaceVersion >= 5)
			AVS_env->DeleteScriptEnvironment();
		else
			AVS_env->~IScriptEnvironment();

		sLogBuffer += "\n\n[Runtime info]\n";

		if (n >= uiFrameInterval)
		{
			if (!bFirstScr)
				utils.CursorUp(10);

			if (uiFirstFrame == uiLastFrame)
			{
				if (uiFirstFrame == 0)
					sConsoleOut = utils.StrFormat("Frames processed:               %u", n, uiFirstFrame);
				else
					sConsoleOut = utils.StrFormat("Frames processed:               %u (%u)", n, uiFirstFrame);
			}
			else
				sConsoleOut = utils.StrFormat("Frames processed:               %u (%u - %u)", n, uiFirstFrame, uiLastFrame);

			utils.SetRGB(COLOR_EMPHASIS, INISettings.bUseColor);
			printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
			sLogBuffer += sConsoleOut + "\n";

			sConsoleOut = utils.StrFormat("FPS (min | max | average):      %s | %s | %s", utils.StrFormatFPS(dFPSMin).c_str(), utils.StrFormatFPS(dFPSMax).c_str(), utils.StrFormatFPS(dFPSAverage).c_str());
			printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
			sLogBuffer += sConsoleOut + "\n";

			sConsoleOut = utils.StrFormat("CPU usage (average):            %u%%", uiCPUUsageAvg);
			printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
			sLogBuffer += sConsoleOut + "\n";

			if (INISettings.bMonitorGPULoad)
				sConsoleOut = utils.StrFormat("GPU usage (average):            %u%%", uiGPUUsageAvg);
			else
				sConsoleOut = utils.StrFormat("GPU usage:                      GPU monitor disabled");

			printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
			sLogBuffer += sConsoleOut + "\n";

			sConsoleOut = utils.StrFormat("Thread count:                   %u", process_info.wThreadCount);
			printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
			sLogBuffer += sConsoleOut + "\n";

			sConsoleOut = utils.StrFormat("Memory usage (phys | virt):     %u | %u MB", dwPhysicalMemPeakMB, dwVirtualMemPeakMB);
			printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
			sLogBuffer += sConsoleOut + "\n";

			if ((INISettings.bLogEstimatedTime) && (n < uiFramesToProcess))
				sConsoleOut = utils.StrFormat("Time (elapsed | estimated):     %s | %s", timer.FormatTimeString(iElapsedMS, FALSE).c_str(), timer.FormatTimeString(iEstimatedMS, FALSE).c_str());
			else
				sConsoleOut = utils.StrFormat("Time (elapsed):                 %s", timer.FormatTimeString(iElapsedMS, FALSE).c_str());

			printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
			sLogBuffer += sConsoleOut + "\n";

			utils.ResetRGB(INISettings.bUseColor);

			printf("\r%s\n", utils.StrPad("", uiConsoleWidth).c_str());
			printf("\r%s\n", utils.StrPad("", uiConsoleWidth).c_str());
			utils.CursorUp(2);
		}
		else
		{
			if (!bFirstScr)
			{
				for (int i = 0; i <= 9; i++)
				{
					printf("\r%s", utils.StrPad("", uiConsoleWidth).c_str());
					utils.CursorUp(1);
				}
			}

			sConsoleOut = "Insufficient data for measurements";
			printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
			sLogBuffer += sConsoleOut;
		}
	}
	catch (AvisynthError err)
	{
		INISettings.bCreateLog = TRUE;
		sAVSError = utils.StrFormat("%s", (PCSTR)err.msg);
		iRet = -1;

		
		if (bEarlyExit == FALSE)
		{
			utils.CursorUp(2);
			printf("\r%s\n", utils.StrPad("", uiConsoleWidth).c_str());
			printf("\r%s\n", utils.StrPad("", uiConsoleWidth).c_str());
			utils.CursorUp(2);
			printcon(COLOR_ERROR, "\r%s\n", sAVSError.c_str());
		}
		else
			printcon(COLOR_ERROR, "\n%s\n", sAVSError.c_str());
	}
	catch (...)
	{
		INISettings.bCreateLog = TRUE;
		iRet = -1;
		if (bEarlyExit == FALSE)
		{
			utils.CursorUp(2);
			printf("\r%s\n", utils.StrPad("", uiConsoleWidth).c_str());
			printf("\r%s\n", utils.StrPad("", uiConsoleWidth).c_str());
			utils.CursorUp(2);
		}
		printcon(COLOR_ERROR, "\nUnknown exception\n");
	}

	#ifdef AVS64
		AVS_linkage = 0;
	#endif

	::FreeLibrary(hDLL);

	if (INISettings.bCreateLog)
	{
		string sRet = CreateLogFile(sAVSFile, sLogBuffer, PerformanceData, uiPDIndex, sAVSError, uiFrameInterval);
		if (sRet != "")
		{
			printcon(COLOR_ERROR, sRet.c_str());
			Pause();
			return -1;
		}
	}

	delete[] PerformanceData;

	Pause();

	return iRet;
}


string ParseINIFile()
{
	string sRet = "";
	string sProgramPath = "";
	string sINIFile = "";
	string sTemp = "";

	char szPath[2050];
	if (::GetModuleFileName(NULL, szPath, 2048) > 0)
	{
		sProgramPath = utils.StrFormat("%s", szPath);
		size_t i = 0;
		for (i = (sProgramPath.length() - 1); i > 0; i--)
		{
		 if (sProgramPath[i] == '\\')
			break;
		}
		sProgramPath = sProgramPath.substr(0, i);
		sINIFile = sProgramPath + "\\AVSMeter.ini";
	}

	INISettings.bCreateLog = FALSE;
	INISettings.bPauseBeforeExit = FALSE;
	INISettings.iStartFrame = 0;
	INISettings.iStopFrame = -1;
	INISettings.bInvokeDistributor = TRUE;
	INISettings.bAllowOnlyOneInstance = TRUE;
	INISettings.bLogEstimatedTime = FALSE;
	INISettings.bUseColor = TRUE;
	INISettings.bMonitorGPULoad = FALSE;
	INISettings.wReadTimeout = 120;

	if (!utils.FileExists(sINIFile)) //write defaults
	{
		ofstream hINIFile(sINIFile.c_str());
		if (!hINIFile.is_open())
	  {
			sRet = utils.StrFormat("\nCannot create \"%s\"\n", sINIFile.c_str());
			return sRet;
		}
		hINIFile << "CreateLog=0\n";
		hINIFile << "PauseBeforeExit=0\n";
		hINIFile << "FrameRange=0,-1\n";
		hINIFile << "InvokeDistributor=1\n";
		hINIFile << "AllowOnlyOneInstance=1\n";
		hINIFile << "LogEstimatedTime=0\n";
		hINIFile << "UseColor=1\n";
		hINIFile << "MonitorGPULoad=0\n";
		hINIFile << "ReadTimeout=120\n";
		hINIFile.flush();
		hINIFile.close();

		return "";
	}

	ifstream hINIFile(sINIFile.c_str());
	if (!hINIFile.is_open())
	{
		sRet = utils.StrFormat("\nCannot open \"%s\"\n", sINIFile.c_str());
		return sRet;
	}

	string sOrgLine = "";
	string sCurrentLine = "";
	size_t spos = 0;

	while (getline(hINIFile, sCurrentLine))
	{
		sOrgLine = sCurrentLine;
		utils.StrToLC(sCurrentLine);
		utils.StrTrim(sCurrentLine);
		sCurrentLine.erase(remove(sCurrentLine.begin(), sCurrentLine.end(), ' '), sCurrentLine.end());

		if (sCurrentLine.substr(0, 9) == "createlog")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "1")
				INISettings.bCreateLog = TRUE;
		}

		if (sCurrentLine.substr(0, 15) == "pausebeforeexit")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "1")
				INISettings.bPauseBeforeExit = TRUE;
		}

		if (sCurrentLine.substr(0, 17) == "invokedistributor")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "0")
				INISettings.bInvokeDistributor = FALSE;
		}

		if (sCurrentLine.substr(0, 20) == "allowonlyoneinstance")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "0")
				INISettings.bAllowOnlyOneInstance = FALSE;
		}

		if (sCurrentLine.substr(0, 16) == "logestimatedtime")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "1")
				INISettings.bLogEstimatedTime = TRUE;
		}

		if (sCurrentLine.substr(0, 14) == "monitorgpuload")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "1")
				INISettings.bMonitorGPULoad = TRUE;
		}

		if (sCurrentLine.substr(0, 8) == "usecolor")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "0")
				INISettings.bUseColor = FALSE;
		}

		if (sCurrentLine.substr(0, 12) == "readtimeout=")
		{
			sTemp = sCurrentLine.substr(12);
			if (utils.IsNumeric(sTemp))
			{
				INISettings.wReadTimeout = (WORD)atol(sTemp.c_str());
				if ((INISettings.wReadTimeout < 10) || (INISettings.wReadTimeout > 3600))
					INISettings.wReadTimeout = 120;
			}
		}

		if (sCurrentLine.substr(0, 11) == "framerange=")
		{
			INISettings.sFrameRange = sOrgLine;
			INISettings.iStartFrame = 0xFFFFFFFFFF;
			INISettings.iStopFrame = 0xFFFFFFFFFF;

			sTemp = sCurrentLine.substr(11);
			if (sTemp.length() < 3)
				continue;

			spos = sTemp.find(",");
			if ((spos < 1) || (spos > 27))
				continue;

			if ((utils.IsNumeric(sTemp.substr(0, spos))) && (utils.IsNumeric(sTemp.substr(spos + 1))))
			{
				INISettings.iStartFrame = _atoi64(sTemp.substr(0, spos).c_str());
				INISettings.iStopFrame = _atoi64(sTemp.substr(spos + 1).c_str());
			}
		}
	}

	hINIFile.close();

	return sRet;
}


string CreateLogFile(string &s_avsfile, string &s_logbuffer, perfdata * cs_pdata, unsigned int ui_index, string &s_avserror, unsigned int ui_frameinterval)
{
	string sRet = "";
	string sAVSBuffer = "";
	string sCurrentLine = "";

	ifstream hAVSFile(s_avsfile.c_str());
	if (!hAVSFile.is_open())
	{
		sRet = utils.StrFormat("\nCannot open \"%s\"\n", s_avsfile.c_str());
		return sRet;
	}

	while (getline(hAVSFile, sCurrentLine))
		sAVSBuffer += sCurrentLine + "\n";

	hAVSFile.close();

	string sLogFile = "";
	size_t ilen = s_avsfile.length();

	if (ilen > 4)
		sLogFile = s_avsfile.substr(0, ilen - 4) + ".log";
	else
		sLogFile = s_avsfile + ".log";

	ofstream hLogFile(sLogFile.c_str());
	if (!hLogFile.is_open())
	{
		sRet = utils.StrFormat("\nCannot create \"%s\"\n", sLogFile.c_str());
		return sRet;
	}

	string sLog = "";

	sLog = s_logbuffer + "\n";
	hLogFile << sLog;
	sLog = "\n[Script]\n" + sAVSBuffer + "\n";
	hLogFile << sLog;

	if (ui_index > 0)
	{
		if (ui_frameinterval == 1)
			sLog = "\n[Performance data]\n       Frame    Frames/sec   Time/frame(ms)   CPU(%)   GPU(%)   Threads   PhysMEM(MB)   VirtMEM(MB)\n";
		else
			sLog = "\n[Performance data]\n       Frame interval    Frames/sec   Time/frame(ms)   CPU(%)   GPU(%)   Threads   PhysMEM(MB)   VirtMEM(MB)\n";

		hLogFile << sLog;

		string stemp1 = "";
		string stemp2 = "";
		if (ui_index > 0)
		{
			for (unsigned int i = 0; i < ui_index; i++)
			{
				if (ui_frameinterval == 1)
				{
					stemp1 = utils.StrFormat("%u", cs_pdata[i].frame_low);
					string spad((12 - stemp1.length()), ' ');
					stemp2 = utils.StrFormat("%s%u %13.3f %16.4f %8u %8u %9u %13u %13u", spad.c_str(), cs_pdata[i].frame_low, cs_pdata[i].fps, 1000.0 / cs_pdata[i].fps, cs_pdata[i].cpu_usage, cs_pdata[i].gpu_usage, cs_pdata[i].num_threads, cs_pdata[i].phys_mem, cs_pdata[i].virt_mem);
				}
				else
				{
					stemp1 = utils.StrFormat("%u-%u", cs_pdata[i].frame_low, cs_pdata[i].frame_high);
					string spad((21 - stemp1.length()), ' ');
					stemp2 = utils.StrFormat("%s%u-%u %13.3f %16.4f %8u %8u %9u %13u %13u", spad.c_str(), cs_pdata[i].frame_low, cs_pdata[i].frame_high, cs_pdata[i].fps, 1000.0 / cs_pdata[i].fps, cs_pdata[i].cpu_usage, cs_pdata[i].gpu_usage, cs_pdata[i].num_threads, cs_pdata[i].phys_mem, cs_pdata[i].virt_mem);
				}

				hLogFile << stemp2 + "\n";
			}
		}
	}

	if (s_avserror != "")
	{
		hLogFile << "\n\n[AviSynth error]\n" + s_avserror + "\n";
	}

	hLogFile.flush();
	hLogFile.close();

	return sRet;
}


void Pause()
{
	if (INISettings.bPauseBeforeExit)
	{
		printf("\nPress any key to exit...");
		for (;;)
		{
			Sleep(100);
			if (_kbhit())
				break;
		}
		printf("\r%s", utils.StrPad("", utils.GetConsoleWidth()).c_str());
	}

	return;
}


void CalculateFrameInterval(PVOID pParam)
{
	ThreadParams* p = (ThreadParams*) pParam;
	p->uiFrameInterval = 0;
	p->sAVSError = "";

	if (!timer.TestTimer())
	{
		p->sAVSError = "Multimedia Timers and/or High Performance Timers are not supported";
		p->bFinished = TRUE;
		_endthread();
	}

	HINSTANCE hDLL = ::LoadLibrary("avisynth");
	if (!hDLL)
	{
		p->sAVSError = "Failed to load avisynth.dll";
		p->bFinished = TRUE;
		_endthread();
	}

	try
	{
		IScriptEnvironment *(__stdcall *CreateEnvironment)(int) = (IScriptEnvironment *(__stdcall *)(int))::GetProcAddress(hDLL, "CreateScriptEnvironment");
		if (!CreateEnvironment)
		{
			p->sAVSError = "Failed to load CreateScriptEnvironment()";
			p->bFinished = TRUE;
			_endthread();
			::FreeLibrary(hDLL);
			_endthread();
		}

		IScriptEnvironment *AVS_env = CreateEnvironment(AVISYNTH_INTERFACE_VERSION);
		if (!AVS_env)
		{
			p->sAVSError = "Could not create IScriptenvironment";
			p->bFinished = TRUE;
			::FreeLibrary(hDLL);
			_endthread();
		}

		#ifdef AVS64
			AVS_linkage = AVS_env->GetAVSLinkage();
		#endif

		AVSValue AVS_main;
		AVS_main = AVS_env->Invoke("Import", p->sAVSFile.c_str());

		if (!AVS_main.IsClip()) //not a clip
			AVS_env->ThrowError("Script did not return a video clip:\n(%s)", p->sAVSFile.c_str());

		AVSValue AVS_temp;
		PClip AVS_clip;
		VideoInfo	AVS_vidinfo;

		int iMTMode = 0;
		try
		{
			AVS_temp = AVS_env->Invoke("GetMTMode", false);
			iMTMode = AVS_temp.IsInt() ? AVS_temp.AsInt() : 0;
			if ((iMTMode > 0) && (iMTMode < 5) && INISettings.bInvokeDistributor)
				AVS_main = AVS_env->Invoke("Distributor", AVS_main);
		}
		catch (IScriptEnvironment::NotFound)
		{
		}

		AVS_clip = AVS_main.AsClip();
		AVS_vidinfo = AVS_clip->GetVideoInfo();
		unsigned int uiTotalFrames = (unsigned int)AVS_vidinfo.num_frames;

		if (uiTotalFrames < 1)
			AVS_env->ThrowError("Script did not return a video clip:\n(%s)", p->sAVSFile.c_str());

		unsigned int uif = 0;
		p->uiCurFrame = 0;
		double TDelta = 0.0;

		while (timer.GetSTDTimer() > 4294937.0)
			Sleep(1000);

		double T0 = timer.GetSTDTimer();
		for (;;)
		{
			PVideoFrame src_frame = AVS_clip->GetFrame(uif, AVS_env);
			++uif;
			p->uiCurFrame = uif;

			TDelta = timer.GetSTDTimer() - T0;
			if ((TDelta >= 5.0) || (uif == uiTotalFrames))
				break;
		}

		p->uiFrameInterval = 50000;
		double dAverageFrameTime = 0.0;
		if (TDelta > 0.2)
			dAverageFrameTime = TDelta / (double)uif;

		if (dAverageFrameTime > 0.000001)
			p->uiFrameInterval = 20000;
		if (dAverageFrameTime > 0.000002)
			p->uiFrameInterval = 10000;
		if (dAverageFrameTime > 0.000004)
			p->uiFrameInterval = 5000;
		if (dAverageFrameTime > 0.000010)
			p->uiFrameInterval = 2000;
		if (dAverageFrameTime > 0.000020)
			p->uiFrameInterval = 1000;
		if (dAverageFrameTime > 0.000040)
			p->uiFrameInterval = 500;
		if (dAverageFrameTime > 0.000100)
			p->uiFrameInterval = 200;
		if (dAverageFrameTime > 0.000200)
			p->uiFrameInterval = 100;
		if (dAverageFrameTime > 0.000400)
			p->uiFrameInterval = 50;
		if (dAverageFrameTime > 0.001000)
			p->uiFrameInterval = 20;
		if (dAverageFrameTime > 0.002000)
			p->uiFrameInterval = 10;
		if (dAverageFrameTime > 0.020000)
			p->uiFrameInterval = 1;

		AVS_clip = 0;
		AVS_main = 0;
		AVS_temp = 0;

		if (AvisynthInfo.iInterfaceVersion >= 5)
			AVS_env->DeleteScriptEnvironment();
		else
			AVS_env->~IScriptEnvironment();
	}
	catch (AvisynthError err)
	{
		p->sAVSError = utils.StrFormat("%s", (PCSTR)err.msg);
		p->uiFrameInterval = 0;
	}
	catch (...)
	{
		p->sAVSError = "Unknown exception";
		p->uiFrameInterval = 0;
	}

	#ifdef AVS64
		AVS_linkage = 0;
	#endif

	::FreeLibrary(hDLL);

	p->bFinished = TRUE;

	_endthread();
}


BOOL GetAvisynthInfo()
{
	AvisynthInfo.sDLLPath = "";
	AvisynthInfo.bAVSLinkage = FALSE;
	AvisynthInfo.bIsCompatible = TRUE;
	AvisynthInfo.bIsMTVersion = FALSE;
	AvisynthInfo.iInterfaceVersion = 0;
	AvisynthInfo.bLoadError = FALSE;
	AvisynthInfo.sErrorMsg = "";
	AvisynthInfo.sVersion = "";
	AvisynthInfo.sVersionString = "Unknown Avisynth Version";

	try
	{
		LOADED_IMAGE li;
		BOOL bLoaded = MapAndLoad("avisynth", NULL, &li,	TRUE,	TRUE);
		if (!bLoaded)
		{
			AvisynthInfo.bLoadError = TRUE;
			return FALSE;
		}

		DWORD expVA = li.FileHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
		PIMAGE_EXPORT_DIRECTORY pExp = (PIMAGE_EXPORT_DIRECTORY)ImageRvaToVa(li.FileHeader, li.MappedAddress, expVA, NULL);
		DWORD rvaNames = pExp->AddressOfNames;
		DWORD *prvaNames = (DWORD*)ImageRvaToVa(li.FileHeader, li.MappedAddress, rvaNames, NULL);
		string sName = "";
		int iPos = 0;

		for (DWORD dwName = 0; dwName < pExp->NumberOfNames; ++dwName)
		{
			DWORD rvaName = prvaNames[dwName];
			sName = (char *)ImageRvaToVa(li.FileHeader, li.MappedAddress, rvaName, NULL);
			utils.StrToLC(sName);

			iPos = (int)sName.find("avs_linkage");
			if (iPos >= 0)
				AvisynthInfo.bAVSLinkage = TRUE;
		}

		UnMapAndLoad(&li);
	}
	catch (...)
	{
		AvisynthInfo.bIsCompatible = FALSE;
	}

	if (!AvisynthInfo.bIsCompatible)
		return FALSE;


	HINSTANCE hDLL = ::LoadLibrary("avisynth");
	if (!hDLL)
	{
		AvisynthInfo.bLoadError = TRUE;
		return FALSE;
	}


	char szPath[2050];
	if (::GetModuleFileName(hDLL, szPath, 2048) > 0)
	{
		AvisynthInfo.sDLLPath = utils.StrFormat("%s", szPath);
		
		if (utils.GetResourceInfo(AvisynthInfo.sDLLPath))
			AvisynthInfo.sVersion = utils.resinfo.sFixedFileVersion;
	}

	try
	{
		IScriptEnvironment *(__stdcall *CreateEnvironment)(int) = (IScriptEnvironment *(__stdcall *)(int))::GetProcAddress(hDLL, "CreateScriptEnvironment");
		if (!CreateEnvironment)
		{
			AvisynthInfo.sErrorMsg = "Failed to load CreateScriptEnvironment()";
			::FreeLibrary(hDLL);
			return FALSE;
		}

		IScriptEnvironment *AVS_env = CreateEnvironment(AVISYNTH_INTERFACE_VERSION);
		if (!AVS_env)
		{
			AvisynthInfo.sErrorMsg = "Could not create IScriptenvironment";
			::FreeLibrary(hDLL);
			return FALSE;
		}

		#ifdef AVS64
			AVS_linkage = AVS_env->GetAVSLinkage();
		#endif

		AVSValue AVS_main;
		AVS_main = AVS_env->Invoke("ColorBars", AVSValue(&AVS_main, 0));

		AVSValue AVS_temp;
		PClip AVS_clip;
		try
		{
			AVS_temp = AVS_env->Invoke("VersionString", AVSValue(&AVS_temp, 0));
			AvisynthInfo.sVersionString = utils.StrFormat("%s", AVS_temp.AsString());
		}
		catch (IScriptEnvironment::NotFound)
		{
		}

		if ((AVS_env->FunctionExists("GetMTMode")) || (AVS_env->FunctionExists("Prefetch")))
			AvisynthInfo.bIsMTVersion = TRUE;

		AVS_clip = AVS_main.AsClip();
		AvisynthInfo.iInterfaceVersion = AVS_clip->GetVersion();
		AVS_clip = 0;
		AVS_main = 0;
		AVS_temp = 0;

		if (AvisynthInfo.iInterfaceVersion >= 5)
			AVS_env->DeleteScriptEnvironment();
		else
			AVS_env->~IScriptEnvironment();
	}
	catch (AvisynthError err)
	{
		AvisynthInfo.sErrorMsg = utils.StrFormat("%s", (PCSTR)err.msg);
	}
	catch (...)
	{
		AvisynthInfo.sErrorMsg = "Unknown exception";
	}

	#ifdef AVS64
		AVS_linkage = 0;
	#endif

	::FreeLibrary(hDLL);

	return TRUE;
}


void PrintUsage()
{
	printf("\nUsage:  AVSMeter script.avs [switches]\n\n");
	printf("Switches:\n");
	printf("-info [-i]   Display clip info only\n");
	printf("-log  [-l]   Create log file\n");
	printf("-gpu         Monitor GPU usage\n");

	return;
}


void printcon(WORD wAttributes, const char *fmt, ...)
{
	if (!fmt)
		return;

	#define MAX_LEN 2048
	va_list args;
	va_start(args, fmt);
	unsigned int len = 256;
	char *buffer = NULL;
	int result = -1;

	while (result == -1)
	{
		if (buffer)
			delete [] buffer;

		buffer = new char [len];
		memset(buffer, ' ', sizeof(buffer));
		result = _vsnprintf(buffer, len, fmt, args);
		len *= 2;
		if (len > MAX_LEN)
		{
			delete [] buffer;
			va_end(args);
			return;
		}
	}

	string str(buffer);
	delete [] buffer;
	va_end(args);

	if (wAttributes)
	{
		utils.SetRGB(wAttributes, INISettings.bUseColor);
		printf(str.c_str());
		utils.ResetRGB(INISettings.bUseColor);
	}
	else
		printf(str.c_str());

	return;
}


