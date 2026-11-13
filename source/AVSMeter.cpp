// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include "stdafx.h"
#include "utility.h"
#include "ProcessInfo.h"
#include "GPUInfo.h"
#include "timer.h"

#ifdef AVS64
	#include "avisynth26.h"
	const AVS_Linkage *AVS_linkage = 0;
#else
	#include "avisynth25.h"
#endif

#define COLOR_DEFAULT           0
#define COLOR_AVSM_VERSION      FG_HRED | BG_BLACK
#define COLOR_AVISYNTH_VERSION  FG_HGREEN | BG_BLACK
#define COLOR_EMPHASIS          FG_WHITE | BG_BLACK
#define COLOR_ERROR             FG_HCYAN | BG_BLACK

#define STD_OUT 0
#define STD_ERR 1

struct stAvisynthInfo
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


struct stSettings
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
} Settings;

typedef struct stPerformanceData
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

typedef struct stThreadParameters
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
void PrintFmt(BYTE bStream, WORD wAttributes, const char *fmt, ...); //bStream: 0 = stdout, 1 = stderr


int main(int argc, char* argv[])
{
	Settings.bPauseBeforeExit = TRUE;
	Settings.bUseColor = TRUE;

	int iRet = 0;
	string sAVSMVersion = utils.GetFileVersion("");

	#ifdef AVS64
		sAVSMVersion += " (x64)";
	#else
		sAVSMVersion += " (x86)";
	#endif

	string sRet = ParseINIFile();
	if (sRet != "")
	{
		PrintFmt(STD_ERR, COLOR_ERROR, sRet.c_str());
		Pause();
		return -1;
	}

	PrintFmt(STD_ERR, COLOR_AVSM_VERSION, "\nAVSMeter %s by Groucho2004\n", sAVSMVersion.c_str());

	string sOutBuf = "";
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
			PrintFmt(STD_ERR, COLOR_ERROR, "\nCannot load avisynth.dll:\n%s\n", utils.OSErrorMessage().c_str());
			Pause();
			return -1;
		}

		if (!AvisynthInfo.bIsCompatible)
		{
			#ifdef AVS64
				PrintFmt(STD_ERR, COLOR_ERROR, "\nCannot load avisynth.dll.\nMake sure you are using a 64 Bit version.\n");
			#else
				PrintFmt(STD_ERR, COLOR_ERROR, "\nCannot load avisynth.dll.\nMake sure you are using a 32 Bit version.\n");
			#endif

			Pause();
			return -1;
		}

		if (AvisynthInfo.sErrorMsg != "")
		{
			PrintFmt(STD_ERR, COLOR_ERROR, "\n%s\n", AvisynthInfo.sErrorMsg.c_str());
			Pause();
			return -1;
		}
	}

	if (!AvisynthInfo.bAVSLinkage)
	{
		#ifdef AVS64
			PrintFmt(STD_ERR, COLOR_ERROR, "\nAvisynth 2.6 x64 is required.\n");
			Pause();
			return -1;
		#endif
	}

	PrintFmt(STD_ERR, COLOR_AVISYNTH_VERSION, "\r%s", AvisynthInfo.sVersionString.c_str());
	if (AvisynthInfo.bIsMTVersion)
		PrintFmt(STD_ERR, COLOR_EMPHASIS, " (%s) (MT)\n", AvisynthInfo.sVersion.c_str());
	else
		PrintFmt(STD_ERR, COLOR_EMPHASIS, " (%s)\n", AvisynthInfo.sVersion.c_str());

	if (Settings.bAllowOnlyOneInstance)
	{
		CreateMutex(NULL, TRUE, "__avsmeter__single__instance__lock__");
		if (GetLastError() == ERROR_ALREADY_EXISTS)
		{
			PrintFmt(STD_ERR, COLOR_ERROR, "\nOne instance of AVSMeter is already running\n");
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

	string sTemp = "";
	for (int iArg = 1; iArg < argc; iArg++)
	{
		sArg = argv[iArg];
		sArgTest = sArg;
		utils.StrToLC(sArgTest);
		utils.StrTrim(sArgTest);

		if ((sArgTest == "-h") || (sArgTest == "-help"))
		{
			PrintUsage();
			Pause();
			return -1;
		}

		if ((sArgTest == "-l") || (sArgTest == "-log"))
		{
			Settings.bCreateLog = TRUE;
			continue;
		}

		if ((sArgTest == "-i") || (sArgTest == "-info"))
		{
			bInfoOnly = TRUE;
			continue;
		}

		if (sArgTest == "-gpu")
		{
			Settings.bMonitorGPULoad = TRUE;
			continue;
		}

		if (sArgTest.substr(0, 7) == "-range=")
		{
			Settings.iStartFrame = 0xFFFFFFFFFF;
			Settings.iStopFrame = 0xFFFFFFFFFF;

			sTemp = sArgTest.substr(7);
			if (sTemp.length() < 3)
			{
				PrintFmt(STD_ERR, COLOR_ERROR, "\nInvalid parameter format: \"%s\"\n", sArg.c_str());
				Pause();
				return -1;
			}

			size_t spos = sTemp.find(",");
			if ((spos < 1) || (spos > 27))
			{
				PrintFmt(STD_ERR, COLOR_ERROR, "\nInvalid parameter format: \"%s\"\n", sArg.c_str());
				Pause();
				return -1;
			}

			Settings.sFrameRange = "FrameRange=" + sTemp;

			if ((utils.IsNumeric(sTemp.substr(0, spos))) && (utils.IsNumeric(sTemp.substr(spos + 1))))
			{
				Settings.iStartFrame = _atoi64(sTemp.substr(0, spos).c_str());
				Settings.iStopFrame = _atoi64(sTemp.substr(spos + 1).c_str());
			}
			else
			{
				PrintFmt(STD_ERR, COLOR_ERROR, "\nInvalid parameter format: \"%s\"\n", sArg.c_str());
				Pause();
				return -1;
			}
			continue;
		}

		if (sArgTest.substr(0, 9) == "-timeout=")
		{
			sTemp = sArgTest.substr(9);
			if (utils.IsNumeric(sTemp))
			{
				Settings.wReadTimeout = (WORD)atol(sTemp.c_str());
				if ((Settings.wReadTimeout < 10) || (Settings.wReadTimeout > 3600))
				{
					PrintFmt(STD_ERR, COLOR_ERROR, "\nInvalid parameter value: \"%s\"\nValue for timeout must be between 10 and 3600\n", sArg.c_str());
					Pause();
					return -1;
				}
			}
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
						PrintFmt(STD_ERR, COLOR_ERROR, "\nFile not found: \"%s\"\n", sArg.c_str());
						Pause();
						return -1;
					}
					else
						continue;
				}
			}
		}

		PrintFmt(STD_ERR, COLOR_ERROR, "\nInvalid argument: \"%s\"\n", sArg.c_str());
		Pause();
		return -1;
	}

	if (sAVSFile == "")
	{
		PrintFmt(STD_ERR, COLOR_ERROR, "\nNo script file specified\n");
		Pause();
		return -1;
	}

	CGPUInfo gpuinfo;

	if (Settings.bMonitorGPULoad)
	{
		if (!gpuinfo.bGPUZIsRunning)
		{
			PrintFmt(STD_ERR, COLOR_ERROR, "\nIn order to measure GPU usage, GPU-Z must be running.\nStart GPU-Z and try again.\n");
			Pause();
			return -1;
		}
		else
		{
			if (!gpuinfo.MemInit())
			{
				PrintFmt(STD_ERR, COLOR_ERROR, "\nCannot initialize GPU-Z shared memory:\n%s", gpuinfo.sError.c_str());
				Pause();
				return -1;
			}
		}
	}


	if (!bInfoOnly)
	{
		sAVSError = "";

		ThreadParams* p = new ThreadParams;
		p->bFinished = FALSE;
		p->sAVSError = sAVSError;
		p->sAVSFile = sAVSFile;

		double dT0 = timer.GetSTDTimer();
		double dT1 = dT0;
		char szAnimation[] = "|/-\\";

		_beginthread(CalculateFrameInterval, 0, p);

		while (!p->bFinished)
		{
			for (unsigned int n = 0; n < 4; n++)
			{
				dT1 = timer.GetSTDTimer();

				if ((dT1 - dT0) > (double)Settings.wReadTimeout)
				{
					PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
					PrintFmt(STD_ERR, COLOR_ERROR, "\nProcess aborted while reading frame #%u after specified timeout (%u sec).\n", p->uiCurFrame, Settings.wReadTimeout);
					Pause();
					return -1;
				}

				if (_kbhit())
				{
					if (_getch() == 0x1B) //ESC
					{
						PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s", utils.StrPad("Cancelled.").c_str());
						PrintFmt(STD_ERR, COLOR_DEFAULT, "\n");
						Pause();
						return -1;
					}
				}

				PrintFmt(STD_ERR, COLOR_DEFAULT, "\rAnalysing script %c\r", szAnimation[n]);

				if (p->bFinished)
					break;

				Sleep(250);
			}
		}

		sAVSError = p->sAVSError;
		uiFrameInterval = p->uiFrameInterval;

		PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
		if ((uiFrameInterval == 0) || (sAVSError != ""))
		{
			PrintFmt(STD_ERR, COLOR_ERROR, "\n%s\n", sAVSError.c_str());
			Pause();
			return -1;
		}
	}


	HINSTANCE hDLL = ::LoadLibrary("avisynth");
	if (!hDLL)
	{
		PrintFmt(STD_ERR, COLOR_ERROR, "\nCannot load avisynth.dll\n");
		Pause();
		return -1;
	}

	try
	{
		IScriptEnvironment *(__stdcall *CreateEnvironment)(int) = (IScriptEnvironment *(__stdcall *)(int))::GetProcAddress(hDLL, "CreateScriptEnvironment");
		if (!CreateEnvironment)
		{
			PrintFmt(STD_ERR, COLOR_ERROR, "\nFailed to load CreateScriptEnvironment()\n");
			Pause();
			return -1;
		}

		IScriptEnvironment *AVS_env = CreateEnvironment(AVISYNTH_INTERFACE_VERSION);
		if (!AVS_env)
		{
			PrintFmt(STD_ERR, COLOR_ERROR, "\nCould not create IScriptenvironment\n");
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
			AVS_env->ThrowError("\"%s\":\nScript did not return a video clip", sAVSFile.c_str());
		}

		BOOL bIsMTVersion = TRUE;
		int iMTMode = 0;
		try
		{
			AVS_temp = AVS_env->Invoke("GetMTMode", false);
			iMTMode = AVS_temp.IsInt() ? AVS_temp.AsInt() : 0;
			if ((iMTMode > 0) && (iMTMode < 5) && Settings.bInvokeDistributor)
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
		sOutBuf = utils.StrFormat("Log file created with:   AVSMeter %s", sAVSMVersion.c_str());
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("Avisynth version:        %s", AvisynthInfo.sVersionString.c_str());
		if (AvisynthInfo.bIsMTVersion)
			sOutBuf += utils.StrFormat(" (%s) (MT)", AvisynthInfo.sVersion.c_str());
		else
			sOutBuf += utils.StrFormat(" (%s)", AvisynthInfo.sVersion.c_str());

		sLogBuffer += sOutBuf + "\n";

		PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());

		if (bIsMTVersion)
		{
			sOutBuf = utils.StrFormat("Active MT Mode: %d", iMTMode);
			PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s\n", sOutBuf.c_str());
			sLogBuffer += "                         " + sOutBuf + "\n";
		}

		if (Settings.bMonitorGPULoad)
		{
			sLogBuffer += "\n\n[Graphics card info]\n";

			sOutBuf = utils.StrFormat("Card name:               %s", gpuinfo.data.CardName.c_str());
			sLogBuffer += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("GPU name:                %s", gpuinfo.data.GPUName.c_str());
			sLogBuffer += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("Bus interface:           %s", gpuinfo.data.BusInterface.c_str());
			sLogBuffer += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("Memory size:             %s", gpuinfo.data.MemSize.c_str());
			sLogBuffer += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("OpenCL version:          %s", gpuinfo.data.OpenCLVersion.c_str());
			sLogBuffer += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("Driver version:          %s", gpuinfo.data.DriverVersion.c_str());
			sLogBuffer += sOutBuf + "\n";
		}


		sLogBuffer += "\n\n[Clip info]\n";

		sOutBuf = utils.StrFormat("Number of frames:          %11u", AVS_vidinfo.num_frames);
		PrintFmt(STD_ERR, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("Length (hh:mm:ss.ms):  %s", timer.FormatTimeString(iMilliSeconds, TRUE).c_str());
		PrintFmt(STD_ERR, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("Frame width:               %11u", AVS_vidinfo.width);
		PrintFmt(STD_ERR, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("Frame height:              %11u", AVS_vidinfo.height);
		PrintFmt(STD_ERR, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("Framerate:                 %11.3f (%u/%u)", (double)AVS_vidinfo.fps_numerator / (double)AVS_vidinfo.fps_denominator, AVS_vidinfo.fps_numerator, AVS_vidinfo.fps_denominator);
		PrintFmt(STD_ERR, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = "Colorspace:                    ";
		PrintFmt(STD_ERR, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
		sLogBuffer += sOutBuf;

		if (AVS_vidinfo.IsYV411())
			sOutBuf = "  YV411";
		else if (AVS_vidinfo.IsYV24())
			sOutBuf = "   YV24";
		else if (AVS_vidinfo.IsYV16())
			sOutBuf = "   YV16";
		else if (AVS_vidinfo.IsY8())
			sOutBuf = "     Y8";
		else if (AVS_vidinfo.IsYV12())
			sOutBuf = "   YV12";
		else if (AVS_vidinfo.IsYUY2())
			sOutBuf = "   YUY2";
		else if (AVS_vidinfo.IsRGB24())
			sOutBuf = "  RGB24";
		else if (AVS_vidinfo.IsRGB32())
			sOutBuf = "  RGB32";
		else
			sOutBuf = "Unknown";

		PrintFmt(STD_ERR, COLOR_DEFAULT, "%s\n", sOutBuf.c_str());
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = "Audio channels:    ";
		sLogBuffer += sOutBuf;
		if (AVS_vidinfo.AudioChannels())
			sOutBuf = utils.StrFormat("%19d", AVS_vidinfo.AudioChannels());
		else
			sOutBuf = "                n/a";

		sLogBuffer += sOutBuf + "\n";

		sOutBuf = "Audio bits/sample: ";
		sLogBuffer += sOutBuf;
		if (AVS_vidinfo.SampleType())
			sOutBuf = utils.StrFormat("%19d", AVS_vidinfo.SampleType());
		else
			sOutBuf = "                n/a";

		sLogBuffer += sOutBuf + "\n";

		sOutBuf = "Audio sample rate: ";
		sLogBuffer += sOutBuf;
		if (AVS_vidinfo.SamplesPerSecond())
			sOutBuf = utils.StrFormat("%19d", AVS_vidinfo.SamplesPerSecond());
		else
			sOutBuf = "                n/a";

		sLogBuffer += sOutBuf + "\n";

		sOutBuf = "Audio samples:     ";
		sLogBuffer += sOutBuf;
		if (AVS_vidinfo.num_audio_samples)
			sOutBuf = utils.StrFormat("%19I64d", AVS_vidinfo.num_audio_samples);
		else
			sOutBuf = "                n/a";

		sLogBuffer += sOutBuf + "\n";


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

			if (Settings.bCreateLog)
			{
				PerformanceData = new perfdata[1];
				string sLogRet = CreateLogFile(sAVSFile, sLogBuffer, PerformanceData, uiPDIndex, sAVSError, uiFrameInterval);
				if (sLogRet != "")
					PrintFmt(STD_ERR, COLOR_ERROR, sLogRet.c_str());

				delete [] PerformanceData;
			}

			#ifdef AVS64
				AVS_linkage = 0;
			#endif
	
			::FreeLibrary(hDLL);

			Pause();
			return 0;
		}

		PrintFmt(STD_ERR, COLOR_DEFAULT, "\n");

		unsigned int uiFramesToProcess = 0;
		unsigned int uiFirstFrame = 0;
		unsigned int uiLastFrame = uiFrames - 1;

		if (Settings.iStopFrame == -1)
			Settings.iStopFrame = (__int64)uiFrames - 1;

		if ((Settings.iStartFrame >= 0) && (Settings.iStartFrame <= Settings.iStopFrame) && (Settings.iStopFrame < (__int64)uiFrames))
		{
			uiFirstFrame = (unsigned int)Settings.iStartFrame;
			uiLastFrame = (unsigned int)Settings.iStopFrame;
			uiFramesToProcess = uiLastFrame - uiFirstFrame + 1;
			if (uiFramesToProcess > 1)
				PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s\r", utils.StrPad("Reading frames...").c_str());
			else
				PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s\r", utils.StrPad("Reading frame...").c_str());
		}
		else
		{
			PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
			PrintFmt(STD_ERR, COLOR_ERROR, "\rInvalid frame range specified in INI file:\n", sOutBuf.c_str());
			PrintFmt(STD_ERR, COLOR_ERROR, "\r\"%s\"\n", Settings.sFrameRange.c_str());
			PrintFmt(STD_ERR, COLOR_EMPHASIS, "\rProceed with default (%u,%u)? [y/n]", uiFirstFrame, uiLastFrame);

			int iKey = 0;
			for (;;)
			{
				Sleep(100);
				if (_kbhit())
				{
					PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
					utils.CursorUp(1);
					PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
					utils.CursorUp(1);
					PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());

					iKey = _getch();
					if ((iKey == 0x59) || (iKey == 0x79))
					{
						PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s\r", utils.StrPad("Reading frame(s)...").c_str());
						break;
					}
					else
					{
						PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s\r", utils.StrPad("").c_str());
						int IVersion = AVS_clip->GetVersion();
						AVS_clip = 0;
						AVS_main = 0;
						AVS_temp = 0;
			
						if (IVersion >= 5)
							AVS_env->DeleteScriptEnvironment();
						else
							AVS_env->~IScriptEnvironment();
			
						if (Settings.bCreateLog)
						{
							PerformanceData = new perfdata[1];
							string sLogRet = CreateLogFile(sAVSFile, sLogBuffer, PerformanceData, uiPDIndex, sAVSError, uiFrameInterval);
							if (sLogRet != "")
								PrintFmt(STD_ERR, COLOR_DEFAULT, sLogRet.c_str());
			
							delete [] PerformanceData;
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

		if (Settings.bMonitorGPULoad)
			gpuinfo.ReadSensors();

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

				if (Settings.bMonitorGPULoad)
				{
					gpuinfo.ReadSensors();
					uiGPUUsageAcc += (unsigned int)gpuinfo.sensors.GPULoad;
					uiGPUUsageAvg = (unsigned int)(((double)uiGPUUsageAcc / (double)uiCounter) + 0.5);
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
					if (Settings.bMonitorGPULoad)
						PerformanceData[uiPDIndex].gpu_usage = gpuinfo.sensors.GPULoad;
					else
						PerformanceData[uiPDIndex].gpu_usage = 0;
					PerformanceData[uiPDIndex].num_threads = process_info.wThreadCount;
					PerformanceData[uiPDIndex].phys_mem = dwPhysicalMemCurrentMB;
					PerformanceData[uiPDIndex].virt_mem = dwVirtualMemCurrentMB;
					++uiPDIndex;

					dLastIntervalTime = dCurrentTime;
				}

				if ((dCurrentTime - dLastDisplayTime) >= (1.0 / 3.0))
				{
					dLastDisplayTime = dCurrentTime;

					if (!bFirstScr)
					{
						if (Settings.bMonitorGPULoad)
							utils.CursorUp(10);
						else
							utils.CursorUp(9);
					}

					bFirstScr = FALSE;
					sOutBuf = utils.StrFormat("Frame (current | last):         %u | %u", uiCurrentFrame, uiLastFrame);
					PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					sOutBuf = utils.StrFormat("FPS (cur | min | max | avg):    %s | %s | %s | %s", utils.StrFormatFPS(dFPSCurrent).c_str(), utils.StrFormatFPS(dFPSMin).c_str(), utils.StrFormatFPS(dFPSMax).c_str(), utils.StrFormatFPS(dFPSAverage).c_str());
					PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					sOutBuf = utils.StrFormat("CPU usage (current | average):  %u%% | %u%%", process_info.wCPUUsage, uiCPUUsageAvg);
					PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());

					if (Settings.bMonitorGPULoad)
					{
						sOutBuf = utils.StrFormat("GPU usage (current | average):  %u%% | %u%%", gpuinfo.sensors.GPULoad, uiGPUUsageAvg);
						PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					}

					sOutBuf = utils.StrFormat("Thread count:                   %u", process_info.wThreadCount);
					PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					sOutBuf = utils.StrFormat("Memory usage (phys | virt):     %u | %u MB", dwPhysicalMemCurrentMB, dwVirtualMemCurrentMB);
					PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					sOutBuf = utils.StrFormat("Time (elapsed | estimated):     %s | %s", timer.FormatTimeString(iElapsedMS, FALSE).c_str(), timer.FormatTimeString(iEstimatedMS, FALSE).c_str());
					PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());

					PrintFmt(STD_ERR, COLOR_DEFAULT, "\nPress \'Esc\' to cancel the process...\n\n");

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
		if (Settings.bMonitorGPULoad)
			gpuinfo.MemRelease();

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
			{
				if (Settings.bMonitorGPULoad)
					utils.CursorUp(10);
				else
					utils.CursorUp(9);
			}

			if (uiFirstFrame == uiLastFrame)
			{
				if (uiFirstFrame == 0)
					sOutBuf = utils.StrFormat("Frames processed:               %u", n, uiFirstFrame);
				else
					sOutBuf = utils.StrFormat("Frames processed:               %u (%u)", n, uiFirstFrame);
			}
			else
				sOutBuf = utils.StrFormat("Frames processed:               %u (%u - %u)", n, uiFirstFrame, uiLastFrame);

			PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			sLogBuffer += sOutBuf + "\n";

			sOutBuf = utils.StrFormat("FPS (min | max | average):      %s | %s | %s", utils.StrFormatFPS(dFPSMin).c_str(), utils.StrFormatFPS(dFPSMax).c_str(), utils.StrFormatFPS(dFPSAverage).c_str());
			PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			sLogBuffer += sOutBuf + "\n";

			sOutBuf = utils.StrFormat("CPU usage (average):            %u%%", uiCPUUsageAvg);
			PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			sLogBuffer += sOutBuf + "\n";

			if (Settings.bMonitorGPULoad)
			{
				sOutBuf = utils.StrFormat("GPU usage (average):            %u%%", uiGPUUsageAvg);
				PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
				sLogBuffer += sOutBuf + "\n";
			}

			sOutBuf = utils.StrFormat("Thread count:                   %u", process_info.wThreadCount);
			PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			sLogBuffer += sOutBuf + "\n";

			sOutBuf = utils.StrFormat("Memory usage (phys | virt):     %u | %u MB", dwPhysicalMemPeakMB, dwVirtualMemPeakMB);
			PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			sLogBuffer += sOutBuf + "\n";

			if ((Settings.bLogEstimatedTime) && (n < uiFramesToProcess))
				sOutBuf = utils.StrFormat("Time (elapsed | estimated):     %s | %s", timer.FormatTimeString(iElapsedMS, FALSE).c_str(), timer.FormatTimeString(iEstimatedMS, FALSE).c_str());
			else
				sOutBuf = utils.StrFormat("Time (elapsed):                 %s", timer.FormatTimeString(iElapsedMS, FALSE).c_str());

			PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			sLogBuffer += sOutBuf + "\n";

			PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s\n", utils.StrPad("").c_str());
			PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s\n", utils.StrPad("").c_str());
			utils.CursorUp(2);
		}
		else
		{
			if (!bFirstScr)
			{
				for (int i = 0; i <= 9; i++)
				{
					PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
					utils.CursorUp(1);
				}
			}

			sOutBuf = "Insufficient data for measurements";
			PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			sLogBuffer += sOutBuf;
		}
	}
	catch (AvisynthError err)
	{
		Settings.bCreateLog = TRUE;
		sAVSError = utils.StrFormat("%s", (PCSTR)err.msg);
		iRet = -1;

		
		if (bEarlyExit == FALSE)
		{
			utils.CursorUp(2);
			PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s\n", utils.StrPad("").c_str());
			PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s\n", utils.StrPad("").c_str());
			utils.CursorUp(2);
			PrintFmt(STD_ERR, COLOR_ERROR, "\r%s\n", sAVSError.c_str());
		}
		else
			PrintFmt(STD_ERR, COLOR_ERROR, "\n%s\n", sAVSError.c_str());
	}
	catch (...)
	{
		Settings.bCreateLog = TRUE;
		iRet = -1;
		if (bEarlyExit == FALSE)
		{
			utils.CursorUp(2);
			PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s\n", utils.StrPad("").c_str());
			PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s\n", utils.StrPad("").c_str());
			utils.CursorUp(2);
		}
		PrintFmt(STD_ERR, COLOR_ERROR, "\nUnknown exception\n");
	}

	#ifdef AVS64
		AVS_linkage = 0;
	#endif

	::FreeLibrary(hDLL);

	if (Settings.bCreateLog)
	{
		string sRet = CreateLogFile(sAVSFile, sLogBuffer, PerformanceData, uiPDIndex, sAVSError, uiFrameInterval);
		if (sRet != "")
		{
			PrintFmt(STD_ERR, COLOR_ERROR, sRet.c_str());
			Pause();
			return -1;
		}
	}

	delete [] PerformanceData;

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

	Settings.bCreateLog = FALSE;
	Settings.bPauseBeforeExit = FALSE;
	Settings.iStartFrame = 0;
	Settings.iStopFrame = -1;
	Settings.bInvokeDistributor = TRUE;
	Settings.bAllowOnlyOneInstance = TRUE;
	Settings.bLogEstimatedTime = FALSE;
	Settings.bUseColor = TRUE;
	Settings.bMonitorGPULoad = FALSE;
	Settings.wReadTimeout = 120;

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
				Settings.bCreateLog = TRUE;
		}

		if (sCurrentLine.substr(0, 15) == "pausebeforeexit")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "1")
				Settings.bPauseBeforeExit = TRUE;
		}

		if (sCurrentLine.substr(0, 17) == "invokedistributor")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "0")
				Settings.bInvokeDistributor = FALSE;
		}

		if (sCurrentLine.substr(0, 20) == "allowonlyoneinstance")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "0")
				Settings.bAllowOnlyOneInstance = FALSE;
		}

		if (sCurrentLine.substr(0, 16) == "logestimatedtime")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "1")
				Settings.bLogEstimatedTime = TRUE;
		}

		if (sCurrentLine.substr(0, 14) == "monitorgpuload")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "1")
				Settings.bMonitorGPULoad = TRUE;
		}

		if (sCurrentLine.substr(0, 8) == "usecolor")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "0")
				Settings.bUseColor = FALSE;
		}

		if (sCurrentLine.substr(0, 12) == "readtimeout=")
		{
			sTemp = sCurrentLine.substr(12);
			if (utils.IsNumeric(sTemp))
			{
				Settings.wReadTimeout = (WORD)atol(sTemp.c_str());
				if ((Settings.wReadTimeout < 10) || (Settings.wReadTimeout > 3600))
					Settings.wReadTimeout = 120;
			}
		}

		if (sCurrentLine.substr(0, 11) == "framerange=")
		{
			Settings.sFrameRange = sOrgLine;
			Settings.iStartFrame = 0xFFFFFFFFFF;
			Settings.iStopFrame = 0xFFFFFFFFFF;

			sTemp = sCurrentLine.substr(11);
			if (sTemp.length() < 3)
				continue;

			spos = sTemp.find(",");
			if ((spos < 1) || (spos > 27))
				continue;

			if ((utils.IsNumeric(sTemp.substr(0, spos))) && (utils.IsNumeric(sTemp.substr(spos + 1))))
			{
				Settings.iStartFrame = _atoi64(sTemp.substr(0, spos).c_str());
				Settings.iStopFrame = _atoi64(sTemp.substr(spos + 1).c_str());
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
	if (Settings.bPauseBeforeExit)
	{
		PrintFmt(STD_ERR, COLOR_DEFAULT, "\nPress any key to exit...");
		for (;;)
		{
			Sleep(100);
			if (_kbhit())
				break;
		}
		PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
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
		p->sAVSError = "QueryPerformanceCounter() is not supported";
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
			if ((iMTMode > 0) && (iMTMode < 5) && Settings.bInvokeDistributor)
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

		p->uiFrameInterval = 10000;
		double dAverageFrameTime = 0.0;
		if (TDelta > 0.2)
			dAverageFrameTime = (1000.0 * TDelta) / (double)uif; //ms

		if (dAverageFrameTime > 0.0010)
			p->uiFrameInterval = 5000;
		if (dAverageFrameTime > 0.0025)
			p->uiFrameInterval = 2000;
		if (dAverageFrameTime > 0.0050)
			p->uiFrameInterval = 1000;
		if (dAverageFrameTime > 0.0100)
			p->uiFrameInterval = 500;
		if (dAverageFrameTime > 0.0250)
			p->uiFrameInterval = 200;
		if (dAverageFrameTime > 0.0500)
			p->uiFrameInterval = 100;
		if (dAverageFrameTime > 0.1000)
			p->uiFrameInterval = 50;
		if (dAverageFrameTime > 0.2500)
			p->uiFrameInterval = 20;
		if (dAverageFrameTime > 0.5000)
			p->uiFrameInterval = 10;
		if (dAverageFrameTime > 5.0000)
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
		AvisynthInfo.sVersion = utils.GetFileVersion(AvisynthInfo.sDLLPath);
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


void PrintFmt(BYTE bStream, WORD wAttributes, const char *fmt, ...)  //bStream: 0 = stdout, 1 = stderr
{
	if (!fmt)	return;

	va_list args;
	va_start(args, fmt);

	if (wAttributes)
		utils.SetRGB(wAttributes, Settings.bUseColor);

	if (bStream == 0)
		vfprintf(stdout, fmt, args);
	if (bStream == 1)
		vfprintf(stderr, fmt, args);

	if (wAttributes)
		utils.ResetRGB(Settings.bUseColor);

	va_end(args);

	return;
}


void PrintUsage()
{
	PrintFmt(STD_OUT, COLOR_EMPHASIS, "\nUsage:  AVSMeter script.avs [switches]\n\n");
	PrintFmt(STD_OUT, COLOR_EMPHASIS, "Switches:\n");
	PrintFmt(STD_OUT, COLOR_EMPHASIS, " -info [-i]           Display clip info only\n");
	PrintFmt(STD_OUT, COLOR_EMPHASIS, " -log  [-l]           Create log file\n");
	PrintFmt(STD_OUT, COLOR_EMPHASIS, " -gpu                 Monitor GPU usage\n");
	PrintFmt(STD_OUT, COLOR_EMPHASIS, " -range=first,last    Specify frame range\n");
	PrintFmt(STD_OUT, COLOR_EMPHASIS, " -timeout=n           Specify timeout (seconds)\n");
	PrintFmt(STD_OUT, COLOR_EMPHASIS, " -help [-h]           Display command line usage\n");

	PrintFmt(STD_OUT, COLOR_DEFAULT, "\n");
	return;
}


