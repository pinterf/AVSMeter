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

#define REFRESH_INTERVAL 0.333333333333333


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
	BOOL      bLogFileDateTimeSuffix;
	BOOL      bSkipSpeedTest;
};


class CPerformanceData
{
public:
	unsigned int  current_frame;
	double        fps_current;
	double        fps_average;
	WORD          cpu_usage;
	WORD          gpu_usage;
	WORD          vpu_usage;
	DWORD         phys_mem;
	DWORD         virt_mem;
	WORD          num_threads;
};


CAvisynthInfo AvisynthInfo;
CUtils utils;
CTimer timer;
CSettings Settings;

unsigned int CalculateFrameInterval(string &s_avsfile, string &s_avserror);
string       CreateLogFile(string &s_avsfile, string &s_logbuffer, string &s_gpuinfo, CPerformanceData * cs_pdata, unsigned int ui_index, string &s_avserror, unsigned int ui_frameinterval, BOOL bNVVP);
string       CreateCSVFile(string &s_avsfile, string &s_gpuinfo, CPerformanceData * cs_pdata, unsigned int ui_index, unsigned int ui_frameinterval, BOOL bNVVP);
string       ParseINIFile();
void         PrintUsage();
void         Pause();
void         PrintConsole(BOOL bUseStdOut, WORD wAttributes, const char *fmt, ...);


