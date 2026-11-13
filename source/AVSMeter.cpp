// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include "stdafx.h"
#include "utility.h"
#include "ProcessInfo.h"
#include "timer.h"

using namespace std;

#define AVSDLL_SUCCESS 0x00
#define AVSDLL_INCOMPATIBLE 0x01
#define CANNOT_LOAD_AVSDLL 0x02
#define NO_AVS_LINKAGE 0x03

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
	double fps;
	WORD  cpu_usage;
	DWORD phys_mem;
	DWORD virt_mem;
	WORD num_threads;
} perfdata;

CUtils utils;
CTimer timer;

unsigned int CalculateFrameInterval(string &s_avsfile, string &s_avserror);
string CreateLogFile(string &s_avsfile, string &s_logbuffer, perfdata * cs_pdata, unsigned int ui_index, string &s_avserror, unsigned int ui_frameinterval);
string ParseINIFile();
BYTE CheckAvisynth();
BOOL GetAvisynthVersion(string &sMsg, BOOL &bIsMTVersion);
void PrintUsage();
void Pause();


int main(int argc, char* argv[])
{
	string sAVSMVersion = "";
	if (!utils.GetProductVersion(sAVSMVersion))
	{
		printf("\nCannot retrieve product version\n");
		Pause();
		return -1;
	}

	#ifdef AVS26_32
		sAVSMVersion += " (AVS 2.6, x86)";
	#elif AVS26_64
		sAVSMVersion += " (AVS 2.6, x64)";
	#else
		sAVSMVersion += " (AVS 2.5, x86)";
	#endif

	printf("\nAVSMeter %s by Groucho2004\n", sAVSMVersion.c_str());

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

	BYTE bRet = CheckAvisynth();

	if (bRet == CANNOT_LOAD_AVSDLL)
	{
		printf("\nCannot load avisynth.dll.\n");
		Pause();
		return -1;
	}

	if (bRet == AVSDLL_INCOMPATIBLE)
	{
		#ifdef AVS26_64
			printf("\nCannot load avisynth.dll.\nMake sure you are using a 64 Bit version.\n");
		#else
			printf("\nCannot load avisynth.dll.\nMake sure you are using a 32 Bit version.\n");
		#endif

		Pause();
		return -1;
	}

	if (bRet == NO_AVS_LINKAGE)
	{
		#if defined(AVS26_32) || defined(AVS26_64)
			printf("\nAvisynth 2.6 (Alpha 4 and above) is required.\n");
			Pause();
			return -1;
		#endif
	}

	string sAvisynthVersion = "";
	BOOL bIsMTVersion = FALSE;
	if (!GetAvisynthVersion(sAvisynthVersion, bIsMTVersion))
	{
		printf("\n%s\n", sAvisynthVersion.c_str());
		Pause();
		return -1;
	}

	if (bIsMTVersion)
		sAvisynthVersion += " (MT)";

	printf("\r%s\n", sAvisynthVersion.c_str());

	string sRet = ParseINIFile();
	if (sRet != "")
	{
		printf(sRet.c_str());
		Pause();
		return -1;
	}

	if (settings.bAllowOnlyOneInstance)
	{
		CreateMutex(NULL, TRUE, "__avsmeter__single__instance__lock__");
		if (GetLastError() == ERROR_ALREADY_EXISTS)
		{
			printf("\nOne instance of AVSMeter is already running\n");
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
						printf("%s", sConsoleOut.c_str());
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
				printf("%s", sConsoleOut.c_str());
				Pause();
				return -1;
			}
		}

		else
		{
			sConsoleOut = utils.StrFormat("\nInvalid argument: \"%s\"\n", sArg.c_str());
			printf("%s", sConsoleOut.c_str());
			Pause();
			return -1;
		}
	}

	if (sAVSFile == "")
	{
		printf("\nNo script file specified\n");
		Pause();
		return -1;
	}

	if (!bInfoOnly)
	{
		sAVSError = "";
		printf("\r%s\r", utils.StrPad("Analysing script, please wait...", uiConsoleWidth).c_str());
		uiFrameInterval = CalculateFrameInterval(sAVSFile, sAVSError);
		printf("\r%s", utils.StrPad("", uiConsoleWidth).c_str());
		if ((uiFrameInterval == 0) || (sAVSError != ""))
		{
			printf("\n%s\n", sAVSError.c_str());
			Pause();
			return -1;
		}
	}


	HINSTANCE hDLL = ::LoadLibrary("avisynth");
	if (!hDLL)
	{
		printf("\nCannot load avisynth.dll\n");
		Pause();
		return -1;
	}

	try
	{
		IScriptEnvironment *(__stdcall *CreateEnvironment)(int) = (IScriptEnvironment *(__stdcall *)(int))::GetProcAddress(hDLL, "CreateScriptEnvironment");
		if (!CreateEnvironment)
		{
			printf("\nFailed to load CreateScriptEnvironment()\n");
			Pause();
			return -1;
		}

		IScriptEnvironment *AVS_env = CreateEnvironment(AVISYNTH_INTERFACE_VERSION);
		if (!AVS_env)
		{
			printf("\nCould not create IScriptenvironment\n");

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

		sLogBuffer += "[General info]\n";
		sConsoleOut = utils.StrFormat("Log file created with:   AVSMeter %s", sAVSMVersion.c_str());
		sLogBuffer += sConsoleOut + "\n";
		sConsoleOut = utils.StrFormat("Avisynth version:        %s", sAvisynthVersion.c_str());
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

		sConsoleOut = utils.StrFormat("Length (hhh:mm:ss.ms):   %s", timer.FormatTimeString(uiMilliSeconds).c_str());
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
					printf(sLogRet.c_str());

				delete[] PerformanceData;
			}

			Pause();
			return 0;
		}

		printf("\n");

		unsigned int uiFramesToProcess = 0;
		unsigned int uiFirstFrame = 0;
		unsigned int uiLastFrame = uiFrames - 1;

		if (settings.iStopFrame == -1)
			settings.iStopFrame = (int)uiFrames - 1;

		if ((settings.iStartFrame >= 0) && (settings.iStartFrame <= settings.iStopFrame) && (settings.iStopFrame < (int)uiFrames))
		{
			uiFirstFrame = (unsigned int)settings.iStartFrame;
			uiLastFrame = (unsigned int)settings.iStopFrame;
			uiFramesToProcess = uiLastFrame - uiFirstFrame + 1;
			printf("\r%s\r", utils.StrPad("Please wait...", uiConsoleWidth).c_str());
		}
		else
		{
			printf("\r%s", utils.StrPad("", uiConsoleWidth).c_str());
			sConsoleOut = utils.StrFormat("Invalid frame range specified (%d,%d), using default", settings.iStartFrame, settings.iStopFrame);
			printf("\r%s\r", sConsoleOut.c_str());
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
			PerformanceData[x].fps = 0.0;
			PerformanceData[x].cpu_usage = 0;
			PerformanceData[x].phys_mem = 0;
			PerformanceData[x].virt_mem = 0;
			PerformanceData[x].num_threads = 0;
		}

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

		double dStartTime = timer.GetTimer();
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
				dCurrentTime = timer.GetTimer();
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
					sConsoleOut = utils.StrFormat("Frame (current | last):         %u | %u", uiCurrentFrame, uiLastFrame);
					printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
					sConsoleOut = utils.StrFormat("FPS (cur | min | max | avg):    %s | %s | %s | %s", utils.StrFormatFPS(dFPSCurrent).c_str(), utils.StrFormatFPS(dFPSMin).c_str(), utils.StrFormatFPS(dFPSMax).c_str(), utils.StrFormatFPS(dFPSAverage).c_str());
					printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
					sConsoleOut = utils.StrFormat("CPU usage (current | average):  %u%% | %u%%", process_info.wCPUUsage, uiCPUUsageAvg);
					printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
					sConsoleOut = utils.StrFormat("Thread count:                   %u", process_info.wThreadCount);
					printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
					sConsoleOut = utils.StrFormat("Physical Memory usage:          %u MB", dwPhysicalMemCurrentMB);
					printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
					sConsoleOut = utils.StrFormat("Virtual Memory usage:           %u MB", dwVirtualMemCurrentMB);
					printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
					sConsoleOut = utils.StrFormat("Time (elapsed | estimated):     %s | %s", timer.FormatTimeString(uiElapsedMS).c_str(), timer.FormatTimeString(uiEstimatedMS).c_str());
					printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());

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

			if (uiFirstFrame == uiLastFrame)
			{
				if (uiFirstFrame == 0)
					sConsoleOut = utils.StrFormat("Frames processed:               %u", n, uiFirstFrame);
				else
					sConsoleOut = utils.StrFormat("Frames processed:               %u (%u)", n, uiFirstFrame);
			}
			else
				sConsoleOut = utils.StrFormat("Frames processed:               %u (%u - %u)", n, uiFirstFrame, uiLastFrame);

			printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
			sLogBuffer += sConsoleOut + "\n";

			sConsoleOut = utils.StrFormat("FPS (min | max | average):      %s | %s | %s", utils.StrFormatFPS(dFPSMin).c_str(), utils.StrFormatFPS(dFPSMax).c_str(), utils.StrFormatFPS(dFPSAverage).c_str());
			printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
			sLogBuffer += sConsoleOut + "\n";

			sConsoleOut = utils.StrFormat("CPU usage (average):            %u%%", uiCPUUsageAvg);
			printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
			sLogBuffer += sConsoleOut + "\n";

			sConsoleOut = utils.StrFormat("Thread count:                   %u", process_info.wThreadCount);
			printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
			sLogBuffer += sConsoleOut + "\n";

			sConsoleOut = utils.StrFormat("Physical Memory usage (peak):   %u MB", dwPhysicalMemPeakMB);
			printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
			sLogBuffer += sConsoleOut + "\n";

			sConsoleOut = utils.StrFormat("Virtual Memory usage (peak):    %u MB", dwVirtualMemPeakMB);
			printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
			sLogBuffer += sConsoleOut + "\n";

			if ((settings.bLogEstimatedTime) && (n < uiFramesToProcess))
				sConsoleOut = utils.StrFormat("Time (elapsed | estimated):     %s | %s", timer.FormatTimeString(uiElapsedMS).c_str(), timer.FormatTimeString(uiEstimatedMS).c_str());
			else
				sConsoleOut = utils.StrFormat("Time (elapsed):                 %s", timer.FormatTimeString(uiElapsedMS).c_str());

			printf("\r%s\n", utils.StrPad(sConsoleOut, uiConsoleWidth).c_str());
			sLogBuffer += sConsoleOut + "\n";

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
		sAVSError = utils.StrFormat("%s", (PCSTR)err.msg);

		if (bEarlyExit == FALSE)
		{
			utils.CursorUp(2);
			printf("\r%s\n", utils.StrPad("", uiConsoleWidth).c_str());
			printf("\r%s\n", utils.StrPad("", uiConsoleWidth).c_str());
			utils.CursorUp(2);
			printf("\r%s\n", sAVSError.c_str());
		}
		else
			printf("\n%s\n", sAVSError.c_str());
	}
	catch (...)
	{
		if (bEarlyExit == FALSE)
		{
			utils.CursorUp(2);
			printf("\r%s\n", utils.StrPad("", uiConsoleWidth).c_str());
			printf("\r%s\n", utils.StrPad("", uiConsoleWidth).c_str());
			utils.CursorUp(2);
			printf("\rUnknown exception\n");
		}
		else
			printf("\nUnknown exception\n");
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
			printf(sRet.c_str());
			Pause();
			return -1;
		}
	}

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
					stemp2 = utils.StrFormat("%s%u %13.3f %16.4f %8u %9u %13u %13u", spad.c_str(), cs_pdata[i].frame_low, cs_pdata[i].fps, 1000.0 / cs_pdata[i].fps, cs_pdata[i].cpu_usage, cs_pdata[i].num_threads, cs_pdata[i].phys_mem, cs_pdata[i].virt_mem);
				}
				else
				{
					stemp1 = utils.StrFormat("%u-%u", cs_pdata[i].frame_low, cs_pdata[i].frame_high);
					string spad((21 - stemp1.length()), ' ');
					stemp2 = utils.StrFormat("%s%u-%u %13.3f %16.4f %8u %9u %13u %13u", spad.c_str(), cs_pdata[i].frame_low, cs_pdata[i].frame_high, cs_pdata[i].fps, 1000.0 / cs_pdata[i].fps, cs_pdata[i].cpu_usage, cs_pdata[i].num_threads, cs_pdata[i].phys_mem, cs_pdata[i].virt_mem);
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


unsigned int CalculateFrameInterval(string &s_avsfile, string &s_avserror)
{
	unsigned int uiFrameInterval = 0;
	s_avserror = "";

	if (!timer.TestTimer())
	{
		s_avserror = "Multimedia Timers and/or High Performance Timers are not supported";
		return uiFrameInterval;
	}

	HINSTANCE hDLL = ::LoadLibrary("avisynth");
	if (!hDLL)
	{
		s_avserror = "Failed to load avisynth.dll";
		return uiFrameInterval;
	}

	try
	{
		IScriptEnvironment *(__stdcall *CreateEnvironment)(int) = (IScriptEnvironment *(__stdcall *)(int))::GetProcAddress(hDLL, "CreateScriptEnvironment");
		if (!CreateEnvironment)
		{
			s_avserror = "Failed to load CreateScriptEnvironment()";
			::FreeLibrary(hDLL);
			return uiFrameInterval;
		}

		IScriptEnvironment *AVS_env = CreateEnvironment(AVISYNTH_INTERFACE_VERSION);
		if (!AVS_env)
		{
			s_avserror = "Could not create IScriptenvironment";
			::FreeLibrary(hDLL);
			return uiFrameInterval;
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
		
		double dT1 = 0.0;
		double dT2 = 0.0;
		double dTDelta = -1.0;

		while (dTDelta < 0.0) //account for timer wrap
		{
			dT1 = timer.GetMMTimer();
			dT2 = dT1;
			for (;;)
			{
				PVideoFrame src_frame = AVS_clip->GetFrame(uiCurrentFrame, AVS_env);
				++uiCurrentFrame;
				dT2 = timer.GetMMTimer();

				if (((dT2 - dT1) >= 4.0) || (uiCurrentFrame == uiTotalFrames))
					break;
			}
			dTDelta = (dT2 - dT1) * 1000.0; //ms
		}

		uiFrameInterval = 50000;
		double dAverageFrameTime = 0.0;
		if (dTDelta > 200)
			dAverageFrameTime = dTDelta / (double)uiCurrentFrame;

		if (dAverageFrameTime > 0.001)
			uiFrameInterval = 20000;
		if (dAverageFrameTime > 0.002)
			uiFrameInterval = 10000;
		if (dAverageFrameTime > 0.004)
			uiFrameInterval = 5000;
		if (dAverageFrameTime > 0.010)
			uiFrameInterval = 2000;
		if (dAverageFrameTime > 0.020)
			uiFrameInterval = 1000;
		if (dAverageFrameTime > 0.040)
			uiFrameInterval = 500;
		if (dAverageFrameTime > 0.100)
			uiFrameInterval = 200;
		if (dAverageFrameTime > 0.200)
			uiFrameInterval = 100;
		if (dAverageFrameTime > 0.400)
			uiFrameInterval = 50;
		if (dAverageFrameTime > 1.000)
			uiFrameInterval = 20;
		if (dAverageFrameTime > 2.000)
			uiFrameInterval = 10;
		if (dAverageFrameTime > 20.00)
			uiFrameInterval = 1;

		AVS_clip = 0;
		AVS_main = 0;
		AVS_temp = 0;

		#if defined(AVS26_32) || defined(AVS26_64)
			AVS_env->DeleteScriptEnvironment();
		#else
			AVS_env->~IScriptEnvironment();
		#endif
	}
	catch (AvisynthError err)
	{
		s_avserror = utils.StrFormat("%s", (PCSTR)err.msg);
		uiFrameInterval = 0;
	}
	catch (...)
	{
		s_avserror = "Unknown exception";
		uiFrameInterval = 0;
	}

	#if defined(AVS26_32) || defined(AVS26_64)
		AVS_linkage = 0;
	#endif
	::FreeLibrary(hDLL);

	return uiFrameInterval;
}


BYTE CheckAvisynth()
{
	BYTE bRet = AVSDLL_SUCCESS;

	try
	{
		LOADED_IMAGE li;
		BOOL bLoaded = MapAndLoad("avisynth", NULL, &li,	TRUE,	TRUE);
		if (!bLoaded)
			return CANNOT_LOAD_AVSDLL;

		DWORD expVA = li.FileHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
		PIMAGE_EXPORT_DIRECTORY pExp = (PIMAGE_EXPORT_DIRECTORY)ImageRvaToVa(li.FileHeader, li.MappedAddress, expVA, NULL);
		DWORD rvaNames = pExp->AddressOfNames;
		DWORD *prvaNames = (DWORD*)ImageRvaToVa(li.FileHeader, li.MappedAddress, rvaNames, NULL);
		string sName = "";
		int iPos = 0;
		bRet = NO_AVS_LINKAGE;

		for (DWORD dwName = 0; dwName < pExp->NumberOfNames; ++dwName)
		{
			DWORD rvaName = prvaNames[dwName];
			sName = (char *)ImageRvaToVa(li.FileHeader, li.MappedAddress, rvaName, NULL);
			utils.StrToLC(sName);
			iPos = (int)sName.find("avs_linkage");
			if (iPos >= 0)
			{
				bRet = AVSDLL_SUCCESS;
				break;
			}
		}

		UnMapAndLoad(&li);
	}
	catch (...)
	{
		bRet = AVSDLL_INCOMPATIBLE;
	}

	return bRet;
}


BOOL GetAvisynthVersion(string &sMsg, BOOL &bIsMTVersion)
{
	HINSTANCE hDLL = ::LoadLibrary("avisynth");
	if (!hDLL)
	{
		sMsg = "Failed to load avisynth.dll";
		return FALSE;
	}

	try
	{
		IScriptEnvironment *(__stdcall *CreateEnvironment)(int) = (IScriptEnvironment *(__stdcall *)(int))::GetProcAddress(hDLL, "CreateScriptEnvironment");
		if (!CreateEnvironment)
		{
			sMsg = "Failed to load CreateScriptEnvironment()";
			::FreeLibrary(hDLL);
			return FALSE;
		}

		IScriptEnvironment *AVS_env = CreateEnvironment(AVISYNTH_INTERFACE_VERSION);
		if (!AVS_env)
		{
			sMsg = "Could not create IScriptenvironment";
			::FreeLibrary(hDLL);
			return FALSE;
		}

		#if defined(AVS26_32) || defined(AVS26_64)
			AVS_linkage = AVS_env->GetAVSLinkage();
		#endif

		AVSValue AVS_temp;
		try
		{
			AVS_temp = AVS_env->Invoke("VersionString", AVSValue(&AVS_temp, 0));
			sMsg = utils.StrFormat("%s", AVS_temp.AsString());
		}
		catch (IScriptEnvironment::NotFound)
		{
		}

		bIsMTVersion = FALSE;
		if ((AVS_env->FunctionExists("GetMTMode")) || (AVS_env->FunctionExists("Prefetch")))
			bIsMTVersion = TRUE;

		AVS_temp = 0;

		#if defined(AVS26_32) || defined(AVS26_64)
			AVS_env->DeleteScriptEnvironment();
		#else
			AVS_env->~IScriptEnvironment();
		#endif
	}
	catch (AvisynthError err)
	{
		sMsg = utils.StrFormat("%s", (PCSTR)err.msg);
	}
	catch (...)
	{
		sMsg = "Unknown exception";
	}

	#if defined(AVS26_32) || defined(AVS26_64)
		AVS_linkage = 0;
	#endif
	::FreeLibrary(hDLL);

	return TRUE;
}


void PrintUsage()
{
	printf("\nUsage:  AVSMeter script.avs [switches]\n\n");
	printf("Switches:\n");
	printf("-i      Display clip info only\n");
	printf("-l      Create log file\n");

	return;
}

