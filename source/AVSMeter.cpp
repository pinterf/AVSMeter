// AVSMeter by Groucho2004
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#define AVSM_VERSION "1.7.6"

#include <windows.h>
#include <conio.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include "utility.h"
#include "ProcessInfo.h"

using namespace std;

#if defined(AVS26_32) || defined(AVS26_64)
	#include "avisynth26.h"
	const AVS_Linkage *AVS_linkage = 0;
#else
	#include "avisynth25.h"
#endif

struct sINISettings
{
	BOOL bCreateLog;
	BOOL bPauseBeforeExit;
	int  iStartFrame;
	int  iStopFrame;
	BOOL bInvokeDistributor;
	BOOL bAllowOnlyOneInstance;
	BOOL bLogEstimatedTime;
} settings;

typedef struct sPerformanceData
{
	unsigned int frame_low;
	unsigned int frame_high;
	double frame_interval_time;
	WORD  cpu_usage;
	DWORD phys_mem;
	DWORD virt_mem;
	WORD num_threads;
} perfdata;

CUtils utils;

unsigned int CalculateFrameInterval(string &s_avsfile, string &s_avserror);
string CreateLogFile(string &s_avsfile, string &s_logbuffer, perfdata * cs_pdata, unsigned int ui_index, string &s_avserror, unsigned int ui_frameinterval);
string ParseINIFile();
void PrintUsage();
void Pause();