int main(int argc, char* argv[])
{
	UINT nPrevErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);
	Settings.bPauseBeforeExit = TRUE;
	Settings.bUseColor = TRUE;
	Settings.sSystemTime = "";

	int iRet = 0;
	string sAVSMVersion = utils.GetFileVersion("");
	if (sAVSMVersion.length() > 5)
		sAVSMVersion = sAVSMVersion.substr(0, 5);

	#ifdef AVS64
		sAVSMVersion += " (x64)";
	#else
		sAVSMVersion += " (x86)";
	#endif

	PrintConsole(Settings.bConUseStdOut, COLOR_AVSM_VERSION, "\nAVSMeter %s", sAVSMVersion.c_str());
	PrintConsole(Settings.bConUseStdOut, COLOR_AVSM_VERSION, " - Copyright (c) 2012-2016, Groucho2004\n");

	string sRet = ParseINIFile();
	if (sRet != "")
	{
		PrintConsole(TRUE, COLOR_ERROR, sRet.c_str());
		Pause();
		return -1;
	}

	string sOutBuf = "";
	string sCommandLine = "";
	string sCmdToken = "";
	string sArg = "";
	string sArgTest = "";
	string sAVSFile = "";
	string sLogBuffer = "";
	string sGPUInfo = "";
	CPerformanceData * perfdata = 0;
	unsigned int uiPDIndex = 0;
	BOOL bEarlyExit = TRUE;
	BOOL bInfoOnly = FALSE;
	string sAVSError = "";
	unsigned int uiFrameInterval = 10;
	string sErrorMsg = "";
	BOOL bPrintAVSInfo = FALSE;

	PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", utils.StrPad("Query Avisynth info...").c_str());

	if (!timer.TestTimer())
	{
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", utils.StrPad("").c_str());
		PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nQueryPerformanceCounter() is not supported\n");
		Pause();
		return -1;
	}

	AvisynthInfo.sAVSDLL = Settings.sAVSDLL;
	BOOL bRet = AvisynthInfo.GetInfo(sErrorMsg);

	if (!bRet)
	{
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", utils.StrPad("").c_str());
		if (AvisynthInfo.bLoadError)
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\n%s\n", sErrorMsg.c_str());
			Pause();
			return -1;
		}

		#ifdef AVS64
			if (!AvisynthInfo.bIs64BitVersion)
			{
				PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nAVSMeter (x64) cannot load a 32 Bit avisynth.dll.\n");
				Pause();
				return -1;
			}
		#else
			if (AvisynthInfo.bIs64BitVersion)
			{
				PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nAVSMeter (x86) cannot load a 64 Bit avisynth.dll.\n");
				Pause();
				return -1;
			}
		#endif

		if (sErrorMsg != "")
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\n%s\n", sErrorMsg.c_str());
		else
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nCannot load avisynth.dll\n%s\n", SysErrorMessage().c_str());

		Pause();
		return -1;
	}

	PrintConsole(Settings.bConUseStdOut, COLOR_AVISYNTH_VERSION, "\r%s", AvisynthInfo.sVersionString.c_str());
	PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, " (%s)\n", AvisynthInfo.sFileVersion.c_str());

	if (Settings.bAllowOnlyOneInstance)
	{
		CreateMutex(NULL, TRUE, "__avsmeter__single__instance__lock__");
		if (GetLastError() == ERROR_ALREADY_EXISTS)
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nOne instance of AVSMeter is already running\n");
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
	size_t arg_len = 0;
	for (int iArg = 1; iArg < argc; iArg++)
	{
		sArg = argv[iArg];
		sArgTest = sArg;
		utils.StrTrim(sArgTest);
		sCmdToken = sArgTest;
		utils.StrToLC(sArgTest);
		arg_len = sArgTest.length();

		if (sArgTest == "-avsinfo")
		{
			sCommandLine += sCmdToken + " ";
			bPrintAVSInfo = TRUE;
			continue;
		}

		if ((sArgTest == "-i") || (sArgTest == "-info"))
		{
			sCommandLine += sCmdToken + " ";
			bInfoOnly = TRUE;
			continue;
		}

		if ((sArgTest == "-l") || (sArgTest == "-log"))
		{
			sCommandLine += sCmdToken + " ";
			Settings.bCreateLog = TRUE;
			continue;
		}

		if (sArgTest == "-n")
		{
			sCommandLine += sCmdToken + " ";
			Settings.bSkipSpeedTest = TRUE;
			continue;
		}

		if (sArgTest == "-csv")
		{
			sCommandLine += sCmdToken + " ";
			Settings.bCreateCSV = TRUE;
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
				PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid parameter format: \"%s\"\n", sArg.c_str());
				Pause();
				return -1;
			}

			size_t spos = sTemp.find(",");
			if ((spos < 1) || (spos > 27))
			{
				PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid parameter format: \"%s\"\n", sArg.c_str());
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
				PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid parameter format: \"%s\"\n", sArg.c_str());
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
				Settings.nProcessPriority = (BYTE)atol(sTemp.c_str());
				if ((Settings.nProcessPriority < 1) || (Settings.nProcessPriority > 3))
				{
					PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid parameter value: \"%s\"\nValue must be between 1, 2 or 3\n", sArg.c_str());
					Pause();
					return -1;
				}
			}
			else
			{
				PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid parameter value: \"%s\"\nValue must be between 1, 2 or 3\n", sArg.c_str());
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
					PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid parameter value: \"%s\"\nValue must be between 1 and 999999\n", sArg.c_str());
					Pause();
					return -1;
				}
			}
			else
			{
				PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid parameter value: \"%s\"\nValue must be between 1 and 999999\n", sArg.c_str());
				Pause();
				return -1;
			}

			continue;
		}

		if (arg_len > 4)
		{
			if (sArgTest.substr(sArgTest.length() - 4) == ".avs")
			{
				LPTSTR lpPart;
				char szOut[2050];
				if (::GetFullPathName(argv[iArg], 2048, szOut, &lpPart))
				{
					sAVSFile = utils.StrFormat("%s", szOut);
					if (!utils.FileExists(sAVSFile))
					{
						PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nFile not found: \"%s\"\n", sArg.c_str());
						Pause();
						return -1;
					}
					else
						continue;
				}
			}
		}

		if ((Settings.bAutoCompleteExtension) && (sAVSFile == ""))
		{
			if (arg_len > 0)
			{
				sTemp = sArg + ".avs";
				LPTSTR lpPart;
				char szOut[2050];
				if (::GetFullPathName(sTemp.c_str(), 2048, szOut, &lpPart))
				{
					sAVSFile = utils.StrFormat("%s", szOut);
					if (!utils.FileExists(sAVSFile))
					{
						PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nFile not found: \"%s\"\n", sTemp.c_str());
						Pause();
						return -1;
					}
					else
						continue;
				}
			}
		}

		PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid argument: \"%s\"\n", sArg.c_str());
		Pause();
		return -1;
	}

	utils.StrTrim(sCommandLine);

	if (Settings.nProcessPriority == 1)
		::SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
	else if (Settings.nProcessPriority == 3)
		::SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	else
		::SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);


	CGPUInfo gpuinfo;
	if (Settings.bMonitorGPULoad)
	{
		if (!gpuinfo.bGPUZIsRunning)
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nIn order to use GPU monitoring, GPU-Z must be running.\n\n");
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "Continue without GPU monitoring? [y/n]");

			for (;;)
			{
				Sleep(10);
				if (_kbhit())
				{
					int iKey = _getch();
					if ((iKey == 0x59) || (iKey == 0x79)) //y
					{
						PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
						utils.CursorUp(1);
						PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
						utils.CursorUp(1);
						PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
						utils.CursorUp(1);
						Settings.bMonitorGPULoad = FALSE;
						break;
					}
					else if ((iKey == 0x4E) || (iKey == 0x6E)) //n
					{
						PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
						utils.CursorUp(1);
						PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
						utils.CursorUp(1);
						PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", utils.StrPad("").c_str());
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
				PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nCannot initialize GPU-Z shared memory:\n%s", gpuinfo.sError.c_str());
				Pause();
				return -1;
			}
		}
	}

	PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", utils.StrPad("Query system info...").c_str());
	utils.GetCPUID();
	PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", utils.StrPad("").c_str());

	if (bPrintAVSInfo)
	{
		utils.CursorUp(1);
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());

		sOutBuf = utils.StrFormat("Log created with:             AVSMeter %s\n", sAVSMVersion.c_str());
		sLogBuffer += sOutBuf;

		sLogBuffer += "\n[OS/Hardware info]\n";
		sOutBuf = utils.StrFormat("OS version:                   %s\n", utils.GetOSVersion().c_str());
		sLogBuffer += sOutBuf;

		if (utils.bCPUIDSuccess)
		{
			sOutBuf = utils.StrFormat("CPU (brand string):           %s\n", utils.cpudata.CPUBrandString.c_str());
			sLogBuffer += sOutBuf;
			sOutBuf = utils.StrFormat("CPU (code name):              %s\n", utils.cpudata.CPUCodeName.c_str());
			sLogBuffer += sOutBuf;
			if (utils.cpudata.CPURealClock > 500)
			{
				sOutBuf = utils.StrFormat("CPU clock (measured):         %u MHz\n", utils.cpudata.CPURealClock);
				sLogBuffer += sOutBuf;
			}
			sOutBuf = utils.StrFormat("CPU cores / Logical cores:    %u / %u\n", utils.cpudata.CPUCores, utils.cpudata.CPULogicalCores);
			sLogBuffer += sOutBuf;
		}


		if (Settings.bMonitorGPULoad)
		{
			sOutBuf = utils.StrFormat("Video card:                   %s", gpuinfo.data.CardName.c_str());
			sLogBuffer += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("GPU name:                     %s", gpuinfo.data.GPUName.c_str());
			sLogBuffer += sOutBuf + "\n";
			if (gpuinfo.data.VPEngineName != "")
			{
				sOutBuf = utils.StrFormat("NVIDIA VPU version:           %s", gpuinfo.data.VPEngineName.c_str());
				sLogBuffer += sOutBuf + "\n";
			}
			sOutBuf = utils.StrFormat("Video memory size:            %s", gpuinfo.data.MemSize.c_str());
			sLogBuffer += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("OpenCL version:               %s", gpuinfo.data.OpenCLVersion.c_str());
			sLogBuffer += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("Graphics driver version:      %s\n", gpuinfo.data.DriverVersion.c_str());
			sLogBuffer += sOutBuf;
		}


		sLogBuffer += "\n\n[Avisynth info]";
		sOutBuf = utils.StrFormat("\nAvisynth VersionString:       %s\n", AvisynthInfo.sVersionString.c_str());
		sLogBuffer += sOutBuf;
		PrintConsole(TRUE, COLOR_EMPHASIS, sOutBuf.c_str());

		sOutBuf = utils.StrFormat("Avisynth VersionNumber:       %s\n", AvisynthInfo.sVersionNumber.c_str());
		sLogBuffer += sOutBuf;
		PrintConsole(TRUE, COLOR_EMPHASIS, sOutBuf.c_str());

		sOutBuf = utils.StrFormat("File version:                 %s\n", AvisynthInfo.sFileVersion.c_str());
		sLogBuffer += sOutBuf;
		PrintConsole(TRUE, COLOR_EMPHASIS, sOutBuf.c_str());

		sOutBuf = utils.StrFormat("Avisynth Interface Version:   %d\n", AvisynthInfo.iInterfaceVersion);
		sLogBuffer += sOutBuf;
		PrintConsole(TRUE, COLOR_EMPHASIS, sOutBuf.c_str());

		sOutBuf = utils.StrFormat("Multi-threading support:      %s\n", AvisynthInfo.bIsMTVersion ? ("Yes") : ("No"));
		sLogBuffer += sOutBuf;
		PrintConsole(TRUE, COLOR_EMPHASIS, sOutBuf.c_str());

		sOutBuf = utils.StrFormat("Linker/compiler version:      %s\n", AvisynthInfo.sLinkerVersion.c_str());
		sLogBuffer += sOutBuf;
		PrintConsole(TRUE, COLOR_EMPHASIS, sOutBuf.c_str());

		sOutBuf = utils.StrFormat("Avisynth DLL location:        %s\n", utils.StrAnsiToOEM(AvisynthInfo.sDLLPath).c_str());
		sLogBuffer += sOutBuf;
		PrintConsole(TRUE, COLOR_EMPHASIS, sOutBuf.c_str());

		sOutBuf = utils.StrFormat("Avisynth DLL time stamp:      %s\n", AvisynthInfo.sTimeStamp.c_str());
		sLogBuffer += sOutBuf;
		PrintConsole(TRUE, COLOR_EMPHASIS, sOutBuf.c_str());

		string sPluginDir = "";
		for (unsigned int uiPlugDir = 0; uiPlugDir < AvisynthInfo.NumPlugDirRefs; uiPlugDir++)
		{
			sPluginDir = AvisynthInfo.saPluginDirRefs[uiPlugDir];
			if (sPluginDir != "")
			{
				size_t spos = sPluginDir.find(":\t");
				sPluginDir = sPluginDir.substr(0, spos + 1) + "    " + sPluginDir.substr(spos + 2);

				sOutBuf = utils.StrFormat("%s\n", sPluginDir.c_str());
				sLogBuffer += sOutBuf;
				PrintConsole(TRUE, COLOR_EMPHASIS, utils.StrAnsiToOEM(sOutBuf).c_str());
			}
		}

		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n\r%s\r", utils.StrPad("Query auto-load plugins...").c_str());
		AvisynthInfo.EnumPluginDLLs();
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
		utils.CursorUp(1);

		string sPluginDLL = "";

		//CPP26 plugs
		if (AvisynthInfo.mPluginDLLsCPP26.size() > 0)
		{
			sOutBuf = "\n\n[Avisynth CPP 2.6 plugins]\n";
			sLogBuffer += sOutBuf;
			PrintConsole(TRUE, COLOR_AVSM_VERSION, sOutBuf.c_str());

			for (set<string, StrCaseInsensitiveCompare>::iterator cpp26 = AvisynthInfo.mPluginDLLsCPP26.begin(); cpp26 != AvisynthInfo.mPluginDLLsCPP26.end(); ++cpp26)
			{
				sPluginDLL = *cpp26;

				sOutBuf = utils.StrFormat("%s", sPluginDLL.c_str());
				sLogBuffer += sOutBuf;
				PrintConsole(TRUE, COLOR_EMPHASIS, utils.StrAnsiToOEM(sOutBuf).c_str());
				sOutBuf = utils.StrFormat("  (%s)\n", utils.GetFileVersion(sPluginDLL).c_str());
				sLogBuffer += sOutBuf;
				PrintConsole(TRUE, FG_HGREEN | BG_BLACK, utils.StrAnsiToOEM(sOutBuf).c_str());
			}
		}

		//CPP25 plugs
		if (AvisynthInfo.mPluginDLLsCPP25.size() > 0)
		{
			sOutBuf = "\n\n[Avisynth CPP 2.5 plugins]\n";
			sLogBuffer += sOutBuf;
			PrintConsole(TRUE, COLOR_AVSM_VERSION, sOutBuf.c_str());

			for (set<string, StrCaseInsensitiveCompare>::iterator cpp25 = AvisynthInfo.mPluginDLLsCPP25.begin(); cpp25 != AvisynthInfo.mPluginDLLsCPP25.end(); ++cpp25)
			{
				sPluginDLL = *cpp25;

				sOutBuf = utils.StrFormat("%s", sPluginDLL.c_str());
				sLogBuffer += sOutBuf;
				PrintConsole(TRUE, COLOR_EMPHASIS, utils.StrAnsiToOEM(sOutBuf).c_str());
				sOutBuf = utils.StrFormat("  (%s)\n", utils.GetFileVersion(sPluginDLL).c_str());
				sLogBuffer += sOutBuf;
				PrintConsole(TRUE, FG_HGREEN | BG_BLACK, utils.StrAnsiToOEM(sOutBuf).c_str());
			}
		}

		//CPP20 plugs
		if (AvisynthInfo.mPluginDLLsCPP20.size() > 0)
		{
			sOutBuf = "\n\n[Avisynth CPP 2.0 plugins]\n";
			sLogBuffer += sOutBuf;
			PrintConsole(TRUE, COLOR_AVSM_VERSION, sOutBuf.c_str());

			for (set<string, StrCaseInsensitiveCompare>::iterator cpp20 = AvisynthInfo.mPluginDLLsCPP20.begin(); cpp20 != AvisynthInfo.mPluginDLLsCPP20.end(); ++cpp20)
			{
				sPluginDLL = *cpp20;

				sOutBuf = utils.StrFormat("%s", sPluginDLL.c_str());
				sLogBuffer += sOutBuf;
				PrintConsole(TRUE, COLOR_EMPHASIS, utils.StrAnsiToOEM(sOutBuf).c_str());
				sOutBuf = utils.StrFormat("  (%s)\n", utils.GetFileVersion(sPluginDLL).c_str());
				sLogBuffer += sOutBuf;
				PrintConsole(TRUE, FG_HGREEN | BG_BLACK, utils.StrAnsiToOEM(sOutBuf).c_str());
			}
		}

		//C25 plugs
		if (AvisynthInfo.mPluginDLLsC25.size() > 0)
		{
			sOutBuf = "\n\n[Avisynth C 2.5 plugins]\n";
			sLogBuffer += sOutBuf;
			PrintConsole(TRUE, COLOR_AVSM_VERSION, sOutBuf.c_str());

			for (set<string, StrCaseInsensitiveCompare>::iterator c25 = AvisynthInfo.mPluginDLLsC25.begin(); c25 != AvisynthInfo.mPluginDLLsC25.end(); ++c25)
			{
				sPluginDLL = *c25;

				sOutBuf = utils.StrFormat("%s", sPluginDLL.c_str());
				sLogBuffer += sOutBuf;
				PrintConsole(TRUE, COLOR_EMPHASIS, utils.StrAnsiToOEM(sOutBuf).c_str());
				sOutBuf = utils.StrFormat("  (%s)\n", utils.GetFileVersion(sPluginDLL).c_str());
				sLogBuffer += sOutBuf;
				PrintConsole(TRUE, FG_HGREEN | BG_BLACK, utils.StrAnsiToOEM(sOutBuf).c_str());
			}
		}

		//C20 plugs
		if (AvisynthInfo.mPluginDLLsC20.size() > 0)
		{
			sOutBuf = "\n\n[Avisynth C 2.0 plugins]\n";
			sLogBuffer += sOutBuf;
			PrintConsole(TRUE, COLOR_AVSM_VERSION, sOutBuf.c_str());

			for (set<string, StrCaseInsensitiveCompare>::iterator c20 = AvisynthInfo.mPluginDLLsC20.begin(); c20 != AvisynthInfo.mPluginDLLsC20.end(); ++c20)
			{
				sPluginDLL = *c20;

				sOutBuf = utils.StrFormat("%s", sPluginDLL.c_str());
				sLogBuffer += sOutBuf;
				PrintConsole(TRUE, COLOR_EMPHASIS, utils.StrAnsiToOEM(sOutBuf).c_str());
				sOutBuf = utils.StrFormat("  (%s)\n", utils.GetFileVersion(sPluginDLL).c_str());
				sLogBuffer += sOutBuf;
				PrintConsole(TRUE, FG_HGREEN | BG_BLACK, utils.StrAnsiToOEM(sOutBuf).c_str());
			}
		}

		if (!AvisynthInfo.mPluginErrors.empty())
		{
			sOutBuf = "\n(!) Plugin errors found, see section [Plugin errors] at the end of this document.\n";
			sLogBuffer += sOutBuf;
		}

		//Plugin Functions
		string sFunction = "";
		if (!AvisynthInfo.mPluginFunctions.empty())
		{
			sOutBuf = "\n\n[External (plugin) functions]\n";
			sLogBuffer += sOutBuf;
		}

		for (set<string, StrCaseInsensitiveCompare>::iterator ppfunc = AvisynthInfo.mPluginFunctions.begin(); ppfunc != AvisynthInfo.mPluginFunctions.end(); ++ppfunc)
		{
			sFunction = *ppfunc;
			sOutBuf = utils.StrFormat("%s\n", sFunction.c_str());
			sLogBuffer += sOutBuf;
		}


		//Internal Functions
		sFunction = "";
		if (!AvisynthInfo.mInternalFunctions.empty())
		{
			sOutBuf = "\n\n[Internal functions]\n";
			sLogBuffer += sOutBuf;
		}

		for (set<string, StrCaseInsensitiveCompare>::iterator pifunc = AvisynthInfo.mInternalFunctions.begin(); pifunc != AvisynthInfo.mInternalFunctions.end(); ++pifunc)
		{
			sFunction = *pifunc;
			sOutBuf = utils.StrFormat("%s\n", sFunction.c_str());
			sLogBuffer += sOutBuf;
		}

		//Plug errors
		string sPluginError = "";
		string sLineSep(utils.GetConsoleWidth() - 2, '-');

		if (!AvisynthInfo.mPluginErrors.empty())
		{
			sOutBuf = "\n\n[Plugin errors]\n";
			sLogBuffer += sOutBuf;
			PrintConsole(TRUE, COLOR_AVSM_VERSION, sOutBuf.c_str());
			sOutBuf = sLineSep + "\n";
			sLogBuffer += sOutBuf;
			PrintConsole(TRUE, COLOR_ERROR, sOutBuf.c_str());
		}

		for (set<string>::iterator err = AvisynthInfo.mPluginErrors.begin(); err != AvisynthInfo.mPluginErrors.end(); ++err)
		{
			sPluginError = *err;
			sOutBuf = utils.StrFormat("\n%s\n\n", sPluginError.c_str());
			sOutBuf += sLineSep + "\n";
			sLogBuffer += sOutBuf;
			PrintConsole(TRUE, COLOR_ERROR, utils.StrAnsiToOEM(sOutBuf).c_str());
		}

		if (Settings.bCreateLog)
		{
			if (Settings.sSystemTime == "")
				Settings.sSystemTime = utils.GetFormattedSystemTime();

 			string sInfoFile = "";
			char szPath[2050];
			if (::GetCurrentDirectory(sizeof(szPath), szPath) > 0)
			{
				if (Settings.sLogDirectory == "")
				{
					if (Settings.bLogFileDateTimeSuffix)
						sInfoFile = utils.StrFormat("%s\\avsinfo [%s].log", szPath, Settings.sSystemTime.c_str());
					else
						sInfoFile = utils.StrFormat("%s\\avsinfo.log", szPath);
				}
				else
				{
					if (Settings.bLogFileDateTimeSuffix)
						sInfoFile = utils.StrFormat("%s\\avsinfo [%s].log", Settings.sLogDirectory.c_str(), Settings.sSystemTime.c_str());
					else
						sInfoFile = utils.StrFormat("%s\\avsinfo.log", Settings.sLogDirectory.c_str());
				}

				ofstream hInfoFile(sInfoFile.c_str());
				if (hInfoFile.is_open())
			  {
					hInfoFile << sLogBuffer;
					hInfoFile.flush();
					hInfoFile.close();
				}
			}
		}

		Pause();
		return 0;
	}

	if (sAVSFile == "")
	{
		PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nNo script file specified\n");
		Pause();
		return -1;
	}


	if ((!bInfoOnly) && (!Settings.bSkipSpeedTest))
	{
		sAVSError = "";
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", utils.StrPad("Loading script...").c_str());
		Sleep(333);
		uiFrameInterval = CalculateFrameInterval(sAVSFile, sAVSError);
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", utils.StrPad("").c_str());
		if (uiFrameInterval == 0)
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\n%s\n", sAVSError.c_str());
			Pause();
			return -1;
		}
		sAVSError = "";
	}


	HINSTANCE hDLL;
	if (Settings.sAVSDLL == "")
		hDLL = ::LoadLibrary("avisynth");
	else
		hDLL = ::LoadLibrary(Settings.sAVSDLL.c_str());

	if (!hDLL)
	{
		PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nCannot load avisynth.dll\n%s\n", SysErrorMessage().c_str());
		Pause();
		return -1;
	}

	try
	{
		_set_se_translator(SE_Translator);
		IScriptEnvironment *(__stdcall *CreateEnvironment)(int) = (IScriptEnvironment *(__stdcall *)(int))::GetProcAddress(hDLL, "CreateScriptEnvironment");
		if (!CreateEnvironment)
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nFailed to load CreateScriptEnvironment()\n");
			Pause();
			return -1;
		}

		IScriptEnvironment *AVS_env = CreateEnvironment(AvisynthInfo.iInterfaceVersion);
		if (!AVS_env)
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nCould not create IScriptenvironment\n");
			Pause();
			return -1;
		}

		AVS_linkage = AVS_env->GetAVSLinkage();
		AVSValue AVS_main;
		AVSValue AVS_temp;
		PClip AVS_clip;
		VideoInfo	AVS_vidinfo;

		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", utils.StrPad("Loading script...").c_str());
		AVS_main = AVS_env->Invoke("Import", sAVSFile.c_str());
		Sleep(333);
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", utils.StrPad("").c_str());

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

		sOutBuf = utils.StrFormat("Log created with:             AVSMeter %s", sAVSMVersion.c_str());
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("Script file:                  %s", sAVSFile.c_str());
		sLogBuffer += sOutBuf + "\n";

		if (sCommandLine != "")
		{
			sOutBuf = utils.StrFormat("Command line switches:        %s", sCommandLine.c_str());
			sLogBuffer += sOutBuf + "\n";
		}

		if (Settings.sLogDirectory != "")
		{
			sOutBuf = utils.StrFormat("Log file directory:           %s", Settings.sLogDirectory.c_str());
			sLogBuffer += sOutBuf + "\n";
		}


		sLogBuffer += "\n\n[OS/Hardware info]\n";
		sOutBuf = utils.StrFormat("OS version:                   %s\n", utils.GetOSVersion().c_str());
		sLogBuffer += sOutBuf;

		if (utils.bCPUIDSuccess)
		{
			sOutBuf = utils.StrFormat("CPU (brand string):           %s\n", utils.cpudata.CPUBrandString.c_str());
			sLogBuffer += sOutBuf;
			sOutBuf = utils.StrFormat("CPU (code name):              %s\n", utils.cpudata.CPUCodeName.c_str());
			sLogBuffer += sOutBuf;
			if (utils.cpudata.CPURealClock > 500)
			{
				sOutBuf = utils.StrFormat("CPU clock (measured):         %u MHz\n", utils.cpudata.CPURealClock);
				sLogBuffer += sOutBuf;
			}
			sOutBuf = utils.StrFormat("CPU cores / Logical cores:    %u / %u\n", utils.cpudata.CPUCores, utils.cpudata.CPULogicalCores);
			sLogBuffer += sOutBuf;
		}


		if (Settings.bMonitorGPULoad)
		{
			sOutBuf = utils.StrFormat("Video card:                   %s", gpuinfo.data.CardName.c_str());
			sLogBuffer += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("GPU name:                     %s", gpuinfo.data.GPUName.c_str());
			sLogBuffer += sOutBuf + "\n";
			if (gpuinfo.data.VPEngineName != "")
			{
				sOutBuf = utils.StrFormat("NVIDIA VPU version:           %s", gpuinfo.data.VPEngineName.c_str());
				sLogBuffer += sOutBuf + "\n";
			}
			sOutBuf = utils.StrFormat("Video memory size:            %s", gpuinfo.data.MemSize.c_str());
			sLogBuffer += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("OpenCL version:               %s", gpuinfo.data.OpenCLVersion.c_str());
			sLogBuffer += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("Graphics driver version:      %s\n", gpuinfo.data.DriverVersion.c_str());
			sLogBuffer += sOutBuf;
		}


		sLogBuffer += "\n\n[Avisynth info]";
		sOutBuf = utils.StrFormat("\nAvisynth VersionString:       %s", AvisynthInfo.sVersionString.c_str());
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("Avisynth VersionNumber:       %s", AvisynthInfo.sVersionNumber.c_str());

		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("File version:                 %s", AvisynthInfo.sFileVersion.c_str());
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("Avisynth Interface Version:   %d", AvisynthInfo.iInterfaceVersion);
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("Multi-threading support:      %s", AvisynthInfo.bIsMTVersion ? ("Yes") : ("No"));
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("Linker/compiler version:      %s", AvisynthInfo.sLinkerVersion.c_str());
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("Avisynth DLL location:        %s", AvisynthInfo.sDLLPath.c_str());
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("Avisynth DLL time stamp:      %s", AvisynthInfo.sTimeStamp.c_str());
		sLogBuffer += sOutBuf + "\n";

		for (unsigned int uiPlugDir = 0; uiPlugDir < AvisynthInfo.NumPlugDirRefs; uiPlugDir++)
		{
			sTemp = AvisynthInfo.saPluginDirRefs[uiPlugDir];
			if (sTemp != "")
			{
				size_t spos = sTemp.find(":\t");
				sTemp = sTemp.substr(0, spos + 1) + "    " + sTemp.substr(spos + 2);

				sOutBuf = utils.StrFormat("%s", sTemp.c_str());
				sLogBuffer += sOutBuf + "\n";
			}
		}

		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());

		sLogBuffer += "\n\n[Clip info]\n";

		if (!bAudioOnly)
			sOutBuf = utils.StrFormat("Number of frames:          %11u", AVS_vidinfo.num_frames);
		else
			sOutBuf = "Number of frames:                  n/a";

		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
		sLogBuffer += sOutBuf + "\n";

		if (!bAudioOnly)
			sOutBuf = utils.StrFormat("Length (hh:mm:ss.ms):  %s", timer.FormatTimeString(iMilliSeconds, TRUE).c_str());
		else
			sOutBuf = "Length (hh:mm:ss.ms):              n/a";

		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
		sLogBuffer += sOutBuf + "\n";

		if (!bAudioOnly)
			sOutBuf = utils.StrFormat("Frame width:               %11u", AVS_vidinfo.width);
		else
			sOutBuf = "Frame width:                       n/a";

		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
		sLogBuffer += sOutBuf + "\n";

		if (!bAudioOnly)
			sOutBuf = utils.StrFormat("Frame height:              %11u", AVS_vidinfo.height);
		else
			sOutBuf = "Frame height:                      n/a";

		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
		sLogBuffer += sOutBuf + "\n";

		if (!bAudioOnly)
		{
			if (AVS_vidinfo.IsFieldBased())
			{
				if (AVS_vidinfo.IsTFF())
					sOutBuf = utils.StrFormat("Framerate:                 %11.3f (%u/%u, TFF)", (double)AVS_vidinfo.fps_numerator / (double)AVS_vidinfo.fps_denominator, AVS_vidinfo.fps_numerator, AVS_vidinfo.fps_denominator);
				else if (AVS_vidinfo.IsBFF())
					sOutBuf = utils.StrFormat("Framerate:                 %11.3f (%u/%u, BFF)", (double)AVS_vidinfo.fps_numerator / (double)AVS_vidinfo.fps_denominator, AVS_vidinfo.fps_numerator, AVS_vidinfo.fps_denominator);
				else
					sOutBuf = utils.StrFormat("Framerate:                 %11.3f (%u/%u)", (double)AVS_vidinfo.fps_numerator / (double)AVS_vidinfo.fps_denominator, AVS_vidinfo.fps_numerator, AVS_vidinfo.fps_denominator);
			}
			else
				sOutBuf = utils.StrFormat("Framerate:                 %11.3f (%u/%u)", (double)AVS_vidinfo.fps_numerator / (double)AVS_vidinfo.fps_denominator, AVS_vidinfo.fps_numerator, AVS_vidinfo.fps_denominator);
		}
		else
			sOutBuf = "Framerate:                         n/a";
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = "Colorspace:                  ";
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
		sLogBuffer += sOutBuf;

		if (AVS_vidinfo.pixel_type ==      0xA0000309)
			sOutBuf = "    YV411";
		else if (AVS_vidinfo.pixel_type == 0xA000030B)
			sOutBuf = "     YV24";
		else if (AVS_vidinfo.pixel_type == 0xA0000308)
			sOutBuf = "     YV16";
		else if (AVS_vidinfo.pixel_type == 0xE0000000)
			sOutBuf = "       Y8";
		else if (AVS_vidinfo.pixel_type == 0xA0000008)
			sOutBuf = "     YV12";
		else if (AVS_vidinfo.pixel_type == 0xA0000010)
			sOutBuf = "     i420";
		else if (AVS_vidinfo.pixel_type == 0x60000004)
			sOutBuf = "     YUY2";
		else if (AVS_vidinfo.pixel_type == 0x50000001)
			sOutBuf = "    RGB24";
		else if (AVS_vidinfo.pixel_type == 0x50000002)
			sOutBuf = "    RGB32";
		else if (AVS_vidinfo.pixel_type == 0xA001030B)
			sOutBuf = "YUV444P16";
		else if (AVS_vidinfo.pixel_type == 0xA0010308)
			sOutBuf = "YUV422P16";
		else if (AVS_vidinfo.pixel_type == 0xA0010008)
			sOutBuf = "YUV420P16";
		else if (AVS_vidinfo.pixel_type == 0xA002030B)
			sOutBuf = "YUV444P32";
		else if (AVS_vidinfo.pixel_type == 0xA0020308)
			sOutBuf = "YUV422P32";
		else if (AVS_vidinfo.pixel_type == 0xA0020008)
			sOutBuf = "YUV420P32";
		else if (AVS_vidinfo.pixel_type == 0xA0000109)
			sOutBuf = "     YUV9";
		else if (AVS_vidinfo.pixel_type == 0xE0010000)
			sOutBuf = "      Y16";
		else if (AVS_vidinfo.pixel_type == 0xE0020000)
			sOutBuf = "      Y32";
		else if (!AVS_vidinfo.HasVideo())
			sOutBuf = "      n/a";
		else
			sOutBuf = "  Unknown";

		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "%s\n", sOutBuf.c_str());
		sLogBuffer += sOutBuf + "\n";

		if (bIsMTVersion)
		{
			sOutBuf = utils.StrFormat("Active MT Mode:                      %d", iMTMode);
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "%s\n", sOutBuf.c_str());
			sLogBuffer += sOutBuf + "\n";
		}

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
			Sleep(50);
			AVS_env->DeleteScriptEnvironment();
			AVS_env = 0;

			if (Settings.bCreateLog)
			{
				perfdata = new CPerformanceData[1];
				string sLogRet = CreateLogFile(sAVSFile, sLogBuffer, sGPUInfo, perfdata, uiPDIndex, sAVSError, uiFrameInterval, FALSE);
				if (sLogRet != "")
					PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, sLogRet.c_str());

				delete [] perfdata;
			}

			AVS_linkage = 0;
			::FreeLibrary(hDLL);

			Pause();
			return 0;
		}

		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n");

		if (!AVS_vidinfo.HasVideo())
			AVS_env->ThrowError("\"%s\":\nScript did not return a video clip", sAVSFile.c_str());

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
				PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", utils.StrPad("Reading frames...").c_str());
			else
				PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", utils.StrPad("Reading frame...").c_str());
		}
		else
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\rInvalid frame range specified:\n");
			PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r\"%s\"\n\n", Settings.sFrameRange.c_str());
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\rProceed with default (%u,%u)? [y/n]", uiFirstFrame, uiLastFrame);

			for (;;)
			{
				Sleep(100);
				if (_kbhit())
				{
					int iKey = _getch();
					if ((iKey == 0x59) || (iKey == 0x79)) //y
					{
						PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
						utils.CursorUp(1);
						PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
						utils.CursorUp(1);
						PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
						utils.CursorUp(1);
						PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
						PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", utils.StrPad("Reading frame(s)...").c_str());
						break;
					}
					else if ((iKey == 0x4E) || (iKey == 0x6E)) //n
					{
						PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
						utils.CursorUp(1);
						PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
						utils.CursorUp(1);
						PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
						utils.CursorUp(1);
						PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
						PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", utils.StrPad("").c_str());
						PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", utils.StrPad("").c_str());

						AVS_clip = 0;
						AVS_main = 0;
						AVS_temp = 0;
						AVS_env->DeleteScriptEnvironment();
						AVS_env = 0;

						if (Settings.bCreateLog)
						{
							perfdata = new CPerformanceData[1];
							string sLogRet = CreateLogFile(sAVSFile, sLogBuffer, sGPUInfo, perfdata, uiPDIndex, sAVSError, uiFrameInterval, FALSE);
							if (sLogRet != "")
								PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, sLogRet.c_str());

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

		while ((uiFramesToProcess / uiFrameInterval) > 500000)
			uiFrameInterval *= 10;

		unsigned int uiArraySize = (uiFramesToProcess / uiFrameInterval) + 2;
		unsigned int uiArrayMemSize = (unsigned int)(((double)(sizeof(CPerformanceData) * uiArraySize) / 1048576.0) + 0.5);
		perfdata = new CPerformanceData[uiArraySize];
		for (unsigned int x = 0; x < uiArraySize; x++)
		{
			perfdata[x].current_frame = 0;
			perfdata[x].fps_current = 0.0;
			perfdata[x].fps_average = 0.0;
			perfdata[x].cpu_usage = 0;
			perfdata[x].gpu_usage = 0;
			perfdata[x].vpu_usage = 0;
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

		DWORD dwMaxGPUClock = 0;
		DWORD dwMaxGPUMemClock = 0;

		unsigned int uiCursorOffset = 0;
		SetErrorMode(nPrevErrorMode);
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

				if (gpuinfo.sensors.GPUClock > dwMaxGPUClock)
					dwMaxGPUClock = gpuinfo.sensors.GPUClock;
				if (gpuinfo.sensors.MEMClock > dwMaxGPUMemClock)
					dwMaxGPUMemClock = gpuinfo.sensors.MEMClock;
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

			perfdata[uiPDIndex].current_frame = uiCurrentFrame;
			perfdata[uiPDIndex].fps_current = dFPSCurrent;
			perfdata[uiPDIndex].fps_average = dFPSAverage;
			perfdata[uiPDIndex].cpu_usage = processinfo.wCPUUsage;

			if (Settings.bMonitorGPULoad)
			{
				perfdata[uiPDIndex].gpu_usage = gpuinfo.sensors.GPULoad;
				perfdata[uiPDIndex].vpu_usage = gpuinfo.sensors.VPULoad;
			}
			else
			{
				perfdata[uiPDIndex].gpu_usage = 0;
				perfdata[uiPDIndex].vpu_usage = 0;
			}

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
			PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			++uiCursorOffset;

			if (Settings.bDisplayFPS)
			{
				sOutBuf = utils.StrFormat("FPS (cur | min | max | avg):    %s | %s | %s | %s", utils.StrFormatFPS(dFPSCurrent).c_str(), utils.StrFormatFPS(dFPSMin).c_str(), utils.StrFormatFPS(dFPSMax).c_str(), utils.StrFormatFPS(dFPSAverage).c_str());
				PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
				++uiCursorOffset;
			}

			if (Settings.bDisplayTPF)
			{
				sOutBuf = utils.StrFormat("TPF (cur | max | min | avg):    %s | %s | %s | %s ms", utils.StrFormatFrameTime(1000.0 / dFPSCurrent).c_str(), utils.StrFormatFrameTime(1000.0 / dFPSMin).c_str(), utils.StrFormatFrameTime(1000.0 / dFPSMax).c_str(), utils.StrFormatFrameTime(1000.0 / dFPSAverage).c_str());
				PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
				++uiCursorOffset;
			}

			sOutBuf = utils.StrFormat("Memory usage (phys | virt):     %u | %u MiB", dwPhysicalMemCurrentMB, dwVirtualMemCurrentMB);
			PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			++uiCursorOffset;

			sOutBuf = utils.StrFormat("Thread count:                   %u", processinfo.wThreadCount);
			PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			++uiCursorOffset;

			sOutBuf = utils.StrFormat("CPU usage (current | average):  %u%% | %u%%", processinfo.wCPUUsage, uiCPUUsageAvg);
			PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			++uiCursorOffset;

			if (Settings.bDisplayEfficiencyIndex)
			{
				sOutBuf = utils.StrFormat("Efficiency index:               %s", utils.StrFormatFrameTime(dFPSAverage / (double)uiCPUUsageAvg).c_str());
				PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
				++uiCursorOffset;
			}

			if (Settings.bMonitorGPULoad)
			{
				PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n");
				++uiCursorOffset;

				sOutBuf = utils.StrFormat("GPU core clock / memory clock:  %u / %u", gpuinfo.sensors.GPUClock, gpuinfo.sensors.MEMClock);
				PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
				++uiCursorOffset;

				sOutBuf = utils.StrFormat("GPU usage (current | average):  %u%% | %u%%", gpuinfo.sensors.GPULoad, uiGPUUsageAvg);
				PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
				++uiCursorOffset;

				if (gpuinfo.data.NVVPU)
				{
					sOutBuf = utils.StrFormat("VPU usage (current | average):  %u%% | %u%%", gpuinfo.sensors.VPULoad, uiVPUUsageAvg);
					PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					++uiCursorOffset;
				}

				if (gpuinfo.data.GeneralMem)
				{
					sOutBuf = utils.StrFormat("GPU memory usage:               %u MiB", gpuinfo.sensors.MemoryUsedGeneral);
					PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					++uiCursorOffset;
				}

				if (gpuinfo.data.DedicatedMem)
				{
					sOutBuf = utils.StrFormat("GPU memory usage (Dedicated):   %u MiB", gpuinfo.sensors.MemoryUsedDedicated);
					PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					++uiCursorOffset;
				}

				if (gpuinfo.data.DynamicMem)
				{
					sOutBuf = utils.StrFormat("GPU memory usage (Dynamic):     %u MiB", gpuinfo.sensors.MemoryUsedDynamic);
					PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					++uiCursorOffset;
				}
			}

			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n");
			++uiCursorOffset;

			sOutBuf = utils.StrFormat("Time (elapsed | estimated):     %s | %s", timer.FormatTimeString(iElapsedMS, FALSE).c_str(), timer.FormatTimeString(iEstimatedMS, FALSE).c_str());
			PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			++uiCursorOffset;

			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\nPress \'Esc\' to cancel the process...\n\n");
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
		Sleep(50);
		AVS_env->DeleteScriptEnvironment();
		AVS_env = 0;

		sLogBuffer += "\n\n[Runtime info]\n";

		if ((n >= uiFrameInterval) && (iElapsedMS >= 500))
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

			PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			sLogBuffer += sOutBuf + "\n";

			if (Settings.bDisplayFPS)
			{
				sOutBuf = utils.StrFormat("FPS (min | max | average):      %s | %s | %s", utils.StrFormatFPS(dFPSMin).c_str(), utils.StrFormatFPS(dFPSMax).c_str(), utils.StrFormatFPS(dFPSAverage).c_str());
				PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
				sLogBuffer += sOutBuf + "\n";
			}

			if (Settings.bDisplayTPF)
			{
				sOutBuf = utils.StrFormat("TPF (max | min | average):      %s | %s | %s ms", utils.StrFormatFrameTime(1000.0 / dFPSMin).c_str(), utils.StrFormatFrameTime(1000.0 / dFPSMax).c_str(), utils.StrFormatFrameTime(1000.0 / dFPSAverage).c_str());
				PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
				sLogBuffer += sOutBuf + "\n";
			}

			sOutBuf = utils.StrFormat("Memory usage (phys | virt):     %u | %u MiB", dwPhysicalMemPeakMB, dwVirtualMemPeakMB);
			PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			sLogBuffer += sOutBuf + "\n";

			sOutBuf = utils.StrFormat("Thread count:                   %u", processinfo.wThreadCount);
			PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			sLogBuffer += sOutBuf + "\n";

			sOutBuf = utils.StrFormat("CPU usage (average):            %u%%", uiCPUUsageAvg);
			PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			sLogBuffer += sOutBuf + "\n";

			if (Settings.bDisplayEfficiencyIndex)
			{
				sOutBuf = utils.StrFormat("Efficiency index:               %s", utils.StrFormatFrameTime(dFPSAverage / (double)uiCPUUsageAvg).c_str());
				PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
				sLogBuffer += sOutBuf + "\n";
			}

			if (Settings.bMonitorGPULoad)
			{
				PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n");
				sLogBuffer += "\n";

				sOutBuf = utils.StrFormat("GPU core clock / memory clock:  %u / %u", dwMaxGPUClock, dwMaxGPUMemClock);
				PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
				sLogBuffer += sOutBuf + "\n";

				sOutBuf = utils.StrFormat("GPU usage (average):            %u%%", uiGPUUsageAvg);
				PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
				sLogBuffer += sOutBuf + "\n";

				if (gpuinfo.data.NVVPU)
				{
					sOutBuf = utils.StrFormat("VPU usage (average):            %u%%", uiVPUUsageAvg);
					PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					sLogBuffer += sOutBuf + "\n";
				}

				if (gpuinfo.data.GeneralMem)
				{
					sOutBuf = utils.StrFormat("GPU memory usage:               %u MiB", gpuinfo.sensors.MemoryUsedGeneral);
					PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					sLogBuffer += sOutBuf + "\n";
				}

				if (gpuinfo.data.DedicatedMem)
				{
					sOutBuf = utils.StrFormat("GPU memory usage (Dedicated):   %u MiB", gpuinfo.sensors.MemoryUsedDedicated);
					PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					sLogBuffer += sOutBuf + "\n";
				}

				if (gpuinfo.data.DynamicMem)
				{
					sOutBuf = utils.StrFormat("GPU memory usage (Dynamic):     %u MiB", gpuinfo.sensors.MemoryUsedDynamic);
					PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					sLogBuffer += sOutBuf + "\n";
				}
			}

			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n");
			sLogBuffer += "\n";

			if ((Settings.bLogEstimatedTime) && (n < uiFramesToProcess))
				sOutBuf = utils.StrFormat("Time (elapsed | estimated):     %s | %s", timer.FormatTimeString(iElapsedMS, FALSE).c_str(), timer.FormatTimeString(iEstimatedMS, FALSE).c_str());
			else
				sOutBuf = utils.StrFormat("Time (elapsed):                 %s", timer.FormatTimeString(iElapsedMS, FALSE).c_str());

			PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			sLogBuffer += sOutBuf + "\n";

			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", utils.StrPad("").c_str());
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", utils.StrPad("").c_str());
			utils.CursorUp(2);
		}
		else
		{
			if (!bFirstScr)
			{
				for (int i = 0; i <= 9; i++)
				{
					PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
					utils.CursorUp(1);
				}
			}

			sOutBuf = "Script runtime is too short for meaningful measurements";
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			sLogBuffer += sOutBuf;
			uiPDIndex = 0;
		}
	}
	catch (AvisynthError err)
	{
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", utils.StrPad("").c_str());
		Settings.bCreateLog = TRUE;
		sAVSError = utils.StrFormat("%s", (PCSTR)err.msg);
		iRet = -1;

		if (bEarlyExit == FALSE)
		{
			utils.CursorUp(2);
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", utils.StrPad("").c_str());
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", utils.StrPad("").c_str());
			utils.CursorUp(2);
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\r%s\n", sAVSError.c_str());
		}
		else
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\n%s\n", sAVSError.c_str());
	}
	catch (std::exception& ex)
	{
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", utils.StrPad("").c_str());
		Settings.bCreateLog = TRUE;
		iRet = -1;
		if (bEarlyExit == FALSE)
		{
			utils.CursorUp(2);
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", utils.StrPad("").c_str());
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", utils.StrPad("").c_str());
			utils.CursorUp(2);
		}

		sAVSError = ex.what();
		if (sAVSError == "")
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nUnknown exception\n");
		else
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\n%s\n", sAVSError.c_str());

	}
	catch (...)
	{
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", utils.StrPad("").c_str());
		Settings.bCreateLog = TRUE;
		iRet = -1;
		if (bEarlyExit == FALSE)
		{
			utils.CursorUp(2);
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", utils.StrPad("").c_str());
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", utils.StrPad("").c_str());
			utils.CursorUp(2);
		}

		sAVSError = SysErrorMessage();
		if (sAVSError == "")
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nUnknown exception\n");
		else
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\n%s\n", sAVSError.c_str());
	}

	AVS_linkage = 0;
	::FreeLibrary(hDLL);

	if (Settings.bCreateLog)
	{
		string sr = CreateLogFile(sAVSFile, sLogBuffer, sGPUInfo, perfdata, uiPDIndex, sAVSError, uiFrameInterval, gpuinfo.data.NVVPU);
		if (sr != "")
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, sr.c_str());
			Pause();
			return -1;
		}
	}

	if (Settings.bCreateCSV)
	{
		string cr = CreateCSVFile(sAVSFile, sGPUInfo, perfdata, uiPDIndex, uiFrameInterval, gpuinfo.data.NVVPU);
		if (cr != "")
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, cr.c_str());
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

	string sAVSMVersion = utils.GetFileVersion("");
	if (sAVSMVersion.length() > 5)
		sAVSMVersion = sAVSMVersion.substr(0, 5);

	string sCurrentLine = "";

	//defaults
	Settings.sLogDirectory = "";
	Settings.sAVSDLL = "";
	Settings.bDisplayFPS = TRUE;
	Settings.bDisplayTPF = FALSE;
	Settings.bDisplayEfficiencyIndex = FALSE;
	Settings.bCreateLog = FALSE;
	Settings.bCreateCSV = FALSE;
	Settings.bLogFileDateTimeSuffix = FALSE;
	Settings.bPauseBeforeExit = FALSE;
	Settings.iStartFrame = 0;
	Settings.iStopFrame = -1;
	Settings.bInvokeDistributor = TRUE;
	Settings.bAllowOnlyOneInstance = TRUE;
	Settings.bUseColor = TRUE;
	Settings.bMonitorGPULoad = FALSE;
	Settings.nProcessPriority = 2;
	Settings.bConUseStdOut = FALSE;
	Settings.iTimeLimit = -1;
	Settings.bSkipSpeedTest = FALSE;
	Settings.bLogEstimatedTime = FALSE;
	Settings.bAutoCompleteExtension = FALSE;

	if (!utils.FileExists(sINIFile)) //No ini file present, write defaults
	{
		ofstream hINIFile(sINIFile.c_str());
		if (!hINIFile.is_open())
	  {
			sRet = utils.StrFormat("\nCannot create \"%s\"\n", sINIFile.c_str());
			return sRet;
		}

		hINIFile << "[Main]\n";
		hINIFile << "Version=" << sAVSMVersion << "\n";
		hINIFile << "LogDirectory=\n";
		hINIFile << "AVSDLL=\n";
		hINIFile << "DisplayFPS=1\n";
		hINIFile << "DisplayTPF=0\n";
		hINIFile << "DisplayEfficiencyIndex=0\n";
		hINIFile << "CreateLog=0\n";
		hINIFile << "CreateCSV=0\n";
		hINIFile << "LogFileDateTimeSuffix=0\n";
		hINIFile << "PauseBeforeExit=0\n";
		hINIFile << "FrameRange=0,-1\n";
		hINIFile << "InvokeDistributor=1\n";
		hINIFile << "AllowOnlyOneInstance=1\n";
		hINIFile << "UseColor=1\n";
		hINIFile << "MonitorGPULoad=0\n";
		hINIFile << "ProcessPriority=2\n";
		hINIFile << "ConUseSTDOUT=0\n";
		hINIFile << "TimeLimit=-1\n";
		hINIFile << "SkipSpeedTest=0\n";

		hINIFile << "\n[Miscellaneous]\n";
		hINIFile << "LogEstimatedTime=0\n";
		hINIFile << "AutoCompleteExtension=0\n";

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
		utils.StrTrim(sCurrentLine);
		sOrgLine = sCurrentLine;
		utils.StrToLC(sCurrentLine);

		if (sCurrentLine.substr(0, 12) == "logdirectory")
		{
			Settings.sLogDirectory = sOrgLine.substr(13);
			Settings.sLogDirectory.erase(Settings.sLogDirectory.find_last_not_of("\\") + 1); //remove trailing bs if present
			utils.StrTrim(Settings.sLogDirectory);
			if ((!utils.DirectoryExists(Settings.sLogDirectory)) && (Settings.sLogDirectory != ""))
			{
				sRet = utils.StrFormat("\nLog directory specified in AVSMeter.ini does not exist:\n\"%s\"\n", Settings.sLogDirectory.c_str());
				return sRet;
			}
		}

		if (sCurrentLine.substr(0, 6) == "avsdll")
		{
			Settings.sAVSDLL = sOrgLine.substr(7);
			utils.StrTrim(Settings.sAVSDLL);
			if ((!utils.FileExists(Settings.sAVSDLL)) && (Settings.sAVSDLL != ""))
			{
				sRet = utils.StrFormat("\nAvisynth.dll specified in AVSMeter.ini does not exist:\n\"%s\"\n", Settings.sAVSDLL.c_str());
				return sRet;
			}
		}

		if (sCurrentLine.substr(0, 10) == "displayfps")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "0")
				Settings.bDisplayFPS = FALSE;
			else
				Settings.bDisplayFPS = TRUE;
		}

		if (sCurrentLine.substr(0, 13) == "skipspeedtest")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "0")
				Settings.bSkipSpeedTest = FALSE;
			else
				Settings.bSkipSpeedTest = TRUE;
		}

		if (sCurrentLine.substr(0, 10) == "displaytpf")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "0")
				Settings.bDisplayTPF = FALSE;
			else
				Settings.bDisplayTPF = TRUE;
		}

		if (sCurrentLine.substr(0, 22) == "displayefficiencyindex")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "0")
				Settings.bDisplayEfficiencyIndex = FALSE;
			else
				Settings.bDisplayEfficiencyIndex = TRUE;
		}

		if (sCurrentLine.substr(0, 9) == "createlog")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "0")
				Settings.bCreateLog = FALSE;
			else
				Settings.bCreateLog = TRUE;
		}

		if (sCurrentLine.substr(0, 9) == "createcsv")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "0")
				Settings.bCreateCSV = FALSE;
			else
				Settings.bCreateCSV = TRUE;
		}

		if (sCurrentLine.substr(0, 21) == "logfiledatetimesuffix")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "0")
				Settings.bLogFileDateTimeSuffix = FALSE;
			else
				Settings.bLogFileDateTimeSuffix = TRUE;
		}

		if (sCurrentLine.substr(0, 15) == "pausebeforeexit")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "0")
				Settings.bPauseBeforeExit = FALSE;
			else
				Settings.bPauseBeforeExit = TRUE;
		}

		if (sCurrentLine.substr(0, 17) == "invokedistributor")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "0")
				Settings.bInvokeDistributor = FALSE;
			else
				Settings.bInvokeDistributor = TRUE;
		}

		if (sCurrentLine.substr(0, 20) == "allowonlyoneinstance")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "0")
				Settings.bAllowOnlyOneInstance = FALSE;
			else
				Settings.bAllowOnlyOneInstance = TRUE;
		}

		if (sCurrentLine.substr(0, 14) == "monitorgpuload")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "0")
				Settings.bMonitorGPULoad = FALSE;
			else
				Settings.bMonitorGPULoad = TRUE;
		}

		if (sCurrentLine.substr(0, 15) == "processpriority")
		{
			sTemp = sCurrentLine.substr(16);
			if (utils.IsNumeric(sTemp))
			{
				Settings.nProcessPriority = (BYTE)atol(sTemp.c_str());
				if ((Settings.nProcessPriority < 1) || (Settings.nProcessPriority > 3))
					Settings.nProcessPriority = 2;
			}
		}

		if (sCurrentLine.substr(0, 9) == "timelimit")
		{
			sTemp = sCurrentLine.substr(10);
			if (utils.IsNumeric(sTemp))
			{
				Settings.iTimeLimit = atoi(sTemp.c_str());
				if ((Settings.iTimeLimit < 1) || (Settings.iTimeLimit > 999999))
					Settings.iTimeLimit = -1;
			}
		}

		if (sCurrentLine.substr(0, 8) == "usecolor")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "0")
				Settings.bUseColor = FALSE;
			else
				Settings.bUseColor = TRUE;
		}

		if (sCurrentLine.substr(0, 12) == "conusestdout")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "0")
				Settings.bConUseStdOut = FALSE;
			else
				Settings.bConUseStdOut = TRUE;
		}

		if (sCurrentLine.substr(0, 10) == "framerange")
		{
			sCurrentLine.erase(remove(sCurrentLine.begin(), sCurrentLine.end(), ' '), sCurrentLine.end());
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

		if (sCurrentLine.substr(0, 16) == "logestimatedtime")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "0")
				Settings.bLogEstimatedTime = FALSE;
			else
				Settings.bLogEstimatedTime = TRUE;
		}

		if (sCurrentLine.substr(0, 21) == "autocompleteextension")
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 1) == "0")
				Settings.bAutoCompleteExtension = FALSE;
			else
				Settings.bAutoCompleteExtension = TRUE;
		}
	}

	hINIFile.close();


	//Write INI file with new settings
	ofstream hNewINIFile(sINIFile.c_str());
	if (!hNewINIFile.is_open())
  {
		sRet = utils.StrFormat("\nCannot create \"%s\"\n", sINIFile.c_str());
		return sRet;
	}

	hNewINIFile << "[Main]\n";
	hNewINIFile << "Version=" << sAVSMVersion << "\n";

	sCurrentLine = utils.StrFormat("LogDirectory=%s\n", Settings.sLogDirectory.c_str());
	hNewINIFile << sCurrentLine;

	sCurrentLine = utils.StrFormat("AVSDLL=%s\n", Settings.sAVSDLL.c_str());
	hNewINIFile << sCurrentLine;

	sCurrentLine = utils.StrFormat("DisplayFPS=%u\n", Settings.bDisplayFPS);
	hNewINIFile << sCurrentLine;

	sCurrentLine = utils.StrFormat("DisplayTPF=%u\n", Settings.bDisplayTPF);
	hNewINIFile << sCurrentLine;

	sCurrentLine = utils.StrFormat("DisplayEfficiencyIndex=%u\n", Settings.bDisplayEfficiencyIndex);
	hNewINIFile << sCurrentLine;

	sCurrentLine = utils.StrFormat("CreateLog=%u\n", Settings.bCreateLog);
	hNewINIFile << sCurrentLine;

	sCurrentLine = utils.StrFormat("CreateCSV=%u\n", Settings.bCreateCSV);
	hNewINIFile << sCurrentLine;

	sCurrentLine = utils.StrFormat("LogFileDateTimeSuffix=%u\n", Settings.bLogFileDateTimeSuffix);
	hNewINIFile << sCurrentLine;

	sCurrentLine = utils.StrFormat("PauseBeforeExit=%u\n", Settings.bPauseBeforeExit);
	hNewINIFile << sCurrentLine;

	sCurrentLine = utils.StrFormat("FrameRange=%I64d,%I64d\n", Settings.iStartFrame, Settings.iStopFrame);
	hNewINIFile << sCurrentLine;

	sCurrentLine = utils.StrFormat("InvokeDistributor=%u\n", Settings.bInvokeDistributor);
	hNewINIFile << sCurrentLine;

	sCurrentLine = utils.StrFormat("AllowOnlyOneInstance=%u\n", Settings.bAllowOnlyOneInstance);
	hNewINIFile << sCurrentLine;

	sCurrentLine = utils.StrFormat("UseColor=%u\n", Settings.bUseColor);
	hNewINIFile << sCurrentLine;

	sCurrentLine = utils.StrFormat("MonitorGPULoad=%u\n", Settings.bMonitorGPULoad);
	hNewINIFile << sCurrentLine;

	sCurrentLine = utils.StrFormat("ProcessPriority=%u\n", Settings.nProcessPriority);
	hNewINIFile << sCurrentLine;

	sCurrentLine = utils.StrFormat("ConUseSTDOUT=%u\n", Settings.bConUseStdOut);
	hNewINIFile << sCurrentLine;

	sCurrentLine = utils.StrFormat("TimeLimit=%I64d\n", Settings.iTimeLimit);
	hNewINIFile << sCurrentLine;

	sCurrentLine = utils.StrFormat("SkipSpeedTest=%u\n", Settings.bSkipSpeedTest);
	hNewINIFile << sCurrentLine;

	hNewINIFile << "\n[Miscellaneous]\n";

	sCurrentLine = utils.StrFormat("LogEstimatedTime=%u\n", Settings.bLogEstimatedTime);
	hNewINIFile << sCurrentLine;

	sCurrentLine = utils.StrFormat("AutoCompleteExtension=%u\n", Settings.bAutoCompleteExtension);
	hNewINIFile << sCurrentLine;

	hNewINIFile.flush();
	hNewINIFile.close();

	return sRet;
}


