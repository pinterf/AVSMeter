#include <windows.h>
#include <conio.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include "utility.h"
#include "BasicString.h"
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

CBasicString bstr;
CUtils utils;

unsigned int CalculateFrameInterval(string &avs_file, string &cs_avserror);
string CreateLogFile(string &cs_avsfile, string &cs_logbuffer, perfdata * cs_pdata, unsigned int ui_index, string &cs_avserror, unsigned int ui_frameinterval);
string ParseINIFile();
void PrintUsage();
void SleepUntilEsc();
string FormatFPS(double d_FPSCurrent, double d_FPSMin, double d_FPSMax, double d_FPSAverage);

int main(int argc, char* argv[])
{
	string sAVSMVersion = "";
	if (!utils.GetProductVersion(sAVSMVersion))
	{
		printf("\nCannot retrieve product version\n");
		SleepUntilEsc();
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

	CreateMutex(NULL, TRUE, "__avsmeter__single__instance__lock__");
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		printf("\nOne instance of AVSMeter is already running\n");
		SleepUntilEsc();
		return -1;
	}

	string csConsoleOut = "";
	unsigned int uiConsoleWidth = utils.GetConsoleWidth();
	string csArg = "";
	string csArgTest = "";
	string csAVSFile = "";
	string csLogBuffer = "";
	perfdata * PerformanceData;
	unsigned int uiPDIndex = 0;
	BOOL bPerfDataInit = FALSE;
	BOOL bEarlyExit = TRUE;
	BOOL bInfoOnly = FALSE;
	string csAVSError = "";
	unsigned int uiFrameInterval = 0;

	string csRet = ParseINIFile();
	if (csRet != "")
	{
		printf(csRet.c_str());
		SleepUntilEsc();
		return -1;
	}

	if (argc < 2)
	{
		PrintUsage();
		SleepUntilEsc();
		return -1;
	}

	for (int iArg = 1; iArg < argc; iArg++)
	{
		csArg = argv[iArg];
		csArgTest = csArg;
		bstr.StrToLC(csArgTest);
		bstr.StrTrim(csArgTest);

		if (csArgTest.length() > 4)
		{
			if (csArgTest.substr(csArgTest.length() - 4) == ".avs")
			{
				LPTSTR lpPart;
				char szOut[2050];
				if (::GetFullPathName(argv[iArg], 2048, szOut, &lpPart))
				{
					csAVSFile = szOut;
					if (!utils.FileExists(csAVSFile))
					{
						csConsoleOut = bstr.StrFormat("\nFile not found: \"%s\"\n", csArg.c_str());
						printf("%s", csConsoleOut.c_str());
						SleepUntilEsc();
						return -1;
					}
				}
			}
			continue;
		}

		else if (csArgTest.length() == 2)
		{
			if (csArgTest == "-l")
			{
				settings.bCreateLog = TRUE;
				continue;
			}
			else if (csArgTest == "-i")
			{
				bInfoOnly = TRUE;
				continue;
			}
			else
			{
				csConsoleOut = bstr.StrFormat("\nInvalid argument: \"%s\"\n", csArg.c_str());
				printf("%s", csConsoleOut.c_str());
				SleepUntilEsc();
				return -1;
			}
		}

		else
		{
			csConsoleOut = bstr.StrFormat("\nInvalid argument: \"%s\"\n", csArg.c_str());
			printf("%s", csConsoleOut.c_str());
			SleepUntilEsc();
			return -1;
		}
	}

	if (csAVSFile == "")
	{
		printf("\nNo script file specified\n");
		SleepUntilEsc();
		return -1;
	}


	if (!utils.TestTimer())
	{
		printf("\nQueryPerformanceCounter() is not supported\n");
		SleepUntilEsc();
		return -1;
	}

	BYTE bRet = utils.CheckAvisynth();

	if (bRet == CANNOT_LOAD_AVSDLL)
	{
		printf("\nCannot load avisynth.dll.\n");
		SleepUntilEsc();
		return -1;
	}

	if (bRet == AVSDLL_INCOMPATIBLE)
	{
		#ifdef AVS26_64
			printf("\nCannot load avisynth.dll.\nMake sure you're using a 64 Bit version.\n");
		#else
			printf("\nCannot load avisynth.dll.\nMake sure you're using a 32 Bit version.\n");
		#endif

		SleepUntilEsc();
		return -1;
	}

	if (bRet == NO_AVS_LINKAGE)
	{
		#if defined(AVS26_32) || defined(AVS26_64)
			printf("\nAvisynth 2.6 (Alpha 4 and above) is required.\n");
			SleepUntilEsc();
			return -1;
		#endif
	}


	if (!bInfoOnly)
	{
		csAVSError = "";
		printf("\r%s\r", bstr.StrPad("Analysing script, please wait...", uiConsoleWidth).c_str());
		uiFrameInterval = CalculateFrameInterval(csAVSFile, csAVSError);
		printf("\r%s", bstr.StrPad("", uiConsoleWidth).c_str());
		if (uiFrameInterval == 0)
		{
			printf("\n%s\n", csAVSError.c_str());
			SleepUntilEsc();
			return -1;
		}
	}

	HINSTANCE hDLL = ::LoadLibrary("avisynth");
	if (!hDLL)
	{
		printf("\nCannot load avisynth.dll\n");
		SleepUntilEsc();
		return -1;
	}

	try
	{
		IScriptEnvironment *(__stdcall *CreateEnvironment)(int) = (IScriptEnvironment *(__stdcall *)(int))::GetProcAddress(hDLL, "CreateScriptEnvironment");
		if (!CreateEnvironment)
		{
			printf("\nFailed to load CreateScriptEnvironment()\n");
			SleepUntilEsc();
			return -1;
		}

		IScriptEnvironment *AVS_env = CreateEnvironment(AVISYNTH_INTERFACE_VERSION);
		if (!AVS_env)
		{
			printf("\nCould not create IScriptenvironment\n");

			SleepUntilEsc();
			return -1;
		}

		#if defined(AVS26_32) || defined(AVS26_64)
			AVS_linkage = AVS_env->GetAVSLinkage();
		#endif

		AVSValue AVS_main;
		AVSValue AVS_temp;
		PClip AVS_clip;
		VideoInfo	AVS_vidinfo;

		AVS_main = AVS_env->Invoke("Import", csAVSFile.c_str());

		if (!AVS_main.IsClip())
		{
			csConsoleOut = bstr.StrFormat("\"%s\":\nScript did not return a video clip", csAVSFile.c_str());
			AVS_env->ThrowError(csConsoleOut.c_str());
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

		string csAvisynthVersion = "";
		try
		{
			AVS_temp = AVS_env->Invoke("VersionString", AVSValue(&AVS_temp, 0));
			csAvisynthVersion = bstr.StrFormat("%s", AVS_temp.AsString());
		}
		catch (IScriptEnvironment::NotFound)
		{
		}

		csConsoleOut = bstr.StrPad(csAvisynthVersion, uiConsoleWidth).c_str();
		printf("\r%s\n", csConsoleOut.c_str());
		csLogBuffer += "[General info]\n";
		csConsoleOut = bstr.StrFormat("Log file created with:   AVSMeter %s", sAVSMVersion.c_str());
		csLogBuffer += csConsoleOut + "\n";
		csConsoleOut = bstr.StrFormat("Avisynth version:        %s", csAvisynthVersion.c_str());
		csLogBuffer += csConsoleOut + "\n";

		printf("\r%s", bstr.StrPad("", uiConsoleWidth).c_str());

		if (bIsMTVersion)
		{
			csConsoleOut = bstr.StrFormat("Active MT Mode: %d", iMTMode);
			printf("\r%s\n", csConsoleOut.c_str());
			csLogBuffer += "                         " + csConsoleOut + "\n";
		}

		csLogBuffer += "\n\n[Clip info]\n";

		csConsoleOut = bstr.StrFormat("Number of frames:          %11u", AVS_vidinfo.num_frames);
		printf("\n%s", csConsoleOut.c_str());
		csLogBuffer += csConsoleOut + "\n";

		csConsoleOut = bstr.StrFormat("Length (hhh:mm:ss.ms):   %s", bstr.FormatTimeString(uiMilliSeconds).c_str());
		printf("\n%s", csConsoleOut.c_str());
		csLogBuffer += csConsoleOut + "\n";

		csConsoleOut = bstr.StrFormat("Frame width:               %11u", AVS_vidinfo.width);
		printf("\n%s", csConsoleOut.c_str());
		csLogBuffer += csConsoleOut + "\n";

		csConsoleOut = bstr.StrFormat("Frame height:              %11u", AVS_vidinfo.height);
		printf("\n%s", csConsoleOut.c_str());
		csLogBuffer += csConsoleOut + "\n";

		csConsoleOut = bstr.StrFormat("Framerate:                 %11.3f (%u/%u)", (double)AVS_vidinfo.fps_numerator / (double)AVS_vidinfo.fps_denominator, AVS_vidinfo.fps_numerator, AVS_vidinfo.fps_denominator);
		printf("\n%s", csConsoleOut.c_str());
		csLogBuffer += csConsoleOut + "\n";

		if (!AVS_vidinfo.IsFieldBased())
			csConsoleOut = "Interlaced:                         No";
		else
		{
			if (AVS_vidinfo.IsTFF())
				csConsoleOut = "Interlaced:                        Yes (TFF)";
			else
				csConsoleOut = "Interlaced:                        Yes (BFF)";
		}

		printf("\n%s", csConsoleOut.c_str());
		csLogBuffer += csConsoleOut + "\n";

		csConsoleOut = "Colorspace:                      ";
		printf("\n%s", csConsoleOut.c_str());
		csLogBuffer += csConsoleOut;

		if (AVS_vidinfo.IsYV411())
			csConsoleOut = "YV411";
		else if (AVS_vidinfo.IsYV24())
			csConsoleOut = " YV24";
		else if (AVS_vidinfo.IsYV16())
			csConsoleOut = " YV16";
		else if (AVS_vidinfo.IsY8())
			csConsoleOut = "   Y8";
		else if (AVS_vidinfo.IsYV12())
			csConsoleOut = " YV12";
		else if (AVS_vidinfo.IsYUY2())
			csConsoleOut = " YUY2";
		else if (AVS_vidinfo.IsRGB24())
			csConsoleOut = "RGB24";
		else if (AVS_vidinfo.IsRGB32())
			csConsoleOut = "RGB32";
		else
			csConsoleOut = "  n/a";

		printf("%s\n", csConsoleOut.c_str());
		csLogBuffer += csConsoleOut + "\n";

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
				string csLogRet = CreateLogFile(csAVSFile, csLogBuffer, PerformanceData, uiPDIndex, csAVSError, uiFrameInterval);
				if (csLogRet != "")
					printf(csLogRet.c_str());
			}

			SleepUntilEsc();
			return 0;
		}

		printf("\n");

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
			printf("\r%s\r", bstr.StrPad("Please wait...", uiConsoleWidth).c_str());
		}
		else
		{
			printf("\r%s", bstr.StrPad("", uiConsoleWidth).c_str());
			csConsoleOut = bstr.StrFormat("Invalid frame range specified (%d,%d), using default", settings.iStartFrame, settings.iStopFrame);
			printf("\r%s\r", csConsoleOut.c_str());
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
					csConsoleOut = bstr.StrFormat("Frame (current | last):         %u | %u", uiCurrentFrame, uiLastFrame);
					printf("\r%s\n", bstr.StrPad(csConsoleOut, uiConsoleWidth).c_str());
					csConsoleOut = "FPS (cur | min | max | avg):    " + FormatFPS(dFPSCurrent, dFPSMin, dFPSMax, dFPSAverage);
					printf("\r%s\n", bstr.StrPad(csConsoleOut, uiConsoleWidth).c_str());
					csConsoleOut = bstr.StrFormat("CPU usage (current | average):  %u%% | %u%%", process_info.wCPUUsage, uiCPUUsageAvg);
					printf("\r%s\n", bstr.StrPad(csConsoleOut, uiConsoleWidth).c_str());
					csConsoleOut = bstr.StrFormat("Thread count:                   %u", process_info.wThreadCount);
					printf("\r%s\n", bstr.StrPad(csConsoleOut, uiConsoleWidth).c_str());
					csConsoleOut = bstr.StrFormat("Physical Memory usage:          %u MB", dwPhysicalMemCurrentMB);
					printf("\r%s\n", bstr.StrPad(csConsoleOut, uiConsoleWidth).c_str());
					csConsoleOut = bstr.StrFormat("Virtual Memory usage:           %u MB", dwVirtualMemCurrentMB);
					printf("\r%s\n", bstr.StrPad(csConsoleOut, uiConsoleWidth).c_str());
					csConsoleOut = bstr.StrFormat("Time (elapsed | estimated):     %s | %s", bstr.FormatTimeString(uiElapsedMS).c_str(), bstr.FormatTimeString(uiEstimatedMS).c_str());
					printf("\r%s\n", bstr.StrPad(csConsoleOut, uiConsoleWidth).c_str());

					printf("\nPress ESC to stop...\n\n");

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

		csLogBuffer += "\n\n[Runtime info]\n";

		if (n >= uiFrameInterval)
		{
			if (!bFirstScr)
				utils.CursorUp(10);

			csConsoleOut = bstr.StrFormat("Frames processed:               %u (%u - %u)", n, uiFirstFrame, uiLastFrame);
			printf("\r%s\n", bstr.StrPad(csConsoleOut, uiConsoleWidth).c_str());
			csLogBuffer += csConsoleOut + "\n";

			csConsoleOut = "FPS (min | max | average):      " + FormatFPS(-1.0, dFPSMin, dFPSMax, dFPSAverage);
			printf("\r%s\n", bstr.StrPad(csConsoleOut, uiConsoleWidth).c_str());
			csLogBuffer += csConsoleOut + "\n";

			csConsoleOut = bstr.StrFormat("CPU usage (average):            %u%%", uiCPUUsageAvg);
			printf("\r%s\n", bstr.StrPad(csConsoleOut, uiConsoleWidth).c_str());
			csLogBuffer += csConsoleOut + "\n";

			csConsoleOut = bstr.StrFormat("Thread count:                   %u", process_info.wThreadCount);
			printf("\r%s\n", bstr.StrPad(csConsoleOut, uiConsoleWidth).c_str());
			csLogBuffer += csConsoleOut + "\n";

			csConsoleOut = bstr.StrFormat("Physical Memory usage (peak):   %u MB", dwPhysicalMemPeakMB);
			printf("\r%s\n", bstr.StrPad(csConsoleOut, uiConsoleWidth).c_str());
			csLogBuffer += csConsoleOut + "\n";

			csConsoleOut = bstr.StrFormat("Virtual Memory usage (peak):    %u MB", dwVirtualMemPeakMB);
			printf("\r%s\n", bstr.StrPad(csConsoleOut, uiConsoleWidth).c_str());
			csLogBuffer += csConsoleOut + "\n";

			if ((settings.bLogEstimatedTime) && (n < uiFramesToProcess))
				csConsoleOut = bstr.StrFormat("Time (elapsed | estimated):     %s | %s", bstr.FormatTimeString(uiElapsedMS).c_str(), bstr.FormatTimeString(uiEstimatedMS).c_str());
			else
				csConsoleOut = bstr.StrFormat("Time (elapsed):                 %s", bstr.FormatTimeString(uiElapsedMS).c_str());

			printf("\r%s\n", bstr.StrPad(csConsoleOut, uiConsoleWidth).c_str());
			csLogBuffer += csConsoleOut + "\n";

			printf("\r%s\n", bstr.StrPad("", uiConsoleWidth).c_str());
			printf("\r%s\n", bstr.StrPad("", uiConsoleWidth).c_str());
			utils.CursorUp(2);
		}
		else
		{
			if (!bFirstScr)
			{
				for (int i = 0; i <= 9; i++)
				{
					printf("\r%s", bstr.StrPad("", uiConsoleWidth).c_str());
					utils.CursorUp(1);
				}
			}
			else
				printf("\r%s", bstr.StrPad("", uiConsoleWidth).c_str());

			csLogBuffer += "Insufficient data\n";
		}
	}
	catch(AvisynthError err)
	{
		csAVSError = bstr.StrFormat("%s", (PCSTR)err.msg);

		if (bEarlyExit == FALSE)
		{
			utils.CursorUp(2);
			printf("\r%s\n", bstr.StrPad("", uiConsoleWidth).c_str());
			printf("\r%s\n", bstr.StrPad("", uiConsoleWidth).c_str());
			utils.CursorUp(2);
			printf("\r%s\n", csAVSError.c_str());
		}
		else
			printf("\n%s\n", csAVSError.c_str());
	}

	#if defined(AVS26_32) || defined(AVS26_64)
		AVS_linkage = 0;
	#endif

	::FreeLibrary(hDLL);

	if (settings.bCreateLog)
	{
		string csRet = CreateLogFile(csAVSFile, csLogBuffer, PerformanceData, uiPDIndex, csAVSError, uiFrameInterval);
		if (csRet != "")
		{
			printf(csRet.c_str());
			SleepUntilEsc();
			return -1;
		}
	}

	if (bPerfDataInit)
		delete[] PerformanceData;

	SleepUntilEsc();

	return 0;
}