int main(int argc, char* argv[])
{
	string sAVSMVersion = AVSM_VERSION;

	#ifdef AVS26_32
		sAVSMVersion += " (AVS 2.6, x86)";
	#elif AVS26_64
		sAVSMVersion += " (AVS 2.6, x64)";
	#else
		sAVSMVersion += " (AVS 2.5, x86)";
	#endif

	fprintf(stderr, "\nAVSMeter %s by Groucho2004\n", sAVSMVersion.c_str());

	string sConsoleOut = "";
	unsigned int uiConsoleWidth = utils.GetConsoleWidth();
	string sArg = "";
	string sArgTest = "";
	string sAVSFile = "";
	string sLogBuffer = "";
	perfdata * PerformanceData;
	unsigned int uiPDIndex = 0;
	BOOL bPerfDataInit = FALSE;
	BOOL bEarlyExit = TRUE;
	BOOL bInfoOnly = FALSE;
	string sAVSError = "";
	unsigned int uiFrameInterval = 0;

	string sRet = ParseINIFile();
	if (sRet != "")
	{
		fprintf(stderr, sRet.c_str());
		Pause();
		return -1;
	}

	if (settings.bAllowOnlyOneInstance)
	{
		CreateMutex(NULL, TRUE, "__avsmeter__single__instance__lock__");
		if (GetLastError() == ERROR_ALREADY_EXISTS)
		{
			fprintf(stderr, "\nOne instance of AVSMeter is already running\n");
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
						fprintf(stderr, "%s", sConsoleOut.c_str());
						Pause();
						return -1;
					}
				}
			}
			continue;
		}

		else if (sArgTest.length() == 2)
		{
			if (sArgTest == "-l")
			{
				settings.bCreateLog = TRUE;
				continue;
			}
			else if (sArgTest == "-i")
			{
				bInfoOnly = TRUE;
				continue;
			}
			else
			{
				sConsoleOut = utils.StrFormat("\nInvalid argument: \"%s\"\n", sArg.c_str());
				fprintf(stderr, "%s", sConsoleOut.c_str());
				Pause();
				return -1;
			}
		}

		else
		{
			sConsoleOut = utils.StrFormat("\nInvalid argument: \"%s\"\n", sArg.c_str());
			fprintf(stderr, "%s", sConsoleOut.c_str());
			Pause();
			return -1;
		}
	}

	if (sAVSFile == "")
	{
		fprintf(stderr, "\nNo script file specified\n");
		Pause();
		return -1;
	}


	if (!utils.TestTimer())
	{
		fprintf(stderr, "\nQueryPerformanceCounter() is not supported\n");
		Pause();
		return -1;
	}

	BYTE bRet = utils.CheckAvisynth();

	if (bRet == CANNOT_LOAD_AVSDLL)
	{
		fprintf(stderr, "\nCannot load avisynth.dll.\n");
		Pause();
		return -1;
	}

	if (bRet == AVSDLL_INCOMPATIBLE)
	{
		#ifdef AVS26_64
			fprintf(stderr, "\nCannot load avisynth.dll.\nMake sure you're using a 64 Bit version.\n");
		#else
			fprintf(stderr, "\nCannot load avisynth.dll.\nMake sure you're using a 32 Bit version.\n");
		#endif

		Pause();
		return -1;
	}

	if (bRet == NO_AVS_LINKAGE)
	{
		#if defined(AVS26_32) || defined(AVS26_64)
			fprintf(stderr, "\nAvisynth 2.6 (Alpha 4 and above) is required.\n");
			Pause();
			return -1;
		#endif
	}


	if (!bInfoOnly)
	{
		sAVSError = "";
		fprintf(stderr, "\r%s\r", utils.StrPad("Analysing script, please wait...", uiConsoleWidth).c_str());
		uiFrameInterval = CalculateFrameInterval(sAVSFile, sAVSError);
		fprintf(stderr, "\r%s", utils.StrPad("", uiConsoleWidth).c_str());
		if (uiFrameInterval == 0)
		{
			fprintf(stderr, "\n%s\n", sAVSError.c_str());
			Pause();
			return -1;
		}
	}

	HINSTANCE hDLL = ::LoadLibrary("avisynth");
	if (!hDLL)
	{
		fprintf(stderr, "\nCannot load avisynth.dll\n");
		Pause();
		return -1;
	}

	try
	{
		IScriptEnvironment *(__stdcall *CreateEnvironment)(int) = (IScriptEnvironment *(__stdcall *)(int))::GetProcAddress(hDLL, "CreateScriptEnvironment");
		if (!CreateEnvironment)
		{
			fprintf(stderr, "\nFailed to load CreateScriptEnvironment()\n");
			Pause();
			return -1;
		}

		IScriptEnvironment *AVS_env = CreateEnvironment(AVISYNTH_INTERFACE_VERSION);
		if (!AVS_env)
		{
			fprintf(stderr, "\nCould not create IScriptenvironment\n");

			Pause();
			return -1;
		}

		#if defined(AVS26_32) || defined(AVS26_64)
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
			if ((iMTMode > 0) && (iMTMode < 5) && settings.bInvokeDistributor)
				AVS_main = AVS_env->Invoke("Distributor", AVS_main);
		}
		catch (IScriptEnvironment::NotFound)
		{
			bIsMTVersion = FALSE;
		}

		AVS_clip = AVS_main.AsClip();
		AVS_vidinfo = AVS_clip->GetVideoInfo();
		unsigned int uiFrames = (unsigned int)AVS_vidinfo.num_frames;
		unsigned int uiMilliSeconds = (unsigned int)((((double)uiFrames * (double)AVS_vidinfo.fps_denominator * 1000.0) / (double)AVS_vidinfo.fps_numerator) + 0.5);

		string sAvisynthVersion = "";
		try
		{
			AVS_temp = AVS_env->Invoke("VersionString", AVSValue(&AVS_temp, 0));
			sAvisynthVersion = utils.StrFormat("%s", AVS_temp.AsString());
		}
		catch (IScriptEnvironment::NotFound)
		{
		}

		sConsoleOut = utils.StrPad(sAvisynthVersion, uiConsoleWidth).c_str();
		fprintf(stderr, "\r%s\n", sConsoleOut.c_str());
		sLogBuffer += "[General info]\n";
		sConsoleOut = utils.StrFormat("Log file created with:   AVSMeter %s", sAVSMVersion.c_str());
		sLogBuffer += sConsoleOut + "\n";
		sConsoleOut = utils.StrFormat("Avisynth version:        %s", sAvisynthVersion.c_str());
		sLogBuffer += sConsoleOut + "\n";

		fprintf(stderr, "\r%s", utils.StrPad("", uiConsoleWidth).c_str());

		if (bIsMTVersion)
		{
			sConsoleOut = utils.StrFormat("Active MT Mode: %d", iMTMode);
			fprintf(stderr, "\r%s\n", sConsoleOut.c_str());
			sLogBuffer += "                         " + sConsoleOut + "\n";
		}

		sLogBuffer += "\n\n[Clip info]\n";

		sConsoleOut = utils.StrFormat("Number of frames:          %11u", AVS_vidinfo.num_frames);
		fprintf(stderr, "\n%s", sConsoleOut.c_str());
		sLogBuffer += sConsoleOut + "\n";

		sConsoleOut = utils.StrFormat("Length (hhh:mm:ss.ms):   %s", utils.StrFormatTime(uiMilliSeconds).c_str());
		fprintf(stderr, "\n%s", sConsoleOut.c_str());
		sLogBuffer += sConsoleOut + "\n";

		sConsoleOut = utils.StrFormat("Frame width:               %11u", AVS_vidinfo.width);
		fprintf(stderr, "\n%s", sConsoleOut.c_str());
		sLogBuffer += sConsoleOut + "\n";

		sConsoleOut = utils.StrFormat("Frame height:              %11u", AVS_vidinfo.height);
		fprintf(stderr, "\n%s", sConsoleOut.c_str());
		sLogBuffer += sConsoleOut + "\n";

		sConsoleOut = utils.StrFormat("Framerate:                 %11.3f (%u/%u)", (double)AVS_vidinfo.fps_numerator / (double)AVS_vidinfo.fps_denominator, AVS_vidinfo.fps_numerator, AVS_vidinfo.fps_denominator);
		fprintf(stderr, "\n%s", sConsoleOut.c_str());
		sLogBuffer += sConsoleOut + "\n";

		if (!AVS_vidinfo.IsFieldBased())
			sConsoleOut = "Interlaced:                         No";
		else
		{
			if (AVS_vidinfo.IsTFF())
				sConsoleOut = "Interlaced:                        Yes (TFF)";
			else
				sConsoleOut = "Interlaced:                        Yes (BFF)";
		}

		fprintf(stderr, "\n%s", sConsoleOut.c_str());
		sLogBuffer += sConsoleOut + "\n";

		sConsoleOut = "Colorspace:                      ";
		fprintf(stderr, "\n%s", sConsoleOut.c_str());
		sLogBuffer += sConsoleOut;

		if (AVS_vidinfo.IsYV411())
			sConsoleOut = "YV411";
		else if (AVS_vidinfo.IsYV24())
			sConsoleOut = " YV24";
		else if (AVS_vidinfo.IsYV16())
			sConsoleOut = " YV16";
		else if (AVS_vidinfo.IsY8())
			sConsoleOut = "   Y8";
		else if (AVS_vidinfo.IsYV12())
			sConsoleOut = " YV12";
		else if (AVS_vidinfo.IsYUY2())
			sConsoleOut = " YUY2";
		else if (AVS_vidinfo.IsRGB24())
			sConsoleOut = "RGB24";
		else if (AVS_vidinfo.IsRGB32())
			sConsoleOut = "RGB32";
		else
			sConsoleOut = "  n/a";

		fprintf(stderr, "%s\n", sConsoleOut.c_str());
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

		sConsoleOut = "Audio samples/sec: ";
		sLogBuffer += sConsoleOut;
		if (AVS_vidinfo.SamplesPerSecond())
			sConsoleOut = utils.StrFormat("%19d", AVS_vidinfo.SamplesPerSecond());
		else
			sConsoleOut = "                n/a";

		sLogBuffer += sConsoleOut + "\n";

		sConsoleOut = "Audio samples:     ";
		sLogBuffer += sConsoleOut;
		if (AVS_vidinfo.num_audio_samples)
			sConsoleOut = utils.StrFormat("%19d", AVS_vidinfo.num_audio_samples);
		else
			sConsoleOut = "                n/a";

		sLogBuffer += sConsoleOut + "\n";


		if (bInfoOnly)
		{
			AVS_clip = 0;
			AVS_main = 0;
			AVS_temp = 0;
			#if defined(AVS26_32) || defined(AVS26_64)
				AVS_env->DeleteScriptEnvironment();
			#else
				AVS_env->~IScriptEnvironment();
			#endif

			if (settings.bCreateLog)
			{
				PerformanceData = new perfdata[1];
				string sLogRet = CreateLogFile(sAVSFile, sLogBuffer, PerformanceData, uiPDIndex, sAVSError, uiFrameInterval);
				if (sLogRet != "")
					fprintf(stderr, sLogRet.c_str());
			}

			Pause();
			return 0;
		}

		fprintf(stderr, "\n");

		unsigned int uiFramesToProcess = 0;
		unsigned int uiFirstFrame = 0;
		unsigned int uiLastFrame = uiFrames - 1;

		if (settings.iStopFrame == -1)
			settings.iStopFrame = (int)uiFrames - 1;

		if ((settings.iStartFrame >= 0) && (settings.iStartFrame < settings.iStopFrame) && (settings.iStopFrame < (int)uiFrames))
		{
			uiFirstFrame = (unsigned int)settings.iStartFrame;
			uiLastFrame = (unsigned int)settings.iStopFrame;
			uiFramesToProcess = uiLastFrame - uiFirstFrame + 1;
			fprintf(stderr, "\r%s\r", utils.StrPad("Please wait...", uiConsoleWidth).c_str());
		}
		else
		{
			fprintf(stderr, "\r%s", utils.StrPad("", uiConsoleWidth).c_str());
			sConsoleOut = utils.StrFormat("Invalid frame range specified (%d,%d), using default", settings.iStartFrame, settings.iStopFrame);
			fprintf(stderr, "\r%s\r", sConsoleOut.c_str());
			Sleep(2000);
			uiFirstFrame = 0;
			uiLastFrame = uiFrames - 1;
			uiFramesToProcess = uiFrames;
		}

		while ((uiFramesToProcess / uiFrameInterval) > 500000)
			uiFrameInterval *= 10;

		unsigned int uiArraySize = (uiFramesToProcess / uiFrameInterval) + 2;

		PerformanceData = new perfdata[uiArraySize];
		for (unsigned int x = 0; x < uiArraySize; x++)
		{
			PerformanceData[x].frame_low = 0;
			PerformanceData[x].frame_high = 0;
			PerformanceData[x].frame_interval_time = 0.0;
			PerformanceData[x].cpu_usage = 0;
			PerformanceData[x].phys_mem = 0;
			PerformanceData[x].virt_mem = 0;
			PerformanceData[x].num_threads = 0;
		}
		bPerfDataInit = TRUE;


		bEarlyExit = FALSE;
		unsigned int n = 0;
		BOOL bFirstScr = TRUE;
		unsigned int uiElapsedMS = 0;
		unsigned int uiEstimatedMS = 0;

		CProcessInfo process_info;
		DWORD dwPhysicalMemPeakMB = 0;
		DWORD dwVirtualMemPeakMB = 0;
		DWORD dwPhysicalMemCurrentMB = 0;
		DWORD dwVirtualMemCurrentMB = 0;
		unsigned int uiCPUUsageAcc = 0;
		unsigned int uiCPUUsageAvg = 0;
		unsigned int uiCounter = 0;

		unsigned int uiCurrentFrame = 0;
		double dFPSAverage = 0.0;
		double dFPSCurrent = 0.0;
		double dFPSMin = 1.0e+20;
		double dFPSMax = 0.0;

		double dStartTime = utils.GetTimer();
		double dCurrentTime = dStartTime;
		double dLastDisplayTime = dStartTime;
		double dLastIntervalTime = dStartTime;

		process_info.Update();

		for (uiCurrentFrame = uiFirstFrame; uiCurrentFrame <= uiLastFrame; uiCurrentFrame++)
		{
			PVideoFrame src_frame = AVS_clip->GetFrame(uiCurrentFrame, AVS_env);
			++n;

			if (((n % uiFrameInterval) == 0) || (n == uiFramesToProcess))
			{
				dCurrentTime = utils.GetTimer();
				process_info.Update();
				++uiCounter;

				uiElapsedMS = (unsigned int)(((dCurrentTime - dStartTime) * 1000.0) + 0.5);
				uiEstimatedMS = (unsigned int)((double)uiFramesToProcess * (double)uiElapsedMS / (double)n);

				uiCPUUsageAcc += (unsigned int)process_info.wCPUUsage;
				uiCPUUsageAvg = (unsigned int)(((double)uiCPUUsageAcc / (double)uiCounter) + 0.5);

				dwPhysicalMemCurrentMB = process_info.dwPhysicalMemMB;
				dwVirtualMemCurrentMB = process_info.dwVirtualMemMB;

				if (dwPhysicalMemCurrentMB > dwPhysicalMemPeakMB)
					dwPhysicalMemPeakMB = dwPhysicalMemCurrentMB;
				if (dwVirtualMemCurrentMB > dwVirtualMemPeakMB)
					dwVirtualMemPeakMB = dwVirtualMemCurrentMB;

				dFPSAverage = (double)n / (dCurrentTime - dStartTime);

				if ((n % uiFrameInterval) == 0)
				{
					if ((dCurrentTime - dLastIntervalTime) > 0.0)
						dFPSCurrent = (double)uiFrameInterval / (dCurrentTime - dLastIntervalTime);
					if (dFPSCurrent > dFPSMax)
						dFPSMax = dFPSCurrent;
					if (dFPSCurrent < dFPSMin)
						dFPSMin = dFPSCurrent;

					PerformanceData[uiPDIndex].frame_low = (uiCurrentFrame + 1) - uiFrameInterval;
					PerformanceData[uiPDIndex].frame_high = uiCurrentFrame;
					PerformanceData[uiPDIndex].frame_interval_time = (dCurrentTime - dLastIntervalTime) / (double)uiFrameInterval;
					PerformanceData[uiPDIndex].cpu_usage = process_info.wCPUUsage;
					PerformanceData[uiPDIndex].num_threads = process_info.wThreadCount;
					PerformanceData[uiPDIndex].phys_mem = dwPhysicalMemCurrentMB;
					PerformanceData[uiPDIndex].virt_mem = dwVirtualMemCurrentMB;
					++uiPDIndex;

					dLastIntervalTime = dCurrentTime;
				}

				if ((dCurrentTime - dLastDisplayTime) > 0.2)
				{
					uiConsoleWidth = utils.GetConsoleWidth();
					dLastDisplayTime = dCurrentTime;

					if (!bFirstScr)
						utils.CursorUp(10);

					bFirstScr = FALSE;
					sConsoleOut = utils.StrFormat("Frame (current | last):         %u | %u", uiCurrentFrame, uiLastFrame);
					fprintf(stderr, "\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
					sConsoleOut = utils.StrFormat("FPS (cur | min | max | avg):    %s | %s | %s | %s", utils.StrFormatFPS(dFPSCurrent).c_str(), utils.StrFormatFPS(dFPSMin).c_str(), utils.StrFormatFPS(dFPSMax).c_str(), utils.StrFormatFPS(dFPSAverage).c_str());
					fprintf(stderr, "\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
					sConsoleOut = utils.StrFormat("CPU usage (current | average):  %u%% | %u%%", process_info.wCPUUsage, uiCPUUsageAvg);
					fprintf(stderr, "\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
					sConsoleOut = utils.StrFormat("Thread count:                   %u", process_info.wThreadCount);
					fprintf(stderr, "\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
					sConsoleOut = utils.StrFormat("Physical Memory usage:          %u MB", dwPhysicalMemCurrentMB);
					fprintf(stderr, "\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
					sConsoleOut = utils.StrFormat("Virtual Memory usage:           %u MB", dwVirtualMemCurrentMB);
					fprintf(stderr, "\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
					sConsoleOut = utils.StrFormat("Time (elapsed | estimated):     %s | %s", utils.StrFormatTime(uiElapsedMS).c_str(), utils.StrFormatTime(uiEstimatedMS).c_str());
					fprintf(stderr, "\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());

					fprintf(stderr, "\nPress \'Esc\' to cancel the process...\n\n");

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

		#if defined(AVS26_32) || defined(AVS26_64)
			AVS_env->DeleteScriptEnvironment();
		#else
			AVS_env->~IScriptEnvironment();
		#endif

		sLogBuffer += "\n\n[Runtime info]\n";

		if (n >= uiFrameInterval)
		{
			if (!bFirstScr)
				utils.CursorUp(10);

			sConsoleOut = utils.StrFormat("Frames processed:               %u (%u - %u)", n, uiFirstFrame, uiLastFrame);
			fprintf(stderr, "\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
			sLogBuffer += sConsoleOut + "\n";

			sConsoleOut = utils.StrFormat("FPS (min | max | average):      %s | %s | %s", utils.StrFormatFPS(dFPSMin).c_str(), utils.StrFormatFPS(dFPSMax).c_str(), utils.StrFormatFPS(dFPSAverage).c_str());
			fprintf(stderr, "\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
			sLogBuffer += sConsoleOut + "\n";

			sConsoleOut = utils.StrFormat("CPU usage (average):            %u%%", uiCPUUsageAvg);
			fprintf(stderr, "\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
			sLogBuffer += sConsoleOut + "\n";

			sConsoleOut = utils.StrFormat("Thread count:                   %u", process_info.wThreadCount);
			fprintf(stderr, "\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
			sLogBuffer += sConsoleOut + "\n";

			sConsoleOut = utils.StrFormat("Physical Memory usage (peak):   %u MB", dwPhysicalMemPeakMB);
			fprintf(stderr, "\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
			sLogBuffer += sConsoleOut + "\n";

			sConsoleOut = utils.StrFormat("Virtual Memory usage (peak):    %u MB", dwVirtualMemPeakMB);
			fprintf(stderr, "\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
			sLogBuffer += sConsoleOut + "\n";

			if ((settings.bLogEstimatedTime) && (n < uiFramesToProcess))
				sConsoleOut = utils.StrFormat("Time (elapsed | estimated):     %s | %s", utils.StrFormatTime(uiElapsedMS).c_str(), utils.StrFormatTime(uiEstimatedMS).c_str());
			else
				sConsoleOut = utils.StrFormat("Time (elapsed):                 %s", utils.StrFormatTime(uiElapsedMS).c_str());

			fprintf(stderr, "\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
			sLogBuffer += sConsoleOut + "\n";

			fprintf(stderr, "\r%s\n", utils.StrPad("", uiConsoleWidth).c_str());
			fprintf(stderr, "\r%s\n", utils.StrPad("", uiConsoleWidth).c_str());
			utils.CursorUp(2);
		}
		else
		{
			if (!bFirstScr)
			{
				for (int i = 0; i <= 9; i++)
				{
					fprintf(stderr, "\r%s", utils.StrPad("", uiConsoleWidth).c_str());
					utils.CursorUp(1);
				}
			}
			else
				fprintf(stderr, "\r%s", utils.StrPad("", uiConsoleWidth).c_str());

			sLogBuffer += "Insufficient data\n";
		}
	}
	catch(AvisynthError err)
	{
		sAVSError = utils.StrFormat("%s", (PCSTR)err.msg);

		if (bEarlyExit == FALSE)
		{
			utils.CursorUp(2);
			fprintf(stderr, "\r%s\n", utils.StrPad("", uiConsoleWidth).c_str());
			fprintf(stderr, "\r%s\n", utils.StrPad("", uiConsoleWidth).c_str());
			utils.CursorUp(2);
			fprintf(stderr, "\r%s\n", sAVSError.c_str());
		}
		else
			fprintf(stderr, "\n%s\n", sAVSError.c_str());
	}

	#if defined(AVS26_32) || defined(AVS26_64)
		AVS_linkage = 0;
	#endif

	::FreeLibrary(hDLL);

	if (settings.bCreateLog)
	{
		string sRet = CreateLogFile(sAVSFile, sLogBuffer, PerformanceData, uiPDIndex, sAVSError, uiFrameInterval);
		if (sRet != "")
		{
			fprintf(stderr, sRet.c_str());
			Pause();
			return -1;
		}
	}

	if (bPerfDataInit)
		delete[] PerformanceData;

	Pause();

	return 0;
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
		hINIFile.flush();
		hINIFile.close();
	}

	ifstream hINIFile(sINIFile.c_str());
	if (!hINIFile.is_open())
	{
		sRet = utils.StrFormat("\nCannot open \"%s\"\n", sINIFile.c_str());
		return sRet;
	}

	settings.bCreateLog = FALSE;
	settings.bPauseBeforeExit = FALSE;
	settings.iStartFrame = 0;
	settings.iStopFrame = -1;
	settings.bInvokeDistributor = TRUE;
	settings.bAllowOnlyOneInstance = TRUE;
	settings.bLogEstimatedTime = FALSE;

	string sCurrentLine = "";
	size_t iPos = 0;

	while (getline(hINIFile, sCurrentLine))
	{
		utils.StrToLC(sCurrentLine);
		utils.StrTrim(sCurrentLine);
		sCurrentLine.erase(remove(sCurrentLine.begin(), sCurrentLine.end(), ' '), sCurrentLine.end());

		if ((sCurrentLine.substr(0, 9) == "createlog") && (sCurrentLine.substr(sCurrentLine.length() - 1) == "1"))
			settings.bCreateLog = TRUE;
		if ((sCurrentLine.substr(0, 15) == "pausebeforeexit") && (sCurrentLine.substr(sCurrentLine.length() - 1) == "1"))
			settings.bPauseBeforeExit = TRUE;
		if ((sCurrentLine.substr(0, 17) == "invokedistributor") && (sCurrentLine.substr(sCurrentLine.length() - 1) == "0"))
			settings.bInvokeDistributor = FALSE;
		if ((sCurrentLine.substr(0, 20) == "allowonlyoneinstance") && (sCurrentLine.substr(sCurrentLine.length() - 1) == "0"))
			settings.bAllowOnlyOneInstance = FALSE;
		if ((sCurrentLine.substr(0, 16) == "logestimatedtime") && (sCurrentLine.substr(sCurrentLine.length() - 1) == "1"))
			settings.bLogEstimatedTime = TRUE;
		if (sCurrentLine.substr(0, 10) == "framerange")
		{
			iPos = sCurrentLine.find("=");
			if (iPos == 10)
			{
				sTemp = sCurrentLine.substr(iPos + 1);
				iPos = sTemp.find(",");
				if (iPos > 0)
				{
					settings.iStartFrame = atoi(sTemp.substr(0, iPos).c_str());
					settings.iStopFrame = atoi(sTemp.substr(iPos + 1).c_str());
				}
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

	string sLogFile = s_avsfile + ".log";
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
			sLog = "\n[Performance data]\n       Frame    Frames/sec   Time/frame(ms)   CPU(%)   Threads   PhysMEM(MB)   VirtMEM(MB)\n";
		else
			sLog = "\n[Performance data]\n       Frame interval    Frames/sec   Time/frame(ms)   CPU(%)   Threads   PhysMEM(MB)   VirtMEM(MB)\n";

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
					stemp2 = utils.StrFormat("%s%u %13.3f %16.6f %8u %9u %13u %13u", spad.c_str(), cs_pdata[i].frame_low, 1.0 / cs_pdata[i].frame_interval_time, cs_pdata[i].frame_interval_time * 1000.0, cs_pdata[i].cpu_usage, cs_pdata[i].num_threads, cs_pdata[i].phys_mem, cs_pdata[i].virt_mem);
				}
				else
				{
					stemp1 = utils.StrFormat("%u-%u", cs_pdata[i].frame_low, cs_pdata[i].frame_high);
					string spad((21 - stemp1.length()), ' ');
					stemp2 = utils.StrFormat("%s%u-%u %13.3f %16.6f %8u %9u %13u %13u", spad.c_str(), cs_pdata[i].frame_low, cs_pdata[i].frame_high, 1.0 / cs_pdata[i].frame_interval_time, cs_pdata[i].frame_interval_time * 1000.0, cs_pdata[i].cpu_usage, cs_pdata[i].num_threads, cs_pdata[i].phys_mem, cs_pdata[i].virt_mem);
				}

				hLogFile << stemp2 + "\n";
			}
		}
	}

	if (s_avserror != "")
	{
		hLogFile << "\n\n[AVISynth error]\n" + s_avserror + "\n";
	}

	hLogFile.flush();
	hLogFile.close();

	return sRet;
}


void Pause()
{
	if (settings.bPauseBeforeExit)
	{
		fprintf(stderr, "\nPress any key to exit...");
		for (;;)
		{
			Sleep(100);
			if (_kbhit())
				break;
		}
		fprintf(stderr, "\r%s", utils.StrPad("", utils.GetConsoleWidth()).c_str());
	}

	return;
}


unsigned int CalculateFrameInterval(string &s_avsfile, string &s_avserror)
{
	unsigned int uiFI = 1000;
	s_avserror = "";

	HINSTANCE hDLL = ::LoadLibrary("avisynth");
	if (!hDLL)
	{
		s_avserror = "Failed to load avisynth.dll";
		return 0;
	}

	try
	{
		IScriptEnvironment *(__stdcall *CreateEnvironment)(int) = (IScriptEnvironment *(__stdcall *)(int))::GetProcAddress(hDLL, "CreateScriptEnvironment");
		if (!CreateEnvironment)
		{
			s_avserror = "Failed to load CreateScriptEnvironment()";
			::FreeLibrary(hDLL);
			return 0;
		}

		IScriptEnvironment *AVS_env = CreateEnvironment(AVISYNTH_INTERFACE_VERSION);
		if (!AVS_env)
		{
			s_avserror = "Could not create IScriptenvironment";
			::FreeLibrary(hDLL);
			return 0;
		}

		#if defined(AVS26_32) || defined(AVS26_64)
			AVS_linkage = AVS_env->GetAVSLinkage();
		#endif

		AVSValue AVS_main;
		AVS_main = AVS_env->Invoke("Import", s_avsfile.c_str());

		if (!AVS_main.IsClip()) //not a clip
			AVS_env->ThrowError("Script did not return a video clip:\n(%s)", s_avsfile.c_str());

		AVSValue AVS_temp;
		PClip AVS_clip;
		VideoInfo	AVS_vidinfo;

		int iMTMode = 0;
		try
		{
			AVS_temp = AVS_env->Invoke("GetMTMode", false);
			iMTMode = AVS_temp.IsInt() ? AVS_temp.AsInt() : 0;
			if ((iMTMode > 0) && (iMTMode < 5) && settings.bInvokeDistributor)
				AVS_main = AVS_env->Invoke("Distributor", AVS_main);
		}
		catch (IScriptEnvironment::NotFound)
		{
		}

		AVS_clip = AVS_main.AsClip();
		AVS_vidinfo = AVS_clip->GetVideoInfo();
		unsigned int uiTotalFrames = (unsigned int)AVS_vidinfo.num_frames;

		if (uiTotalFrames < 1)
			AVS_env->ThrowError("Script did not return a video clip:\n(%s)", s_avsfile.c_str());

		unsigned int uiCurrentFrame = 0;
		double dT1 = utils.GetTimer();
		double dT2 = dT1;
		for (;;)
		{
			PVideoFrame src_frame = AVS_clip->GetFrame(uiCurrentFrame, AVS_env);
			++uiCurrentFrame;
			dT2 = utils.GetTimer();

			if (((dT2 - dT1) >= 4.0) || (uiCurrentFrame == uiTotalFrames))
				break;
		}

		double dTDelta = (dT2 - dT1) * 1000.0; //ms
		double dAverageFrameTime = 0.0;
		if (dTDelta > 200)
			dAverageFrameTime = dTDelta / (double)uiCurrentFrame;

		if (dAverageFrameTime > 0.10)
			uiFI = 500;
		if (dAverageFrameTime > 0.25)
			uiFI = 200;
		if (dAverageFrameTime > 0.50)
			uiFI = 100;
		if (dAverageFrameTime > 1.00)
			uiFI = 50;
		if (dAverageFrameTime > 2.50)
			uiFI = 20;
		if (dAverageFrameTime > 5.00)
			uiFI = 10;
		if (dAverageFrameTime > 50.0)
			uiFI = 1;

		AVS_clip = 0;
		AVS_main = 0;
		AVS_temp = 0;

		#if defined(AVS26_32) || defined(AVS26_64)
			AVS_env->DeleteScriptEnvironment();
		#else
			AVS_env->~IScriptEnvironment();
		#endif
	}
	catch(AvisynthError err)
	{
		s_avserror = utils.StrFormat("%s", (PCSTR)err.msg);
		uiFI = 0;
	}

	#if defined(AVS26_32) || defined(AVS26_64)
		AVS_linkage = 0;
	#endif
	::FreeLibrary(hDLL);

	return uiFI;
}


void PrintUsage()
{
		fprintf(stderr, "\nUsage:  AVSMeter script.avs [switches]\n\n");
		fprintf(stderr, "Switches:\n");
		fprintf(stderr, "-i      Display clip info only\n");
		fprintf(stderr, "-l      Create log file\n");

		return;
}