string CreateLogFile(string &s_avsfile, string &s_logbuffer, string &s_gpuinfo, CPerformanceData * cs_pdata, unsigned int ui_index, string &s_avserror, unsigned int ui_frameinterval, BOOL bNVVP)
{
	string sRet = "";
	string sAVSBuffer = "";
	string sCurrentLine = "";

	if (Settings.sSystemTime == "")
		Settings.sSystemTime = utils.GetFormattedSystemTime();

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
	{
		if (Settings.bLogFileDateTimeSuffix)
			sLogFile = s_avsfile.substr(0, ilen - 4) + " [" + Settings.sSystemTime + "].log";
		else
			sLogFile = s_avsfile.substr(0, ilen - 4) + ".log";
	}
	else
	{
		if (Settings.bLogFileDateTimeSuffix)
			sLogFile = s_avsfile + " [" + Settings.sSystemTime + "].log";
		else
			sLogFile = s_avsfile + ".log";
	}

	if (Settings.sLogDirectory != "")
	{
		size_t sLen = sLogFile.length();
		for (size_t nPos = (sLen - 1); nPos > 0; nPos--)
		{
			if (sLogFile[nPos] == '\\')
			{
				sLogFile = Settings.sLogDirectory + "\\" + sLogFile.substr(nPos + 1);
				break;
			}
		}
	}

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
		if (Settings.bMonitorGPULoad)
		{
			if (bNVVP)
				sLog = "\n[Performance data]\n       Frame    Frames/sec   Time/frame(ms)   CPU(%)   GPU(%)   VPU(%)   Threads  PhysMEM(MiB)  VirtMEM(MiB)\n";
			else
				sLog = "\n[Performance data]\n       Frame    Frames/sec   Time/frame(ms)   CPU(%)   GPU(%)   Threads  PhysMEM(MiB)  VirtMEM(MiB)\n";
		}
		else
			sLog = "\n[Performance data]\n       Frame    Frames/sec   Time/frame(ms)   CPU(%)   Threads  PhysMEM(MiB)  VirtMEM(MiB)\n";


		hLogFile << sLog;

		string stemp1 = "";
		string stemp2 = "";
		unsigned int uiFrame = 0;
		if (ui_index > 0)
		{
			for (unsigned int i = 0; i < ui_index; i++)
			{
				uiFrame = cs_pdata[i].current_frame + 1;
				stemp1 = utils.StrFormat("%u", uiFrame);
				string spad((12 - stemp1.length()), ' ');
				if (Settings.bMonitorGPULoad)
				{
					if (bNVVP)
						stemp2 = utils.StrFormat("%s%u %13.3f %16.6f %8u %8u %8u %9u %13u %13u", spad.c_str(), uiFrame, cs_pdata[i].fps_current, 1000.0 / cs_pdata[i].fps_current, cs_pdata[i].cpu_usage, cs_pdata[i].gpu_usage, cs_pdata[i].vpu_usage, cs_pdata[i].num_threads, cs_pdata[i].phys_mem, cs_pdata[i].virt_mem);
					else
						stemp2 = utils.StrFormat("%s%u %13.3f %16.6f %8u %8u %9u %13u %13u", spad.c_str(), uiFrame, cs_pdata[i].fps_current, 1000.0 / cs_pdata[i].fps_current, cs_pdata[i].cpu_usage, cs_pdata[i].gpu_usage, cs_pdata[i].num_threads, cs_pdata[i].phys_mem, cs_pdata[i].virt_mem);
				}
				else
					stemp2 = utils.StrFormat("%s%u %13.3f %16.6f %8u %9u %13u %13u", spad.c_str(), uiFrame, cs_pdata[i].fps_current, 1000.0 / cs_pdata[i].fps_current, cs_pdata[i].cpu_usage, cs_pdata[i].num_threads, cs_pdata[i].phys_mem, cs_pdata[i].virt_mem);

				hLogFile << stemp2 + "\n";
			}
		}
	}

	if (s_avserror != "")
		hLogFile << "\n\n[AviSynth error]\n" + s_avserror + "\n";

	hLogFile.flush();
	hLogFile.close();

	return sRet;
}