string ParseINIFile()
{
	string csRet = "";
	string csProgramPath = "";
	string csINIFile = "";
	string csTemp = "";

	char szPath[2050];
	if (::GetModuleFileName(NULL, szPath, 2048) > 0)
	{
		csProgramPath = bstr.StrFormat("%s", szPath);
		size_t i = 0;
		for (i = (csProgramPath.length() - 1); i > 0; i--)
		{
		 if (csProgramPath[i] == '\\')
			break;
		}
		csProgramPath = csProgramPath.substr(0, i);
		csINIFile = csProgramPath + "\\AVSMeter.ini";
	}


	if (!utils.FileExists(csINIFile)) //write defaults
	{
		ofstream hINIFile(csINIFile.c_str());
		if (!hINIFile.is_open())
	  {
			csRet = bstr.StrFormat("\nCannot create \"%s\"\n", csINIFile.c_str());
			return csRet;
		}
		hINIFile << "CreateLog=0\n";
		hINIFile << "PauseBeforeExit=0\n";
		hINIFile << "FrameRange=0,-1\n";
		hINIFile << "InvokeDistributor=1\n";
		hINIFile << "LogEstimatedTime=0\n";
		hINIFile.flush();
		hINIFile.close();
	}

	ifstream hINIFile(csINIFile.c_str());
	if (!hINIFile.is_open())
	{
		csRet = bstr.StrFormat("\nCannot open \"%s\"\n", csINIFile.c_str());
		return csRet;
	}

	settings.bCreateLog = FALSE;
	settings.bPauseBeforeExit = FALSE;
	settings.iStartFrame = 0;
	settings.iStopFrame = -1;
	settings.bInvokeDistributor = TRUE;
	settings.bLogEstimatedTime = FALSE;

	string csCurrentLine = "";
	size_t iPos = 0;

	while (getline(hINIFile, csCurrentLine))
	{
		bstr.StrToLC(csCurrentLine);
		bstr.StrTrim(csCurrentLine);
		csCurrentLine.erase(remove(csCurrentLine.begin(), csCurrentLine.end(), ' '), csCurrentLine.end());

		if ((csCurrentLine.substr(0, 9) == "createlog") && (csCurrentLine.substr(csCurrentLine.length() - 1) == "1"))
			settings.bCreateLog = TRUE;
		if ((csCurrentLine.substr(0, 15) == "pausebeforeexit") && (csCurrentLine.substr(csCurrentLine.length() - 1) == "1"))
			settings.bPauseBeforeExit = TRUE;
		if ((csCurrentLine.substr(0, 17) == "invokedistributor") && (csCurrentLine.substr(csCurrentLine.length() - 1) == "0"))
			settings.bInvokeDistributor = FALSE;
		if ((csCurrentLine.substr(0, 16) == "logestimatedtime") && (csCurrentLine.substr(csCurrentLine.length() - 1) == "1"))
			settings.bLogEstimatedTime = TRUE;
		if (csCurrentLine.substr(0, 10) == "framerange")
		{
			iPos = csCurrentLine.find("=");
			if (iPos == 10)
			{
				csTemp = csCurrentLine.substr(iPos + 1);
				iPos = csTemp.find(",");
				if (iPos > 0)
				{
					settings.iStartFrame = atoi(csTemp.substr(0, iPos).c_str());
					settings.iStopFrame = atoi(csTemp.substr(iPos + 1).c_str());
				}
			}
		}
	}

	hINIFile.close();

	return csRet;
}


