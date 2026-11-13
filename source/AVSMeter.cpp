/*
	This file is part of AVSMeter,
	Copyright(C) Groucho2004.

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

#include "stdafx.h"
#include "AvisynthInfo.h"
#include "utility.h"
#include "ProcessInfo.h"
#include "GPUInfo.h"
#include "timer.h"

#define COLOR_DEFAULT           0
#define COLOR_AVSM_VERSION      FG_HRED | BG_BLACK
#define COLOR_AVISYNTH_VERSION  FG_HGREEN | BG_BLACK
#define COLOR_EMPHASIS          FG_WHITE | BG_BLACK
#define COLOR_ERROR             FG_HCYAN | BG_BLACK

#define STD_OUT 0
#define STD_ERR 1

#define REFRESH_INTERVAL 0.25

class CSettings
{
public:
	BOOL      bCreateLog;
	BOOL      bPauseBeforeExit;
	__int64   iStartFrame;
	__int64   iStopFrame;
	string    sFrameRange;
	__int64   iTimeLimit;
	BOOL      bInvokeDistributor;
	BOOL      bAllowOnlyOneInstance;
	BOOL      bLogEstimatedTime;
	BOOL      bUseColor;
	BOOL      bUseFixedFrameInterval;
	BOOL      bMonitorGPULoad;
	BYTE      bProcessPriority;
};

struct PerformanceData
{
	unsigned int  frame_low;
	unsigned int  frame_high;
	double        fps;
	WORD          cpu_usage;
	DWORD         phys_mem;
	DWORD         virt_mem;
	WORD          num_threads;
};


CAvisynthInfo AvisynthInfo;
CUtils utils;
CTimer timer;
CSettings Settings;

unsigned int CalculateFrameInterval(string &s_avsfile, string &s_avserror);
string CreateLogFile(string &s_avsfile, string &s_logbuffer, string &s_gpuinfo, PerformanceData * cs_pdata, unsigned int ui_index, string &s_avserror, unsigned int ui_frameinterval);
string ParseINIFile();
void PrintUsage();
void Pause();
void PrintFmt(BYTE bStream, WORD wAttributes, const char *fmt, ...); //bStream: 0 = stdout, 1 = stderr


int main(int argc, char* argv[])
{
	//some defaults
	Settings.bPauseBeforeExit = TRUE;
	Settings.bUseColor = TRUE;
	Settings.iTimeLimit = -1;
	Settings.bUseFixedFrameInterval = FALSE;
	Settings.bProcessPriority = 2;
	Settings.bMonitorGPULoad = FALSE;

	int iRet = 0;
	string sAVSMVersion = utils.GetFileVersion("");
	if (sAVSMVersion.length() > 5)
		sAVSMVersion = sAVSMVersion.substr(0, 5);

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

	PrintFmt(STD_ERR, COLOR_AVSM_VERSION, "\nAVSMeter %s\n", sAVSMVersion.c_str());

	string sOutBuf = "";
	string sCommandLine = "";
	string sCmdToken = "";
	string sArg = "";
	string sArgTest = "";
	string sAVSFile = "";
	string sLogBuffer = "";
	string sGPUInfo = "";
	PerformanceData * perfdata = 0;
	unsigned int uiPDIndex = 0;
	BOOL bEarlyExit = TRUE;
	BOOL bInfoOnly = FALSE;
	string sAVSError = "";
	unsigned int uiFrameInterval = 0;

	string sErrorMsg = "";
	BOOL bRet = AvisynthInfo.GetAvisynthInfo(sErrorMsg);

	if (!bRet)
	{
		if (AvisynthInfo.bLoadError)
		{
			PrintFmt(STD_ERR, COLOR_ERROR, "\n%s\n", sErrorMsg.c_str());
			Pause();
			return -1;
		}

		#ifdef AVS64
			if (!AvisynthInfo.bIs64BitVersion)
			{
				PrintFmt(STD_ERR, COLOR_ERROR, "\nAVSMeter (x64) cannot load a 32 Bit avisynth.dll.\n");
				Pause();
				return -1;
			}
		#else
			if (AvisynthInfo.bIs64BitVersion)
			{
				PrintFmt(STD_ERR, COLOR_ERROR, "\nAVSMeter (x86) cannot load a 64 Bit avisynth.dll.\n");
				Pause();
				return -1;
			}
		#endif

		if (sErrorMsg != "")
			PrintFmt(STD_ERR, COLOR_ERROR, "\n%s\n", sErrorMsg.c_str());
		else
			PrintFmt(STD_ERR, COLOR_ERROR, "\nCannot load avisynth.dll\n%s\n", utils.OSErrorMessage().c_str());

		Pause();
		return -1;
	}

	PrintFmt(STD_ERR, COLOR_AVISYNTH_VERSION, "\r%s (%s)\n", AvisynthInfo.sVersionString.c_str(), AvisynthInfo.sVersion.c_str());

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
		utils.StrTrim(sArgTest);
		sCmdToken = sArgTest;
		utils.StrToLC(sArgTest);

		if ((sArgTest == "-h") || (sArgTest == "-help"))
		{
			PrintUsage();
			Pause();
			return -1;
		}

		if ((sArgTest == "-l") || (sArgTest == "-log"))
		{
			sCommandLine += sCmdToken + " ";
			Settings.bCreateLog = TRUE;
			continue;
		}

		if ((sArgTest == "-i") || (sArgTest == "-info"))
		{
			sCommandLine += sCmdToken + " ";
			bInfoOnly = TRUE;
			continue;
		}

		if ((sArgTest == "-fixed") || (sArgTest == "-f"))
		{
			sCommandLine += sCmdToken + " ";
			Settings.bUseFixedFrameInterval = TRUE;
			continue;
		}

		if (sArgTest == "-gpu")
		{
			sCommandLine += sCmdToken + " ";
			Settings.bMonitorGPULoad = TRUE;
			continue;
		}

		if (sArgTest.substr(0, 7) == "-range=")
		{
			sCommandLine += sCmdToken + " ";
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

		if (sArgTest.substr(0, 10) == "-priority=")
		{
			sCommandLine += sCmdToken + " ";
			sTemp = sArgTest.substr(10);
			if (utils.IsNumeric(sTemp))
			{
				Settings.bProcessPriority = (BYTE)atol(sTemp.c_str());
				if ((Settings.bProcessPriority < 1) || (Settings.bProcessPriority > 3))
				{
					PrintFmt(STD_ERR, COLOR_ERROR, "\nInvalid parameter value: \"%s\"\nValue must be between 1 and 3\n", sArg.c_str());
					Pause();
					return -1;
				}
			}
			else
			{
				PrintFmt(STD_ERR, COLOR_ERROR, "\nInvalid parameter value: \"%s\"\nValue must be between 1 and 3\n", sArg.c_str());
				Pause();
				return -1;
			}

			continue;
		}

		if (sArgTest.substr(0, 11) == "-timelimit=")
		{
			sCommandLine += sCmdToken + " ";
			sTemp = sArgTest.substr(11);
			if (utils.IsNumeric(sTemp))
			{
				Settings.iTimeLimit = atoi(sTemp.c_str());
				if ((Settings.iTimeLimit < 1) || (Settings.iTimeLimit > 999999))
				{
					PrintFmt(STD_ERR, COLOR_ERROR, "\nInvalid parameter value: \"%s\"\nValue must be between 1 and 999999\n", sArg.c_str());
					Pause();
					return -1;
				}
			}
			else
			{
				PrintFmt(STD_ERR, COLOR_ERROR, "\nInvalid parameter value: \"%s\"\nValue must be between 1 and 999999\n", sArg.c_str());
				Pause();
				return -1;
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

	utils.StrTrim(sCommandLine);

	if (sAVSFile == "")
	{
		PrintFmt(STD_ERR, COLOR_ERROR, "\nNo script file specified\n");
		Pause();
		return -1;
	}


	if (Settings.bProcessPriority == 1)
		::SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
	else if (Settings.bProcessPriority == 3)
		::SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	else
		::SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);


	CGPUInfo gpuinfo;
	if (Settings.bMonitorGPULoad)
	{
		if (!gpuinfo.bGPUZIsRunning)
		{
			PrintFmt(STD_ERR, COLOR_ERROR, "\nIn order to measure GPU usage, GPU-Z must be running.\n");
			PrintFmt(STD_ERR, COLOR_EMPHASIS, "Continue without GPU monitoring? [y/n]");

			for (;;)
			{
				Sleep(10);
				if (_kbhit())
				{
					int iKey = _getch();
					if ((iKey == 0x59) || (iKey == 0x79)) //y
					{
						PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
						utils.CursorUp(1);
						PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
						utils.CursorUp(1);
						Settings.bMonitorGPULoad = FALSE;
						break;
					}
					else if ((iKey == 0x4E) || (iKey == 0x6E)) //n
					{
						PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
						utils.CursorUp(1);
						PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
						utils.CursorUp(1);
						Pause();
						return -1;
					}
					else
						continue;
				}
			}
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
		uiFrameInterval = 10;
		if (!Settings.bUseFixedFrameInterval)
		{
			PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s\r", utils.StrPad("Analysing script...").c_str());
			uiFrameInterval = CalculateFrameInterval(sAVSFile, sAVSError);

			PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s\r", utils.StrPad("").c_str());

			if ((uiFrameInterval == 0) || (sAVSError != ""))
			{
				PrintFmt(STD_ERR, COLOR_ERROR, "\n%s\n", sAVSError.c_str());
				Pause();
				return -1;
			}
		}
		else
		{
			if (!timer.TestTimer())
			{
				PrintFmt(STD_ERR, COLOR_ERROR, "\nQueryPerformanceCounter() is not supported\n");
				Pause();
				return -1;
			}
		}
	}


	HINSTANCE hDLL = ::LoadLibrary("avisynth");
	if (!hDLL)
	{
		PrintFmt(STD_ERR, COLOR_ERROR, "\nCannot load avisynth.dll\n%s\n", utils.OSErrorMessage().c_str());
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

		IScriptEnvironment *AVS_env = CreateEnvironment(AvisynthInfo.iInterfaceVersion);
		if (!AVS_env)
		{
			PrintFmt(STD_ERR, COLOR_ERROR, "\nCould not create IScriptenvironment\n");
			Pause();
			return -1;
		}

		AVS_linkage = AVS_env->GetAVSLinkage();
		AVSValue AVS_main;
		AVSValue AVS_temp;
		PClip AVS_clip;
		VideoInfo	AVS_vidinfo;

		PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s\r", utils.StrPad("Loading script...").c_str());
		AVS_main = AVS_env->Invoke("Import", sAVSFile.c_str());
		PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s\r", utils.StrPad("").c_str());

		if (!AVS_main.IsClip())
			AVS_env->ThrowError("\"%s\":\nScript did not return a clip", sAVSFile.c_str());

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

		BOOL bAudioOnly = FALSE;
		if (!AVS_vidinfo.HasVideo() && AVS_vidinfo.HasAudio())
			bAudioOnly = TRUE;

		unsigned int uiFrames = 0;
		__int64 iMilliSeconds = 0;
		if (!bAudioOnly)
		{
			uiFrames = (unsigned int)AVS_vidinfo.num_frames;
			iMilliSeconds = (__int64)((((double)uiFrames * (double)AVS_vidinfo.fps_denominator * 1000.0) / (double)AVS_vidinfo.fps_numerator) + 0.5);
		}

		sLogBuffer += "[General info]\n";
		sOutBuf = utils.StrFormat("Log file created with:   AVSMeter %s", sAVSMVersion.c_str());
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("Script file:             %s", sAVSFile.c_str());
		sLogBuffer += sOutBuf + "\n";

		if (sCommandLine != "")
		{
			sOutBuf = utils.StrFormat("Command line switches:   %s", sCommandLine.c_str());
			sLogBuffer += sOutBuf + "\n";
		}

		sOutBuf = utils.StrFormat("Avisynth version:        %s (%s)", AvisynthInfo.sVersionString.c_str(), AvisynthInfo.sVersion.c_str());
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("Avisynth DLL location:   %s", AvisynthInfo.sDLLPath.c_str());
		sLogBuffer += sOutBuf + "\n";

		if (AvisynthInfo.sPluginDir != "")
		{
			sOutBuf = utils.StrFormat("Plugin directory:        %s", AvisynthInfo.sPluginDir.c_str());
			sLogBuffer += sOutBuf + "\n";
		}

		PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());

		if (bIsMTVersion)
		{
			sOutBuf = utils.StrFormat("Active MT Mode: %d", iMTMode);
			PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s\n", sOutBuf.c_str());
			sLogBuffer += "                         " + sOutBuf + "\n";
		}

		if (Settings.bMonitorGPULoad)
		{
			sGPUInfo += "\n\n[Graphics card info]\n";

			sOutBuf = utils.StrFormat("Card name:               %s", gpuinfo.data.CardName.c_str());
			sGPUInfo += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("GPU name:                %s", gpuinfo.data.GPUName.c_str());
			sGPUInfo += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("Memory size:             %s", gpuinfo.data.MemSize.c_str());
			sGPUInfo += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("OpenCL version:          %s", gpuinfo.data.OpenCLVersion.c_str());
			sGPUInfo += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("Driver version:          %s", gpuinfo.data.DriverVersion.c_str());
			sGPUInfo += sOutBuf + "\n\n";
		}

		sLogBuffer += "\n\n[Clip info]\n";

		if (!bAudioOnly)
			sOutBuf = utils.StrFormat("Number of frames:          %11u", AVS_vidinfo.num_frames);
		else
			sOutBuf = "Number of frames:                  n/a";

		PrintFmt(STD_ERR, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
		sLogBuffer += sOutBuf + "\n";

		if (!bAudioOnly)
			sOutBuf = utils.StrFormat("Length (hh:mm:ss.ms):  %s", timer.FormatTimeString(iMilliSeconds, TRUE).c_str());
		else
			sOutBuf = "Length (hh:mm:ss.ms):              n/a";

		PrintFmt(STD_ERR, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
		sLogBuffer += sOutBuf + "\n";

		if (!bAudioOnly)
			sOutBuf = utils.StrFormat("Frame width:               %11u", AVS_vidinfo.width);
		else
			sOutBuf = "Frame width:                       n/a";

		PrintFmt(STD_ERR, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
		sLogBuffer += sOutBuf + "\n";

		if (!bAudioOnly)
			sOutBuf = utils.StrFormat("Frame height:              %11u", AVS_vidinfo.height);
		else
			sOutBuf = "Frame height:                      n/a";

		PrintFmt(STD_ERR, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
		sLogBuffer += sOutBuf + "\n";

		if (!bAudioOnly)
			sOutBuf = utils.StrFormat("Framerate:                 %11.3f (%u/%u)", (double)AVS_vidinfo.fps_numerator / (double)AVS_vidinfo.fps_denominator, AVS_vidinfo.fps_numerator, AVS_vidinfo.fps_denominator);
		else
			sOutBuf = "Framerate:                         n/a";
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
		else if (!AVS_vidinfo.HasVideo())
			sOutBuf = "    n/a";
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
			AVS_clip = 0;
			AVS_main = 0;
			AVS_temp = 0;
			AVS_env->DeleteScriptEnvironment();
			AVS_env = 0;

			if (Settings.bCreateLog)
			{
				perfdata = new PerformanceData[1];
				string sLogRet = CreateLogFile(sAVSFile, sLogBuffer, sGPUInfo, perfdata, uiPDIndex, sAVSError, uiFrameInterval);
				if (sLogRet != "")
					PrintFmt(STD_ERR, COLOR_ERROR, sLogRet.c_str());

				delete [] perfdata;
			}

			AVS_linkage = 0;
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
			PrintFmt(STD_ERR, COLOR_ERROR, "\rInvalid frame range specified:\n");
			PrintFmt(STD_ERR, COLOR_ERROR, "\r\"%s\"\n", Settings.sFrameRange.c_str());
			PrintFmt(STD_ERR, COLOR_EMPHASIS, "\rProceed with default (%u,%u)? [y/n]", uiFirstFrame, uiLastFrame);

			for (;;)
			{
				Sleep(100);
				if (_kbhit())
				{
					int iKey = _getch();
					if ((iKey == 0x59) || (iKey == 0x79)) //y
					{
						PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
						utils.CursorUp(1);
						PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
						utils.CursorUp(1);
						PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
						PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s\r", utils.StrPad("Reading frame(s)...").c_str());
						break;
					}
					else if ((iKey == 0x4E) || (iKey == 0x6E)) //n
					{
						PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
						utils.CursorUp(1);
						PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
						utils.CursorUp(1);
						PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
						PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s\r", utils.StrPad("").c_str());
						AVS_clip = 0;
						AVS_main = 0;
						AVS_temp = 0;
						AVS_env->DeleteScriptEnvironment();
						AVS_env = 0;
			
						if (Settings.bCreateLog)
						{
							perfdata = new PerformanceData[1];
							string sLogRet = CreateLogFile(sAVSFile, sLogBuffer, sGPUInfo, perfdata, uiPDIndex, sAVSError, uiFrameInterval);
							if (sLogRet != "")
								PrintFmt(STD_ERR, COLOR_DEFAULT, sLogRet.c_str());
			
							delete [] perfdata;
						}
			
						AVS_linkage = 0;
						::FreeLibrary(hDLL);
			
						Pause();
						return 0;
					}
					else
						continue;
				}
			}

			uiFirstFrame = 0;
			uiLastFrame = uiFrames - 1;
			uiFramesToProcess = uiFrames;
		}

		if ((Settings.bUseFixedFrameInterval) && (uiFramesToProcess <= 10))
			uiFrameInterval = 1;

		while ((uiFramesToProcess / uiFrameInterval) > 200000)
			uiFrameInterval *= 10;

		unsigned int uiArraySize = (uiFramesToProcess / uiFrameInterval) + 2;

		perfdata = new PerformanceData[uiArraySize];
		for (unsigned int x = 0; x < uiArraySize; x++)
		{
			perfdata[x].frame_low = 0;
			perfdata[x].frame_high = 0;
			perfdata[x].fps = 0.0;
			perfdata[x].cpu_usage = 0;
			perfdata[x].phys_mem = 0;
			perfdata[x].virt_mem = 0;
			perfdata[x].num_threads = 0;
		}

		bEarlyExit = FALSE;
		unsigned int n = 0;
		BOOL bFirstScr = TRUE;
		__int64 iElapsedMS = 0;
		__int64 iEstimatedMS = 0;

		CProcessInfo processinfo;
		DWORD dwPhysicalMemPeakMB = 0;
		DWORD dwVirtualMemPeakMB = 0;
		DWORD dwPhysicalMemCurrentMB = 0;
		DWORD dwVirtualMemCurrentMB = 0;
		unsigned int uiCPUUsageAcc = 0;
		unsigned int uiCPUUsageAvg = 0;
		unsigned int uiGPUUsageAcc = 0;
		unsigned int uiGPUUsageAvg = 0;
		unsigned int uiVPUUsageAcc = 0;
		unsigned int uiVPUUsageAvg = 0;
		unsigned int uiCounter = 0;

		unsigned int uiCurrentFrame = 0;
		double dFPSAverage = 0.0;
		double dFPSCurrent = 0.0;
		double dFPSMin = 1.0e+20;
		double dFPSMax = 0.0;

		if (Settings.bMonitorGPULoad)
			gpuinfo.ReadSensors();

		processinfo.Update();

		double dStartTime = timer.GetTimer();
		double dCurrentTime = dStartTime;
		double dLastDisplayTime = dStartTime;
		double dLastIntervalTime = dStartTime;

		unsigned int uiCursorOffset = 0;

		for (uiCurrentFrame = uiFirstFrame; uiCurrentFrame <= uiLastFrame; uiCurrentFrame++)
		{
			PVideoFrame src_frame = AVS_clip->GetFrame(uiCurrentFrame, AVS_env);
			++n;

			if (((n % uiFrameInterval) != 0) && (n != uiFramesToProcess))
				continue;

			dCurrentTime = timer.GetTimer();
			processinfo.Update();
			++uiCounter;

			if (Settings.bMonitorGPULoad)
			{
				gpuinfo.ReadSensors();
				uiGPUUsageAcc += (unsigned int)gpuinfo.sensors.GPULoad;
				uiGPUUsageAvg = (unsigned int)(((double)uiGPUUsageAcc / (double)uiCounter) + 0.5);
				uiVPUUsageAcc += (unsigned int)gpuinfo.sensors.VPULoad;
				uiVPUUsageAvg = (unsigned int)(((double)uiVPUUsageAcc / (double)uiCounter) + 0.5);
			}

			iElapsedMS = (__int64)(((dCurrentTime - dStartTime) * 1000.0) + 0.5);
			iEstimatedMS = (__int64)((double)uiFramesToProcess * (double)iElapsedMS / (double)n);

			uiCPUUsageAcc += (unsigned int)processinfo.wCPUUsage;
			uiCPUUsageAvg = (unsigned int)(((double)uiCPUUsageAcc / (double)uiCounter) + 0.5);

			dwPhysicalMemCurrentMB = processinfo.dwPhysicalMemMB;
			dwVirtualMemCurrentMB = processinfo.dwVirtualMemMB;

			if (dwPhysicalMemCurrentMB > dwPhysicalMemPeakMB)
				dwPhysicalMemPeakMB = dwPhysicalMemCurrentMB;
			if (dwVirtualMemCurrentMB > dwVirtualMemPeakMB)
				dwVirtualMemPeakMB = dwVirtualMemCurrentMB;

			dFPSAverage = (double)n / (dCurrentTime - dStartTime);

			if ((n % uiFrameInterval) != 0)
				continue;

			if ((dCurrentTime - dLastIntervalTime) > 0.000001)
				dFPSCurrent = (double)uiFrameInterval / (dCurrentTime - dLastIntervalTime);
			else
				dFPSCurrent = (double)uiFrameInterval / 0.000001;

			if (dFPSCurrent > dFPSMax)
				dFPSMax = dFPSCurrent;
			if (dFPSCurrent < dFPSMin)
				dFPSMin = dFPSCurrent;

			perfdata[uiPDIndex].frame_low = (uiCurrentFrame + 1) - uiFrameInterval;
			perfdata[uiPDIndex].frame_high = uiCurrentFrame;
			perfdata[uiPDIndex].fps = dFPSCurrent;
			perfdata[uiPDIndex].cpu_usage = processinfo.wCPUUsage;

			perfdata[uiPDIndex].num_threads = processinfo.wThreadCount;
			perfdata[uiPDIndex].phys_mem = dwPhysicalMemCurrentMB;
			perfdata[uiPDIndex].virt_mem = dwVirtualMemCurrentMB;
			++uiPDIndex;

			dLastIntervalTime = dCurrentTime;

			if ((dCurrentTime - dLastDisplayTime) < REFRESH_INTERVAL)
				continue;

			dLastDisplayTime = dCurrentTime;

			if (!bFirstScr)
			{
				utils.CursorUp(uiCursorOffset);
				uiCursorOffset = 0;
			}

			bFirstScr = FALSE;

			sOutBuf = utils.StrFormat("Frame (current | last):         %u | %u", uiCurrentFrame, uiLastFrame);
			PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			++uiCursorOffset;

			sOutBuf = utils.StrFormat("FPS (cur | min | max | avg):    %s | %s | %s | %s", utils.StrFormatFPS(dFPSCurrent).c_str(), utils.StrFormatFPS(dFPSMin).c_str(), utils.StrFormatFPS(dFPSMax).c_str(), utils.StrFormatFPS(dFPSAverage).c_str());
			PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			++uiCursorOffset;

			sOutBuf = utils.StrFormat("Memory usage (phys | virt):     %u | %u MB", dwPhysicalMemCurrentMB, dwVirtualMemCurrentMB);
			PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			++uiCursorOffset;

			sOutBuf = utils.StrFormat("Thread count:                   %u", processinfo.wThreadCount);
			PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			++uiCursorOffset;

			sOutBuf = utils.StrFormat("CPU usage (current | average):  %u%% | %u%%", processinfo.wCPUUsage, uiCPUUsageAvg);
			PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			++uiCursorOffset;

			if (Settings.bMonitorGPULoad)
			{
				PrintFmt(STD_ERR, COLOR_DEFAULT, "\n");
				++uiCursorOffset;

				sOutBuf = utils.StrFormat("GPU usage (current | average):  %u%% | %u%%", gpuinfo.sensors.GPULoad, uiGPUUsageAvg);
				PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
				++uiCursorOffset;

				if (gpuinfo.data.NVVP)
				{
					sOutBuf = utils.StrFormat("Video engine load (cur | avg):  %u%% | %u%%", gpuinfo.sensors.VPULoad, uiVPUUsageAvg);
					PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					++uiCursorOffset;
				}

				if (gpuinfo.data.GeneralMem)
				{
					sOutBuf = utils.StrFormat("GPU memory usage:               %u MB", gpuinfo.sensors.MemoryUsedGeneral);
					PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					++uiCursorOffset;
				}

				if (gpuinfo.data.DedicatedMem)
				{
					sOutBuf = utils.StrFormat("GPU memory usage (Dedicated):   %u MB", gpuinfo.sensors.MemoryUsedDedicated);
					PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					++uiCursorOffset;
				}

				if (gpuinfo.data.DynamicMem)
				{
					sOutBuf = utils.StrFormat("GPU memory usage (Dynamic):     %u MB", gpuinfo.sensors.MemoryUsedDynamic);
					PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					++uiCursorOffset;
				}
			}

			PrintFmt(STD_ERR, COLOR_DEFAULT, "\n");
			++uiCursorOffset;

			sOutBuf = utils.StrFormat("Time (elapsed | estimated):     %s | %s", timer.FormatTimeString(iElapsedMS, FALSE).c_str(), timer.FormatTimeString(iEstimatedMS, FALSE).c_str());
			PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			++uiCursorOffset;

			PrintFmt(STD_ERR, COLOR_DEFAULT, "\nPress \'Esc\' to cancel the process...\n\n");
			++uiCursorOffset;
			++uiCursorOffset;
			++uiCursorOffset;


			if (Settings.iTimeLimit != -1)
			{
				if (iElapsedMS >= (Settings.iTimeLimit * 1000))
				{
					uiLastFrame = uiCurrentFrame;
					break;
				}
			}

			if (_kbhit())
			{
				if (_getch() == 0x1B) //ESC
				{
					uiLastFrame = uiCurrentFrame;
					break;
				}
			}
		}

		processinfo.CloseProcess();

		if (Settings.bMonitorGPULoad)
			gpuinfo.MemRelease();

		AVS_clip = 0;
		AVS_main = 0;
		AVS_temp = 0;

		AVS_env->DeleteScriptEnvironment();
		AVS_env = 0;

		sLogBuffer += "\n\n[Runtime info]\n";

		if (n >= uiFrameInterval)
		{
			if (!bFirstScr)
				utils.CursorUp(uiCursorOffset);

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

			sOutBuf = utils.StrFormat("Memory usage (phys | virt):     %u | %u MB", dwPhysicalMemPeakMB, dwVirtualMemPeakMB);
			PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			sLogBuffer += sOutBuf + "\n";

			sOutBuf = utils.StrFormat("Thread count:                   %u", processinfo.wThreadCount);
			PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			sLogBuffer += sOutBuf + "\n";

			sOutBuf = utils.StrFormat("CPU usage (average):            %u%%", uiCPUUsageAvg);
			PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			sLogBuffer += sOutBuf + "\n";

			if (Settings.bMonitorGPULoad)
			{
				PrintFmt(STD_ERR, COLOR_DEFAULT, "\n");

				sOutBuf = utils.StrFormat("GPU usage (average):            %u%%", uiGPUUsageAvg);
				PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
				sLogBuffer += sOutBuf + "\n";

				if (gpuinfo.data.NVVP)
				{
					sOutBuf = utils.StrFormat("Video engine load (average):    %u%%", uiVPUUsageAvg);
					PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					sLogBuffer += sOutBuf + "\n";
				}

				if (gpuinfo.data.GeneralMem)
				{
					sOutBuf = utils.StrFormat("GPU memory usage:               %u MB", gpuinfo.sensors.MemoryUsedGeneral);
					PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					sLogBuffer += sOutBuf + "\n";
				}

				if (gpuinfo.data.DedicatedMem)
				{
					sOutBuf = utils.StrFormat("GPU memory usage (Dedicated):   %u MB", gpuinfo.sensors.MemoryUsedDedicated);
					PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					sLogBuffer += sOutBuf + "\n";
				}

				if (gpuinfo.data.DynamicMem)
				{
					sOutBuf = utils.StrFormat("GPU memory usage (Dynamic):     %u MB", gpuinfo.sensors.MemoryUsedDynamic);
					PrintFmt(STD_ERR, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					sLogBuffer += sOutBuf + "\n";
				}
			}

			PrintFmt(STD_ERR, COLOR_DEFAULT, "\n");

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
		PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s\r", utils.StrPad("").c_str());
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
		PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s\r", utils.StrPad("").c_str());
		Settings.bCreateLog = TRUE;
		iRet = -1;
		if (bEarlyExit == FALSE)
		{
			utils.CursorUp(2);
			PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s\n", utils.StrPad("").c_str());
			PrintFmt(STD_ERR, COLOR_DEFAULT, "\r%s\n", utils.StrPad("").c_str());
			utils.CursorUp(2);
		}

		string sError = utils.OSErrorMessage();
		if (sError == "")
			PrintFmt(STD_ERR, COLOR_ERROR, "\nUnknown exception\n");
		else
			PrintFmt(STD_ERR, COLOR_ERROR, "\n%s\n", sError.c_str());
	}

	AVS_linkage = 0;
	::FreeLibrary(hDLL);

	if (Settings.bCreateLog)
	{
		string sRet = CreateLogFile(sAVSFile, sLogBuffer, sGPUInfo, perfdata, uiPDIndex, sAVSError, uiFrameInterval);
		if (sRet != "")
		{
			PrintFmt(STD_ERR, COLOR_ERROR, sRet.c_str());
			Pause();
			return -1;
		}
	}

	delete [] perfdata;

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

	string sAVSMVersion = utils.GetFileVersion("");
	if (sAVSMVersion.length() > 5)
		sAVSMVersion = sAVSMVersion.substr(0, 5);

	string sCurrentLine = "";
	BOOL bNewVersion = TRUE;

	ifstream hTestINIFile(sINIFile.c_str());
	if (utils.FileExists(sINIFile))
	{
		if (!hTestINIFile.is_open())
		{
			sRet = utils.StrFormat("\nCannot open \"%s\"\n", sINIFile.c_str());
			return sRet;
		}

		while (getline(hTestINIFile, sCurrentLine))
		{
			utils.StrToLC(sCurrentLine);
			utils.StrTrim(sCurrentLine);
			sCurrentLine.erase(remove(sCurrentLine.begin(), sCurrentLine.end(), ' '), sCurrentLine.end());

			if (sCurrentLine.substr(0, 15) == "avsmeterversion")
			{
				if (sCurrentLine.substr(sCurrentLine.length() - 5) == sAVSMVersion)
				{
					bNewVersion = FALSE;
					break;
				}
			}
		}

		hTestINIFile.close();
	}


	if ((!utils.FileExists(sINIFile)) || bNewVersion) //write defaults
	{
		ofstream hINIFile(sINIFile.c_str());
		if (!hINIFile.is_open())
	  {
			sRet = utils.StrFormat("\nCannot create \"%s\"\n", sINIFile.c_str());
			return sRet;
		}

		hINIFile << "AVSMeterVersion=" << sAVSMVersion << "\n";
		hINIFile << "CreateLog=0\n";
		hINIFile << "PauseBeforeExit=0\n";
		hINIFile << "FrameRange=0,-1\n";
		hINIFile << "InvokeDistributor=1\n";
		hINIFile << "AllowOnlyOneInstance=1\n";
		hINIFile << "LogEstimatedTime=0\n";
		hINIFile << "UseColor=1\n";

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
	sCurrentLine = "";
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

		if (sCurrentLine.substr(0, 8) == "usecolor")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "0")
				Settings.bUseColor = FALSE;
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


string CreateLogFile(string &s_avsfile, string &s_logbuffer, string &s_gpuinfo, PerformanceData * cs_pdata, unsigned int ui_index, string &s_avserror, unsigned int ui_frameinterval)
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
	utils.StrTrim(sAVSBuffer);

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
	sLog = "\n[Script]\n" + sAVSBuffer + "\n\n";
	hLogFile << sLog;

	if (Settings.bMonitorGPULoad)
		hLogFile << s_gpuinfo;

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


unsigned int CalculateFrameInterval(string &s_avsfile, string &s_avserror)
{
	unsigned int uiFrameInterval = 0;
	s_avserror = "";

	if (!timer.TestTimer())
	{
		s_avserror = "QueryPerformanceCounter() is not supported";
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

		IScriptEnvironment *AVS_env = CreateEnvironment(AvisynthInfo.iInterfaceVersion);
		if (!AVS_env)
		{
			s_avserror = "Could not create IScriptenvironment";
			::FreeLibrary(hDLL);
			return uiFrameInterval;
		}

		AVS_linkage = AVS_env->GetAVSLinkage();

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
			if ((iMTMode > 0) && (iMTMode < 5) && Settings.bInvokeDistributor)
				AVS_main = AVS_env->Invoke("Distributor", AVS_main);
		}
		catch (IScriptEnvironment::NotFound)
		{
		}

		AVS_clip = AVS_main.AsClip();
		AVS_vidinfo = AVS_clip->GetVideoInfo();

		unsigned int uiTotalFrames = (unsigned int)AVS_vidinfo.num_frames;

		if (!AVS_vidinfo.HasVideo())
			AVS_env->ThrowError("Script did not return a video clip:\n(%s)", s_avsfile.c_str());

		unsigned int uif = 0;
		double TDelta = 0.0;

		double T0 = timer.GetSTDTimer();
		for (;;)
		{
			PVideoFrame src_frame = AVS_clip->GetFrame(uif, AVS_env);
			++uif;

			TDelta = timer.GetSTDTimer() - T0;
			if ((TDelta >= 5.0) || (uif == uiTotalFrames))
				break;
		}

		uiFrameInterval = 5000;
		double dAverageFrameTime = 0.0;
		if (TDelta > 0.2)
			dAverageFrameTime = (1000.0 * TDelta) / (double)uif; //ms

		if (dAverageFrameTime >  0.005)
			uiFrameInterval = 2000;
		if (dAverageFrameTime >  0.010)
			uiFrameInterval = 1000;
		if (dAverageFrameTime >  0.020)
			uiFrameInterval = 500;
		if (dAverageFrameTime >  0.050)
			uiFrameInterval = 200;
		if (dAverageFrameTime >  0.100)
			uiFrameInterval = 100;
		if (dAverageFrameTime >  0.200)
			uiFrameInterval = 50;
		if (dAverageFrameTime >  0.500)
			uiFrameInterval = 20;
		if (dAverageFrameTime >  1.000)
			uiFrameInterval = 10;
		if (dAverageFrameTime > 10.000)
			uiFrameInterval = 1;

		AVS_clip = 0;
		AVS_main = 0;
		AVS_temp = 0;
		AVS_env->DeleteScriptEnvironment();
		AVS_env = 0;
	}
	catch (AvisynthError err)
	{
		s_avserror = utils.StrFormat("%s", (PCSTR)err.msg);
		uiFrameInterval = 0;
	}
	catch (...)
	{
		s_avserror = utils.OSErrorMessage();
		if (s_avserror == "")
			s_avserror = "Unknown exception";

		uiFrameInterval = 0;
	}

	AVS_linkage = 0;

	::FreeLibrary(hDLL);

	return uiFrameInterval;
}


void PrintFmt(BYTE bStream, WORD wAttributes, const char *fmt, ...)
{
	if (!fmt)	return;

	va_list args;
	va_start(args, fmt);

	if (wAttributes)
		utils.SetRGB(wAttributes, Settings.bUseColor);

	if (bStream == STD_OUT)
	{
		vfprintf(stdout, fmt, args);
		fflush(stdout);
	}
	if (bStream == STD_ERR)
	{
		vfprintf(stderr, fmt, args);
		fflush(stderr);
	}

	if (wAttributes)
		utils.ResetRGB(Settings.bUseColor);

	va_end(args);
	
	return;
}


void PrintUsage()
{
	PrintFmt(STD_OUT, COLOR_EMPHASIS, "\nUsage:  AVSMeter script.avs [switches]\n\n");
	PrintFmt(STD_OUT, COLOR_EMPHASIS, "Switches:\n");
	PrintFmt(STD_OUT, COLOR_EMPHASIS, " -info  [-i]         Display clip info only\n");
	PrintFmt(STD_OUT, COLOR_EMPHASIS, " -log   [-l]         Create log file\n");
	PrintFmt(STD_OUT, COLOR_EMPHASIS, " -fixed [-f]         Use fixed frame interval for measurements\n");
	PrintFmt(STD_OUT, COLOR_EMPHASIS, " -gpu                Monitor GPU/VPU usage\n");
	PrintFmt(STD_OUT, COLOR_EMPHASIS, " -range=first,last   Specify frame range\n");
	PrintFmt(STD_OUT, COLOR_EMPHASIS, " -timelimit=n        Specify time (sec) after which the process will exit\n");
	PrintFmt(STD_OUT, COLOR_EMPHASIS, " -priority=n         Specify process priority (1:low, 2:normal, 3:high)\n");
	PrintFmt(STD_OUT, COLOR_EMPHASIS, " -help [-h]          Display command line usage\n");

	PrintFmt(STD_OUT, COLOR_DEFAULT, "\n");
	return;
}