string CreateCSVFile(string &s_avsfile, string &s_gpuinfo, CPerformanceData * cs_pdata, unsigned int ui_index, unsigned int ui_frameinterval, BOOL bNVVP)
{
	string sRet = "";
	string sCurrentLine = "";
	string sCSVFile = "";

	if (Settings.sSystemTime == "")
		Settings.sSystemTime = utils.GetFormattedSystemTime();

	size_t ilen = s_avsfile.length();

	if (ilen > 4)
	{
		if (Settings.bLogFileDateTimeSuffix)
			sCSVFile = s_avsfile.substr(0, ilen - 4) + " [" + Settings.sSystemTime + "].csv";
		else
			sCSVFile = s_avsfile.substr(0, ilen - 4) + ".csv";
	}
	else
	{
		if (Settings.bLogFileDateTimeSuffix)
			sCSVFile = s_avsfile + " [" + Settings.sSystemTime + "].csv";
		else
			sCSVFile = s_avsfile + ".csv";
	}


	if (Settings.sLogDirectory != "")
	{
		size_t sLen = sCSVFile.length();
		for (size_t nPos = (sLen - 1); nPos > 0; nPos--)
		{
			if (sCSVFile[nPos] == '\\')
			{
				sCSVFile = Settings.sLogDirectory + "\\" + sCSVFile.substr(nPos + 1);
				break;
			}
		}
	}


	ofstream hCSVFile(sCSVFile.c_str());
	if (!hCSVFile.is_open())
	{
		sRet = utils.StrFormat("\nCannot create \"%s\"\n", sCSVFile.c_str());
		return sRet;
	}

	string sCSV = "";

	if (ui_index > 0)
	{
		if (Settings.bMonitorGPULoad)
		{
			if (bNVVP)
				sCSV = "Frame,Frames/sec,Frames/sec(average),Time/frame(ms),Time/frame(average)(ms),CPU(%),GPU(%),VPU(%),Threads,PhysMEM(MiB),VirtMEM(MiB)\n";
			else
				sCSV = "Frame,Frames/sec,Frames/sec(average),Time/frame(ms),Time/frame(average)(ms),CPU(%),GPU(%),Threads,PhysMEM(MiB),VirtMEM(MiB)\n";
		}
		else
			sCSV = "Frame,Frames/sec,Frames/sec(average),Time/frame(ms),Time/frame(average)(ms),CPU(%),Threads,PhysMEM(MiB),VirtMEM(MiB)\n";

		hCSVFile << sCSV;

		string stemp = "";
		unsigned int uiFrame = 0;
		if (ui_index > 0)
		{
			for (unsigned int i = 0; i < ui_index; i++)
			{
				uiFrame = cs_pdata[i].current_frame + 1;
				if (Settings.bMonitorGPULoad)
				{
					if (bNVVP)
						stemp = utils.StrFormat("%u,%.3f,%.3f,%.6f,%.6f,%u,%u,%u,%u,%u,%u", uiFrame, cs_pdata[i].fps_current, cs_pdata[i].fps_average, 1000.0 / cs_pdata[i].fps_current, 1000.0 / cs_pdata[i].fps_average, cs_pdata[i].cpu_usage, cs_pdata[i].gpu_usage, cs_pdata[i].vpu_usage, cs_pdata[i].num_threads, cs_pdata[i].phys_mem, cs_pdata[i].virt_mem);
					else
						stemp = utils.StrFormat("%u,%.3f,%.3f,%.6f,%.6f,%u,%u,%u,%u,%u", uiFrame, cs_pdata[i].fps_current, cs_pdata[i].fps_average, 1000.0 / cs_pdata[i].fps_current, 1000.0 / cs_pdata[i].fps_average, cs_pdata[i].cpu_usage, cs_pdata[i].gpu_usage, cs_pdata[i].num_threads, cs_pdata[i].phys_mem, cs_pdata[i].virt_mem);
				}
				else
					stemp = utils.StrFormat("%u,%.3f,%.3f,%.6f,%.6f,%u,%u,%u,%u", uiFrame, cs_pdata[i].fps_current, cs_pdata[i].fps_average, 1000.0 / cs_pdata[i].fps_current, 1000.0 / cs_pdata[i].fps_average, cs_pdata[i].cpu_usage, cs_pdata[i].num_threads, cs_pdata[i].phys_mem, cs_pdata[i].virt_mem);

				hCSVFile << stemp + "\n";
			}
		}
	}

	hCSVFile.flush();
	hCSVFile.close();

	return sRet;
}