string CreateLogFile(string &cs_avsfile, string &cs_logbuffer, perfdata * cs_pdata, unsigned int ui_index, string &cs_avserror, unsigned int ui_frameinterval)
{
	string csRet = "";
	string csAVSBuffer = "";
	string csCurrentLine = "";

	ifstream hAVSFile(cs_avsfile.c_str());
	if (!hAVSFile.is_open())
	{
		csRet = bstr.StrFormat("\nCannot open \"%s\"\n", cs_avsfile.c_str());
		return csRet;
	}

	while (getline(hAVSFile, csCurrentLine))
		csAVSBuffer += csCurrentLine + "\n";

	hAVSFile.close();

	string csLogFile = cs_avsfile + ".log";
	ofstream hLogFile(csLogFile.c_str());
	if (!hLogFile.is_open())
	{
		csRet = bstr.StrFormat("\nCannot create \"%s\"\n", csLogFile.c_str());
		return csRet;
	}

	string csLog = "";

	csLog = cs_logbuffer + "\n";
	hLogFile << csLog;
	csLog = "\n[Script]\n" + csAVSBuffer + "\n";
	hLogFile << csLog;

	if (ui_index > 0)
	{
		if (ui_frameinterval == 1)
			csLog = "\n[Performance data]\n       Frame   Frames/sec   Time/frame(ms)   CPU(%)   Threads   PhysMEM(MB)   VirtMEM(MB)\n";
		else
			csLog = "\n[Performance data]\n       Frame interval   Frames/sec   Time/frame(ms)   CPU(%)   Threads   PhysMEM(MB)   VirtMEM(MB)\n";

		hLogFile << csLog;

		string stemp1 = "";
		string stemp2 = "";
		if (ui_index > 0)
		{
			for (unsigned int i = 0; i < ui_index; i++)
			{
				if (ui_frameinterval == 1)
				{
					stemp1 = bstr.StrFormat("%u", cs_pdata[i].frame_low);
					string spad((12 - stemp1.length()), ' ');
					stemp2 = bstr.StrFormat("%s%u %12.2f %16.6f %8u %9u %13u %13u", spad.c_str(), cs_pdata[i].frame_low, 1.0 / cs_pdata[i].frame_interval_time, cs_pdata[i].frame_interval_time * 1000.0, cs_pdata[i].cpu_usage, cs_pdata[i].num_threads, cs_pdata[i].phys_mem, cs_pdata[i].virt_mem);
				}
				else
				{
					stemp1 = bstr.StrFormat("%u-%u", cs_pdata[i].frame_low, cs_pdata[i].frame_high);
					string spad((21 - stemp1.length()), ' ');
					stemp2 = bstr.StrFormat("%s%u-%u %12.2f %16.6f %8u %9u %13u %13u", spad.c_str(), cs_pdata[i].frame_low, cs_pdata[i].frame_high, 1.0 / cs_pdata[i].frame_interval_time, cs_pdata[i].frame_interval_time * 1000.0, cs_pdata[i].cpu_usage, cs_pdata[i].num_threads, cs_pdata[i].phys_mem, cs_pdata[i].virt_mem);
				}

				hLogFile << stemp2 + "\n";
			}
		}
	}

	if (cs_avserror != "")
	{
		hLogFile << "\n\n[AVISynth error]\n" + cs_avserror + "\n";
	}

	hLogFile.flush();
	hLogFile.close();

	return csRet;
}