void Pause()
{
	if (Settings.bPauseBeforeExit)
	{
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\nPress any key to exit...");
		for (;;)
		{
			Sleep(100);
			if (_kbhit())
				break;
		}
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", utils.StrPad("").c_str());
	}

	return;
}


void PrintConsole(BOOL bUseStdOut, WORD wAttributes, const char *fmt, ...)
{
	if (!fmt)	return;

	va_list args;
	va_start(args, fmt);

	if (wAttributes)
		utils.SetRGB(wAttributes, Settings.bUseColor);

	if (bUseStdOut)
	{
		vfprintf(stdout, fmt, args);
		fflush(stdout);
	}
	else
	{
		vfprintf(stderr, fmt, args);
		fflush(stderr);
	}

	if (wAttributes)
		utils.ResetRGB(Settings.bUseColor);

	va_end(args);

	return;
}


unsigned int CalculateFrameInterval(string &s_avsfile, string &s_avserror)
{
	unsigned int uiFrameInterval = 0;
	s_avserror = "Unknown error";

	HINSTANCE hDLL;
	if (Settings.sAVSDLL == "")
		hDLL = ::LoadLibrary("avisynth");
	else
		hDLL = ::LoadLibrary(Settings.sAVSDLL.c_str());

	if (!hDLL)
	{
		s_avserror = "Failed to load avisynth.dll";
		return uiFrameInterval;
	}

	try
	{
		_set_se_translator(SE_Translator);
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

		unsigned int uiFrame = 0;
		double TDelta = 0.0;

		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", utils.StrPad("Analysing script...").c_str());
		double T0 = timer.GetSTDTimer();
		for (;;)
		{
			PVideoFrame src_frame = AVS_clip->GetFrame(uiFrame, AVS_env);
			++uiFrame;
			TDelta = timer.GetSTDTimer() - T0;
			if ((TDelta >= 5.0) || (uiFrame == uiTotalFrames))
				break;
		}

		double dAverageFrameTime = 0.0;
		if (TDelta > 0.5)
			dAverageFrameTime = (1000.0 * TDelta) / (double)uiFrame; //ms
		else
			AVS_env->ThrowError("Script runtime is too short for meaningful measurements");

		uiFrameInterval = 10000;
		if (dAverageFrameTime >  0.004) uiFrameInterval = 5000;
		if (dAverageFrameTime >  0.010) uiFrameInterval = 2000;
		if (dAverageFrameTime >  0.020) uiFrameInterval = 1000;
		if (dAverageFrameTime >  0.040) uiFrameInterval =  500;
		if (dAverageFrameTime >  0.100) uiFrameInterval =  200;
		if (dAverageFrameTime >  0.200) uiFrameInterval =  100;
		if (dAverageFrameTime >  0.400) uiFrameInterval =   50;
		if (dAverageFrameTime >  1.000) uiFrameInterval =   20;
		if (dAverageFrameTime >  2.000) uiFrameInterval =   10;
		if (dAverageFrameTime > 20.000) uiFrameInterval =    1;

		AVS_clip = 0;
		AVS_main = 0;
		AVS_temp = 0;
		Sleep(50);
		AVS_env->DeleteScriptEnvironment();
		AVS_env = 0;
	}
	catch (AvisynthError err)
	{
		s_avserror = utils.StrFormat("%s", (PCSTR)err.msg);
		uiFrameInterval = 0;
	}
	catch (std::exception& ex)
	{
		s_avserror = ex.what();
		uiFrameInterval = 0;
	}
	catch (...)
	{
		s_avserror = SysErrorMessage();
		if (s_avserror == "")
			s_avserror = "Unknown exception";

		uiFrameInterval = 0;
	}

	AVS_linkage = 0;

	if (!::FreeLibrary(hDLL))
	{
		uiFrameInterval = 0;
		s_avserror = SysErrorMessage();
		if (s_avserror != "")
			s_avserror = "Cannot unload avisynth.dll\n" + s_avserror;
		else
			s_avserror = "Cannot unload avisynth.dll";
	}
	Sleep(50);

	return uiFrameInterval;
}


void PrintUsage()
{
	PrintConsole(TRUE, COLOR_EMPHASIS, "\nUsage:  AVSMeter script.avs [switches]\n\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "Switches:\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  -avsinfo            Display extended Avisynth info\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  -info [-i]          Display clip info\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  -log  [-l]          Create log file\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  -csv                Create csv file\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  -gpu                Monitor GPU/VPU usage\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  -range=first,last   Set frame range\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  -timelimit=n        Set time limit (seconds)\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  -priority=n         Set process priority (1:low, 2:normal, 3:high)\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  -n                  Skip initial speed test\n\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  For more info on the command line switches and INI file\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  options read the documentation (AVSMeter.html) included\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  in the distribution package.\n");

	PrintConsole(TRUE, COLOR_DEFAULT, "\n");
	return;
}