void SleepUntilEsc()
{
	if (settings.bPauseBeforeExit)
	{
		printf("\nPress ESC to exit...");
		for (;;)
		{
			Sleep(100);
			if (_kbhit())
			{
				if (_getch() == 0x1B) //ESC
					break;
			}
		}
		printf("\r%s", bstr.StrPad("", utils.GetConsoleWidth()).c_str());
	}

	return;
}


unsigned int CalculateFrameInterval(string &avs_file, string &cs_avserror)
{
	unsigned int uiFI = 1000;
	cs_avserror = "";

	HINSTANCE hDLL = ::LoadLibrary("avisynth");
	if (!hDLL)
	{
		cs_avserror = "Failed to load avisynth.dll";
		return 0;
	}

	try
	{
		IScriptEnvironment *(__stdcall *CreateEnvironment)(int) = (IScriptEnvironment *(__stdcall *)(int))::GetProcAddress(hDLL, "CreateScriptEnvironment");
		if (!CreateEnvironment)
		{
			cs_avserror = "Failed to load CreateScriptEnvironment()";
			::FreeLibrary(hDLL);
			return 0;
		}

		IScriptEnvironment *AVS_env = CreateEnvironment(AVISYNTH_INTERFACE_VERSION);
		if (!AVS_env)
		{
			cs_avserror = "Could not create IScriptenvironment";
			::FreeLibrary(hDLL);
			return 0;
		}

		#if defined(AVS26_32) || defined(AVS26_64)
			AVS_linkage = AVS_env->GetAVSLinkage();
		#endif

		AVSValue AVS_main;
		AVS_main = AVS_env->Invoke("Import", avs_file.c_str());

		if (!AVS_main.IsClip()) //not a clip
			AVS_env->ThrowError("Script did not return a video clip:\n(%s)", avs_file.c_str());

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
			AVS_env->ThrowError("Script did not return a video clip:\n(%s)", avs_file.c_str());

		BOOL bTesting = TRUE;
		unsigned int uiCurrentFrame = 0;
		double dT1 = utils.GetTimer();
		double dT2 = dT1;
		while (bTesting)
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
		cs_avserror = bstr.StrFormat("%s", (PCSTR)err.msg);
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
		printf("\nUsage:  AVSMeter script.avs [switches]\n\n");
		printf("Switches:\n");
		printf("-i      Display clip info only\n");
		printf("-l      Create log file\n");

		return;
}


string FormatFPS(double d_FPSCurrent, double d_FPSMin, double d_FPSMax, double d_FPSAverage)
{
	string sRet = "";
	string sTemp = "";

	if (d_FPSCurrent > 0.0)
	{
		if (d_FPSCurrent < 10000.0)
			sTemp = bstr.StrFormat("%.2f", d_FPSCurrent);
		else
			sTemp = bstr.StrFormat("%u", (unsigned int)d_FPSCurrent);

		sRet += sTemp;
	}

	if (d_FPSMin > 0.0)
	{
		if (d_FPSMin < 10000.0)
			sTemp = bstr.StrFormat("%.2f", d_FPSMin);
		else
			sTemp = bstr.StrFormat("%u", (unsigned int)d_FPSMin);

		if (d_FPSCurrent > 0.0)
			sRet += " | ";

		sRet += sTemp;
	}

	if (d_FPSMax > 0.0)
	{
		if (d_FPSMax < 10000.0)
			sTemp = bstr.StrFormat("%.2f", d_FPSMax);
		else
			sTemp = bstr.StrFormat("%u", (unsigned int)d_FPSMax);

		if (d_FPSMin > 0.0)
			sRet += " | ";

		sRet += sTemp;
	}

	if (d_FPSAverage > 0.0)
	{
		if (d_FPSAverage < 10000.0)
			sTemp = bstr.StrFormat("%.2f", d_FPSAverage);
		else
			sTemp = bstr.StrFormat("%u", (unsigned int)d_FPSAverage);

		if (d_FPSMax > 0.0)
			sRet += " | ";

		sRet += sTemp;
	}


	return sRet;
}


