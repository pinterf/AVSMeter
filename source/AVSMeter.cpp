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

unsigned int CalculateFrameInterval(string &s_avsfile);
string       CreateLogFile(string &s_avsfile, string &s_logbuffer, string &s_gpuinfo, vector<CPerformanceData> &cs_pdata, string &s_avserror, BOOL bNVVP, BOOL bOmitPerfData);
string       CreateCSVFile(string &s_avsfile, vector<CPerformanceData> &cs_pdata, BOOL bNVVP);
string       ParseINIFile();
BOOL         WriteINIFile(string &s_inifile);
void         PrintUsage();
void         Pause();
void         PrintConsole(BOOL bUseStdOut, WORD wAttributes, const char *fmt, ...);


int main(int argc, char* argv[])
{
	UINT nPrevErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);
	Settings.sSystemTime = "";

	string sINIRet = ParseINIFile();

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

	if (sINIRet != "")
	{
		PrintConsole(TRUE, COLOR_ERROR, sINIRet.c_str());
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
	string sLogBufferFunctions = "";
	string sLogBufferDeps = "";
	BOOL   bRuntimeTooShort = FALSE;
	string sGPUInfo = "";
	vector<CPerformanceData> perfdata;
	BOOL bEarlyExit = TRUE;
	BOOL bInfoOnly = FALSE;
	string sAVSError = "";
	unsigned int uiFrameInterval = 10;
	string sErrorMsg = "";
	BOOL bPrintAVSInfo = FALSE;

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

		if ((sArgTest == "-s") || (sArgTest == "-simple"))
		{
			sCommandLine += sCmdToken + " ";
			Settings.bSimpleMode = TRUE;
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
					PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid parameter value: \"%s\"\nValue must be \'-1\' (no time limit) or between \'1\' and \'999999\'\n", sArg.c_str());
					Pause();
					return -1;
				}
			}
			else
			{
				PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid parameter value: \"%s\"\nValue must be \'-1\' (no time limit) or between \'1\' and \'999999\'\n", sArg.c_str());
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
				char szOut[MAX_PATH + 1];
				if (::GetFullPathName(argv[iArg], MAX_PATH, szOut, &lpPart))
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
				char szOut[MAX_PATH + 1];
				if (::GetFullPathName(sTemp.c_str(), MAX_PATH, szOut, &lpPart))
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


	if (bPrintAVSInfo)
	{
		if (bInfoOnly)
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\n\"-avsinfo\" and \"-info\" are mutually exclusive\n");
			Pause();
			return -1;
		}
		if (sAVSFile != "")
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nSpecifying a script together with \"-avsinfo\" is pointless\n");
			Pause();
			return -1;
		}
	}

	utils.StrTrim(sCommandLine);

	if (Settings.nProcessPriority == 1)
		::SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
	else if (Settings.nProcessPriority == 3)
		::SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	else
		::SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);

	PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", utils.StrPad("Query Avisynth info...").c_str());

	AvisynthInfo.sAVSDLL = Settings.sAVSDLL;
	BOOL bRet = AvisynthInfo.GetInfo(sErrorMsg);
	PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", utils.StrPad("").c_str());

	if (!bRet)
	{
		if (AvisynthInfo.bLoadError)
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\n%s\n", sErrorMsg.c_str());
			Pause();
			return -1;
		}

		#ifdef AVS64
			if (!AvisynthInfo.bIs64BitAVS)
			{
				PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nAVSMeter (x64) cannot load a 32 Bit avisynth.dll.\n");
				Pause();
				return -1;
			}
		#else
			if (AvisynthInfo.bIs64BitAVS)
			{
				PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nAVSMeter (x86) cannot load a 64 Bit avisynth.dll.\n");
				Pause();
				return -1;
			}
		#endif

		if (sErrorMsg != "")
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\n%s\n", sErrorMsg.c_str());
		else
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nCannot load avisynth.dll:\n%s\n", SysErrorMessage().c_str());

		Pause();
		return -1;
	}


	if (!bPrintAVSInfo)
	{
		PrintConsole(Settings.bConUseStdOut, COLOR_AVISYNTH_VERSION, "\r%s", AvisynthInfo.sVersionString.c_str());
		PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, " (%s)\n", AvisynthInfo.sFileVersion.c_str());
	}

	CGPUInfo gpuinfo;
	if (Settings.bMonitorGPULoad)
	{
		if (!gpuinfo.bGPUZIsRunning)
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nIn order to collect GPU info, GPU-Z must be running.\n\n");
			PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "Continue without GPU info? [y/n]");

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

	if (!timer.TestTimer())
	{
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", utils.StrPad("").c_str());
		PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nQueryPerformanceCounter() is not supported\n");
		Pause();
		return -1;
	}

	sys.GetCPUID();
	PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", utils.StrPad("").c_str());

	if (bPrintAVSInfo)
	{
		sOutBuf = utils.StrFormat("Log created with:           AVSMeter %s\n", sAVSMVersion.c_str());
		sLogBuffer += sOutBuf;

		sLogBuffer += "\n[OS/Hardware info]\n";
		sOutBuf = utils.StrFormat("Operating system:           %s\n", sys.GetOSVersion().c_str());
		sLogBuffer += sOutBuf;

		if (sys.bCPUIDSuccess)
		{
			sOutBuf = utils.StrFormat("CPU (brand string):         %s\n", sys.cpudata.CPUBrandString.c_str());
			sLogBuffer += sOutBuf;
			sOutBuf = utils.StrFormat("CPU (code name):            %s\n", sys.cpudata.CPUCodeName.c_str());
			sLogBuffer += sOutBuf;
			sOutBuf = utils.StrFormat("CPU instructions sets:      %s\n", sys.cpudata.CPUSupportedInstructionSets.c_str());
			sLogBuffer += sOutBuf;
			if (sys.cpudata.CPURealClock > 100)
			{
				sOutBuf = utils.StrFormat("CPU clock (measured):       %u MHz\n", sys.cpudata.CPURealClock);
				sLogBuffer += sOutBuf;
			}
			sOutBuf = utils.StrFormat("CPU cores / Logical cores:  %u / %u\n", sys.cpudata.CPUCores, sys.cpudata.CPULogicalCores);
			sLogBuffer += sOutBuf;
		}


		if (Settings.bMonitorGPULoad)
		{
			sOutBuf = utils.StrFormat("Video card:                 %s", gpuinfo.data.CardName.c_str());
			sLogBuffer += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("GPU name:                   %s", gpuinfo.data.GPUName.c_str());
			sLogBuffer += sOutBuf + "\n";
			if (gpuinfo.data.VPEngineName != "")
			{
				sOutBuf = utils.StrFormat("NVIDIA VPU version:         %s", gpuinfo.data.VPEngineName.c_str());
				sLogBuffer += sOutBuf + "\n";
			}
			sOutBuf = utils.StrFormat("Video memory size:          %s", gpuinfo.data.MemSize.c_str());
			sLogBuffer += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("OpenCL version:             %s", gpuinfo.data.OpenCLVersion.c_str());
			sLogBuffer += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("Graphics driver version:    %s\n", gpuinfo.data.DriverVersion.c_str());
			sLogBuffer += sOutBuf;
		}


		sLogBuffer += "\n\n[Avisynth core info]";
		sOutBuf = utils.StrFormat("\nVersionString:              %s\n", AvisynthInfo.sVersionString.c_str());
		sLogBuffer += sOutBuf;
		PrintConsole(TRUE, COLOR_EMPHASIS, sOutBuf.c_str());

		sOutBuf = utils.StrFormat("VersionNumber:              %s\n", AvisynthInfo.sVersionNumber.c_str());
		sLogBuffer += sOutBuf;
		PrintConsole(TRUE, COLOR_EMPHASIS, sOutBuf.c_str());

		sOutBuf = utils.StrFormat("File version:               %s\n", AvisynthInfo.sFileVersion.c_str());
		sLogBuffer += sOutBuf;
		PrintConsole(TRUE, COLOR_EMPHASIS, sOutBuf.c_str());

		sOutBuf = utils.StrFormat("Interface Version:          %d\n", AvisynthInfo.iInterfaceVersion);
		sLogBuffer += sOutBuf;
		PrintConsole(TRUE, COLOR_EMPHASIS, sOutBuf.c_str());

		sOutBuf = utils.StrFormat("Multi-threading support:    %s\n", AvisynthInfo.bIsMTVersion ? ("Yes") : ("No"));
		sLogBuffer += sOutBuf;
		PrintConsole(TRUE, COLOR_EMPHASIS, sOutBuf.c_str());

		sOutBuf = utils.StrFormat("Linker/compiler version:    %s\n", AvisynthInfo.sLinkerVersion.c_str());
		sLogBuffer += sOutBuf;
		PrintConsole(TRUE, COLOR_EMPHASIS, sOutBuf.c_str());

		sOutBuf = utils.StrFormat("Avisynth.dll location:      %s\n", utils.StrAnsiToOEM(AvisynthInfo.sDLLPath).c_str());
		sLogBuffer += sOutBuf;
		PrintConsole(TRUE, COLOR_EMPHASIS, sOutBuf.c_str());

		if (Settings.bUseUTC)
			sOutBuf = utils.StrFormat("Avisynth.dll time stamp:    %s\n", AvisynthInfo.sTimeStampUTC.c_str());
		else
			sOutBuf = utils.StrFormat("Avisynth.dll time stamp:    %s\n", AvisynthInfo.sTimeStamp.c_str());

		sLogBuffer += sOutBuf;
		PrintConsole(TRUE, COLOR_EMPHASIS, sOutBuf.c_str());

		string sPluginDir = "";
		for (unsigned int uiPlugDir = 0; uiPlugDir < AvisynthInfo.vPluginDirs.size(); uiPlugDir++)
		{
			sPluginDir = AvisynthInfo.vPluginDirs[uiPlugDir];
			if (sPluginDir != "")
			{
				size_t spos = sPluginDir.find(":\t");
				sPluginDir = sPluginDir.substr(0, spos + 1) + "  " + sPluginDir.substr(spos + 2);

				sOutBuf = utils.StrFormat("%s\n", sPluginDir.c_str());
				sLogBuffer += sOutBuf;
				PrintConsole(TRUE, COLOR_EMPHASIS, utils.StrAnsiToOEM(sOutBuf).c_str());
			}
		}

		string sPluginDLL = "";
		string sOldPluginType = "";			
		string sPluginType = "";	
		unsigned int uiPlugin = 0;
		size_t spos = 0;

		if (AvisynthInfo.vPlugins.size() > 0)
		{
			PrintConsole(TRUE, COLOR_DEFAULT, "\n");
			sLogBuffer += "\n";

			for (uiPlugin = 0; uiPlugin < AvisynthInfo.vPlugins.size(); uiPlugin++)
			{
				sPluginDLL = AvisynthInfo.vPlugins[uiPlugin];
				spos = sPluginDLL.find("|");
				sPluginType = sPluginDLL.substr(0, spos);
				utils.StrTrim(sPluginType);

				if (sPluginType != sOldPluginType)
				{
					sOutBuf = utils.StrFormat("\n[%s plugins]\n", sPluginType.c_str());
					sLogBuffer += sOutBuf;
					PrintConsole(TRUE, COLOR_AVSM_VERSION, sOutBuf.c_str());
					sOldPluginType = sPluginType;
				}

				sPluginDLL = sPluginDLL.substr(spos + 1);
				sOutBuf = utils.StrFormat("%s", sPluginDLL.c_str());
				sLogBuffer += sOutBuf;
				PrintConsole(TRUE, COLOR_EMPHASIS, utils.StrAnsiToOEM(sOutBuf).c_str());

				sOutBuf = utils.StrFormat("  [%s]\n", utils.GetFileVersion(sPluginDLL).c_str());
				sLogBuffer += sOutBuf;
				PrintConsole(TRUE, FG_HGREEN | BG_BLACK, utils.StrAnsiToOEM(sOutBuf).c_str());
			}
		}

		//Internal Functions
		string sFunction = "";
		if (AvisynthInfo.vInternalFunctions.size() > 0)
		{
			sOutBuf = "[Internal (core) functions]\n";
			sLogBufferFunctions += sOutBuf;
		}

		for (unsigned int IFunc = 0; IFunc < AvisynthInfo.vInternalFunctions.size(); IFunc++)
		{
			sFunction = AvisynthInfo.vInternalFunctions[IFunc];
			sOutBuf = utils.StrFormat("%s\n", sFunction.c_str());
			sLogBufferFunctions += sOutBuf;
		}

		//Plugin Functions
		sFunction = "";
		if (AvisynthInfo.vPluginFunctions.size() > 0)
		{
			sOutBuf = "\n\n[External (plugin) functions]\n";
			sLogBufferFunctions += sOutBuf;
		}

		for (unsigned int PFunc = 0; PFunc < AvisynthInfo.vPluginFunctions.size(); PFunc++)
		{
			sFunction = AvisynthInfo.vPluginFunctions[PFunc];
			sOutBuf = utils.StrFormat("%s\n", sFunction.c_str());
			sLogBufferFunctions += sOutBuf;
		}

		//Plugin Dependencies
		if (AvisynthInfo.vPluginDependencies.size() > 0)
		{
			if (AvisynthInfo.bIs64BitAVS)
				sOutBuf = "[DLL dependencies (x64)]\n\n";
			else
				sOutBuf = "[DLL dependencies (x86)]\n\n";

			sLogBufferDeps += sOutBuf;
		}

		for (unsigned int PDep = 0; PDep < AvisynthInfo.vPluginDependencies.size(); PDep++)
		{
			sOutBuf = utils.StrFormat("%s\n", AvisynthInfo.vPluginDependencies[PDep].c_str());
			sLogBufferDeps += sOutBuf;
		}

		//Plug errors
		string sPluginError = "";
		string sLineSep(utils.GetConsoleWidth() - 2, '-');

		if (AvisynthInfo.vPluginErrors.size() > 0)
		{
			sOutBuf = "\n\n[Plugin errors/warnings]\n";
			sLogBuffer += sOutBuf;
			PrintConsole(TRUE, COLOR_AVSM_VERSION, sOutBuf.c_str());
			sOutBuf = sLineSep + "\n";
			sLogBuffer += sOutBuf;
			PrintConsole(TRUE, COLOR_ERROR, sOutBuf.c_str());
		}

		for (unsigned int err = 0; err < AvisynthInfo.vPluginErrors.size(); err++)
		{
			sPluginError = AvisynthInfo.vPluginErrors[err];
			sOutBuf = utils.StrFormat("\n%s\n\n", sPluginError.c_str());
			sOutBuf += sLineSep + "\n";
			sLogBuffer += sOutBuf;
			PrintConsole(TRUE, COLOR_ERROR, utils.StrAnsiToOEM(sOutBuf).c_str());
		}

		if (Settings.bCreateLog)
		{
			if (Settings.sSystemTime == "")
				Settings.sSystemTime = sys.GetFormattedSystemTime();

 			string sInfoFile = "";
 			string sFunctionsFile = "";
			string sDepsFile = "";
			char szPath[MAX_PATH + 1];
			if (::GetCurrentDirectory(MAX_PATH, szPath) > 0)
			{
				if (Settings.sLogDirectory == "")
				{
					if (Settings.bLogFileDateTimeSuffix)
					{
						sInfoFile = utils.StrFormat("%s\\avsinfo [%s].log", szPath, Settings.sSystemTime.c_str());
						sFunctionsFile = utils.StrFormat("%s\\avsfunctions [%s].log", szPath, Settings.sSystemTime.c_str());
						sDepsFile = utils.StrFormat("%s\\avsdeps [%s].log", szPath, Settings.sSystemTime.c_str());
					}
					else
					{
						sInfoFile = utils.StrFormat("%s\\avsinfo.log", szPath);
						sFunctionsFile = utils.StrFormat("%s\\avsfunctions.log", szPath);
						sDepsFile = utils.StrFormat("%s\\avsdeps.log", szPath);
					}
				}
				else
				{
					if (Settings.bLogFileDateTimeSuffix)
					{
						sInfoFile = utils.StrFormat("%s\\avsinfo [%s].log", Settings.sLogDirectory.c_str(), Settings.sSystemTime.c_str());
						sFunctionsFile = utils.StrFormat("%s\\avsfunctions [%s].log", Settings.sLogDirectory.c_str(), Settings.sSystemTime.c_str());
						sDepsFile = utils.StrFormat("%s\\avsdeps [%s].log", Settings.sLogDirectory.c_str(), Settings.sSystemTime.c_str());
					}
					else
					{
						sInfoFile = utils.StrFormat("%s\\avsinfo.log", Settings.sLogDirectory.c_str());
						sFunctionsFile = utils.StrFormat("%s\\avsfunctions.log", Settings.sLogDirectory.c_str());
						sDepsFile = utils.StrFormat("%s\\avsdeps.log", Settings.sLogDirectory.c_str());
					}
				}

				ofstream hInfoFile(sInfoFile.c_str());
				if (hInfoFile.is_open())
			  {
					hInfoFile << sLogBuffer;
					hInfoFile.flush();
					hInfoFile.close();
				}

				ofstream hFunctionsFile(sFunctionsFile.c_str());
				if (hFunctionsFile.is_open())
			  {
					hFunctionsFile << sLogBufferFunctions;
					hFunctionsFile.flush();
					hFunctionsFile.close();
				}

				ofstream hDepsFile(sDepsFile.c_str());
				if (hDepsFile.is_open())
			  {
					hDepsFile << sLogBufferDeps;
					hDepsFile.flush();
					hDepsFile.close();
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


	if ((!bInfoOnly) && (!Settings.bSimpleMode))
	{
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", utils.StrPad("Loading script...").c_str());
		Sleep(200);
		uiFrameInterval = CalculateFrameInterval(sAVSFile);
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", utils.StrPad("").c_str());
	}

	HINSTANCE hDLL;
	if (Settings.sAVSDLL == "")
		hDLL = ::LoadLibrary("avisynth");
	else
		hDLL = ::LoadLibrary(Settings.sAVSDLL.c_str());

	if (!hDLL)
	{
		PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nCannot load avisynth.dll:\n%s\n", SysErrorMessage().c_str());
		Pause();
		return -1;
	}

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

	try
	{
		_set_se_translator(SE_Translator);

		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", utils.StrPad("Loading script...").c_str());
		AVS_main = AVS_env->Invoke("Import", sAVSFile.c_str());
		Sleep(200);
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

		sOutBuf = utils.StrFormat("Log created with:           AVSMeter %s", sAVSMVersion.c_str());
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("Script file:                %s", sAVSFile.c_str());
		sLogBuffer += sOutBuf + "\n";

		if (sCommandLine != "")
		{
			sOutBuf = utils.StrFormat("Command line switches:      %s", sCommandLine.c_str());
			sLogBuffer += sOutBuf + "\n";
		}

		if (Settings.sLogDirectory != "")
		{
			sOutBuf = utils.StrFormat("Log file directory:         %s", Settings.sLogDirectory.c_str());
			sLogBuffer += sOutBuf + "\n";
		}


		sLogBuffer += "\n\n[OS/Hardware info]\n";
		sOutBuf = utils.StrFormat("Operating system:           %s\n", sys.GetOSVersion().c_str());
		sLogBuffer += sOutBuf;

		if (sys.bCPUIDSuccess)
		{
			sOutBuf = utils.StrFormat("CPU (brand string):         %s\n", sys.cpudata.CPUBrandString.c_str());
			sLogBuffer += sOutBuf;
			sOutBuf = utils.StrFormat("CPU (code name):            %s\n", sys.cpudata.CPUCodeName.c_str());
			sLogBuffer += sOutBuf;
			sOutBuf = utils.StrFormat("CPU instructions sets:      %s\n", sys.cpudata.CPUSupportedInstructionSets.c_str());
			sLogBuffer += sOutBuf;
			if (sys.cpudata.CPURealClock > 100)
			{
				sOutBuf = utils.StrFormat("CPU clock (measured):       %u MHz\n", sys.cpudata.CPURealClock);
				sLogBuffer += sOutBuf;
			}
			sOutBuf = utils.StrFormat("CPU cores / Logical cores:  %u / %u\n", sys.cpudata.CPUCores, sys.cpudata.CPULogicalCores);
			sLogBuffer += sOutBuf;
		}

		if (Settings.bMonitorGPULoad)
		{
			sOutBuf = utils.StrFormat("Video card:                 %s", gpuinfo.data.CardName.c_str());
			sLogBuffer += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("GPU name:                   %s", gpuinfo.data.GPUName.c_str());
			sLogBuffer += sOutBuf + "\n";
			if (gpuinfo.data.VPEngineName != "")
			{
				sOutBuf = utils.StrFormat("NVIDIA VPU version:         %s", gpuinfo.data.VPEngineName.c_str());
				sLogBuffer += sOutBuf + "\n";
			}
			sOutBuf = utils.StrFormat("Video memory size:          %s", gpuinfo.data.MemSize.c_str());
			sLogBuffer += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("OpenCL version:             %s", gpuinfo.data.OpenCLVersion.c_str());
			sLogBuffer += sOutBuf + "\n";
			sOutBuf = utils.StrFormat("Graphics driver version:    %s\n", gpuinfo.data.DriverVersion.c_str());
			sLogBuffer += sOutBuf;
		}


		sLogBuffer += "\n\n[Avisynth core info]";
		sOutBuf = utils.StrFormat("\nVersionString:              %s", AvisynthInfo.sVersionString.c_str());
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("VersionNumber:              %s", AvisynthInfo.sVersionNumber.c_str());

		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("File version:               %s", AvisynthInfo.sFileVersion.c_str());
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("Interface Version:          %d", AvisynthInfo.iInterfaceVersion);
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("Multi-threading support:    %s", AvisynthInfo.bIsMTVersion ? ("Yes") : ("No"));
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("Linker/compiler version:    %s", AvisynthInfo.sLinkerVersion.c_str());
		sLogBuffer += sOutBuf + "\n";

		sOutBuf = utils.StrFormat("Avisynth.dll location:      %s", AvisynthInfo.sDLLPath.c_str());
		sLogBuffer += sOutBuf + "\n";

		if (Settings.bUseUTC)
			sOutBuf = utils.StrFormat("Avisynth.dll time stamp:    %s", AvisynthInfo.sTimeStampUTC.c_str());
		else
			sOutBuf = utils.StrFormat("Avisynth.dll time stamp:    %s", AvisynthInfo.sTimeStamp.c_str());

		sLogBuffer += sOutBuf + "\n";

		for (unsigned int uiPlugDir = 0; uiPlugDir < AvisynthInfo.vPluginDirs.size(); uiPlugDir++)
		{
			sTemp = AvisynthInfo.vPluginDirs[uiPlugDir];
			if (sTemp != "")
			{
				size_t spos = sTemp.find(":\t");
				sTemp = sTemp.substr(0, spos + 1) + "  " + sTemp.substr(spos + 2);

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

		sOutBuf = "Colorspace:                 ";
		PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
		sLogBuffer += sOutBuf;

		if (!AVS_vidinfo.HasVideo())
			sOutBuf = "      n/a";
		else
		{
			switch (AVS_vidinfo.pixel_type)
			{
				case VideoInfo::CS_YV411:       sOutBuf = "     YV411"; break;
				case VideoInfo::CS_YV24:        sOutBuf = "      YV24"; break;
				case VideoInfo::CS_YV16:        sOutBuf = "      YV16"; break;
				case VideoInfo::CS_Y8:          sOutBuf = "        Y8"; break;
				case VideoInfo::CS_YV12:        sOutBuf = "      YV12"; break;
				case VideoInfo::CS_I420:        sOutBuf = "      i420"; break;
				case VideoInfo::CS_YUY2:        sOutBuf = "      YUY2"; break;
				case VideoInfo::CS_BGR24:       sOutBuf = "     RGB24"; break;
				case VideoInfo::CS_BGR32:       sOutBuf = "     RGB32"; break;
				case VideoInfo::CS_YUV444P16:   sOutBuf = " YUV444P16"; break;
				case VideoInfo::CS_YUV422P16:   sOutBuf = " YUV422P16"; break;
				case VideoInfo::CS_YUV420P16:   sOutBuf = " YUV420P16"; break;
				case VideoInfo::CS_YUV444PS:    sOutBuf = "  YUV444PS"; break;
				case VideoInfo::CS_YUV422PS:    sOutBuf = "  YUV422PS"; break;
				case VideoInfo::CS_YUV420PS:    sOutBuf = "  YUV420PS"; break;
				case VideoInfo::CS_Y16:         sOutBuf = "       Y16"; break;
				case VideoInfo::CS_Y32:         sOutBuf = "       Y32"; break;
				case VideoInfo::CS_YUV444P10:   sOutBuf = " YUV444P10"; break;
				case VideoInfo::CS_YUV422P10:   sOutBuf = " YUV422P10"; break;
				case VideoInfo::CS_YUV420P10:   sOutBuf = " YUV420P10"; break;
				case VideoInfo::CS_Y10:         sOutBuf = "       Y10"; break;
				case VideoInfo::CS_YUV444P12:   sOutBuf = " YUV444P12"; break;
				case VideoInfo::CS_YUV422P12:   sOutBuf = " YUV422P12"; break;
				case VideoInfo::CS_YUV420P12:   sOutBuf = " YUV420P12"; break;
				case VideoInfo::CS_Y12:         sOutBuf = "       Y12"; break;
				case VideoInfo::CS_YUV444P14:   sOutBuf = " YUV444P14"; break;
				case VideoInfo::CS_YUV422P14:   sOutBuf = " YUV422P14"; break;
				case VideoInfo::CS_YUV420P14:   sOutBuf = " YUV420P14"; break;
				case VideoInfo::CS_Y14:         sOutBuf = "       Y14"; break;
				case VideoInfo::CS_BGR48:       sOutBuf = "     BGR48"; break;
				case VideoInfo::CS_BGR64:       sOutBuf = "     BGR64"; break;
				case VideoInfo::CS_RGBP:        sOutBuf = "      RGBP"; break;
				case VideoInfo::CS_RGBP10:      sOutBuf = "    RGBP10"; break;
				case VideoInfo::CS_RGBP12:      sOutBuf = "    RGBP12"; break;
				case VideoInfo::CS_RGBP14:      sOutBuf = "    RGBP14"; break;
				case VideoInfo::CS_RGBP16:      sOutBuf = "    RGBP16"; break;
				case VideoInfo::CS_RGBPS:       sOutBuf = "     RGBPS"; break;
				case VideoInfo::CS_RGBAP:       sOutBuf = "     RGBAP"; break;
				case VideoInfo::CS_RGBAP10:     sOutBuf = "   RGBAP10"; break;
				case VideoInfo::CS_RGBAP12:     sOutBuf = "   RGBAP12"; break;
				case VideoInfo::CS_RGBAP14:     sOutBuf = "   RGBAP14"; break;
				case VideoInfo::CS_RGBAP16:     sOutBuf = "   RGBAP16"; break;
				case VideoInfo::CS_RGBAPS:      sOutBuf = "    RGBAPS"; break;
				case VideoInfo::CS_YUVA444:     sOutBuf = "   YUVA444"; break;
				case VideoInfo::CS_YUVA422:     sOutBuf = "   YUVA422"; break;
				case VideoInfo::CS_YUVA420:     sOutBuf = "   YUVA420"; break;
				case VideoInfo::CS_YUVA444P10:  sOutBuf = "YUVA444P10"; break;
				case VideoInfo::CS_YUVA422P10:  sOutBuf = "YUVA422P10"; break;
				case VideoInfo::CS_YUVA420P10:  sOutBuf = "YUVA420P10"; break;
				case VideoInfo::CS_YUVA444P12:  sOutBuf = "YUVA444P12"; break;
				case VideoInfo::CS_YUVA422P12:  sOutBuf = "YUVA422P12"; break;
				case VideoInfo::CS_YUVA420P12:  sOutBuf = "YUVA420P12"; break;
				case VideoInfo::CS_YUVA444P14:  sOutBuf = "YUVA444P14"; break;
				case VideoInfo::CS_YUVA422P14:  sOutBuf = "YUVA422P14"; break;
				case VideoInfo::CS_YUVA420P14:  sOutBuf = "YUVA420P14"; break;
				case VideoInfo::CS_YUVA444P16:  sOutBuf = "YUVA444P16"; break;
				case VideoInfo::CS_YUVA422P16:  sOutBuf = "YUVA422P16"; break;
				case VideoInfo::CS_YUVA420P16:  sOutBuf = "YUVA420P16"; break;
				case VideoInfo::CS_YUVA444PS:   sOutBuf = " YUVA444PS"; break;
				case VideoInfo::CS_YUVA422PS:   sOutBuf = " YUVA422PS"; break;
				case VideoInfo::CS_YUVA420PS:   sOutBuf = " YUVA420PS"; break;
				default:                        sOutBuf = "  Unknown";
			}
		}

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
			Sleep(100);
			AVS_env->DeleteScriptEnvironment();
			AVS_env = 0;

			if (Settings.bCreateLog)
			{
				string sLogRet = CreateLogFile(sAVSFile, sLogBuffer, sGPUInfo, perfdata, sAVSError, FALSE, bRuntimeTooShort);
				if (sLogRet != "")
					PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, sLogRet.c_str());
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
						Sleep(100);
						AVS_env->DeleteScriptEnvironment();
						AVS_env = 0;

						if (Settings.bCreateLog)
						{
							string sLogRet = CreateLogFile(sAVSFile, sLogBuffer, sGPUInfo, perfdata, sAVSError, FALSE, bRuntimeTooShort);
							if (sLogRet != "")
								PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, sLogRet.c_str());
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

		if (!Settings.bSimpleMode)
		{
			while ((uiFramesToProcess / uiFrameInterval) > 200000)
				uiFrameInterval *= 10;

			perfdata.reserve((uiFramesToProcess / uiFrameInterval) + 2);
		}
		else
		{
			uiFrameInterval = 1;
			while ((uiFramesToProcess / uiFrameInterval) > 200000)
				uiFrameInterval *= 10;
		}


		unsigned int uiFramesRead = 0;
		BOOL bFirstScr = TRUE;
		__int64 iElapsedMS = 0;
		__int64 iEstimatedMS = 0;

		CProcessInfo processinfo;
		DWORD dwPhysicalMemPeakMB = 0;
		DWORD dwVirtualMemPeakMB = 0;
		DWORD dwPhysicalMemCurrentMB = 0;
		DWORD dwVirtualMemCurrentMB = 0;
		unsigned int uiCPUUsageCur = 0;
		unsigned int uiCPUUsageAcc = 0;
		unsigned int uiCPUUsageAvg = 0;
		unsigned int uiGPUUsageCur = 0;
		unsigned int uiGPUUsageAcc = 0;
		unsigned int uiGPUUsageAvg = 0;
		unsigned int uiVPUUsageCur = 0;
		unsigned int uiVPUUsageAcc = 0;
		unsigned int uiVPUUsageAvg = 0;
		unsigned int uiIntervalCounter = 0;

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
			++uiFramesRead;

			if (((uiFramesRead % uiFrameInterval) != 0) && (uiFramesRead != uiFramesToProcess))
				continue;

			dCurrentTime = timer.GetTimer();

			if (!Settings.bSimpleMode)
				processinfo.Update();

			++uiIntervalCounter;

			if ((Settings.bMonitorGPULoad) && (!Settings.bSimpleMode))
			{
				gpuinfo.ReadSensors();
				uiGPUUsageCur = (unsigned int)gpuinfo.sensors.GPULoad;
				uiGPUUsageAcc += uiGPUUsageCur;
				uiGPUUsageAvg = (unsigned int)(((double)uiGPUUsageAcc / (double)uiIntervalCounter) + 0.5);
				uiVPUUsageCur = (unsigned int)gpuinfo.sensors.VPULoad;
				uiVPUUsageAcc += uiVPUUsageCur;
				uiVPUUsageAvg = (unsigned int)(((double)uiVPUUsageAcc / (double)uiIntervalCounter) + 0.5);

				if (gpuinfo.sensors.GPUClock > dwMaxGPUClock)
					dwMaxGPUClock = gpuinfo.sensors.GPUClock;
				if (gpuinfo.sensors.MEMClock > dwMaxGPUMemClock)
					dwMaxGPUMemClock = gpuinfo.sensors.MEMClock;
			}

			iElapsedMS = (__int64)(((dCurrentTime - dStartTime) * 1000.0) + 0.5);
			iEstimatedMS = (__int64)((double)uiFramesToProcess * (double)iElapsedMS / (double)uiFramesRead);

			uiCPUUsageCur = (unsigned int)processinfo.btCPUUsage;
			uiCPUUsageAcc += uiCPUUsageCur;
			uiCPUUsageAvg = (unsigned int)(((double)uiCPUUsageAcc / (double)uiIntervalCounter) + 0.5);

			dwPhysicalMemCurrentMB = processinfo.dwPhysicalMemMB;
			dwVirtualMemCurrentMB = processinfo.dwVirtualMemMB;

			if (dwPhysicalMemCurrentMB > dwPhysicalMemPeakMB)
				dwPhysicalMemPeakMB = dwPhysicalMemCurrentMB;
			if (dwVirtualMemCurrentMB > dwVirtualMemPeakMB)
				dwVirtualMemPeakMB = dwVirtualMemCurrentMB;

			dFPSAverage = (double)uiFramesRead / (dCurrentTime - dStartTime);

			if ((uiFramesRead % uiFrameInterval) != 0)
				continue;

			if ((dCurrentTime - dLastIntervalTime) > 0.000001)
				dFPSCurrent = (double)uiFrameInterval / (dCurrentTime - dLastIntervalTime);
			else
				dFPSCurrent = (double)uiFrameInterval / 0.000001;

			if (dFPSCurrent > dFPSMax)
				dFPSMax = dFPSCurrent;
			if (dFPSCurrent < dFPSMin)
				dFPSMin = dFPSCurrent;

			if (!Settings.bSimpleMode)
			{
				CPerformanceData pdata;
				pdata.current_frame = uiCurrentFrame;
				pdata.fps_current = (float)dFPSCurrent;
				pdata.fps_average = (float)dFPSAverage;
				pdata.cpu_usage = processinfo.btCPUUsage;

				if (Settings.bMonitorGPULoad)
				{
					pdata.gpu_usage = gpuinfo.sensors.GPULoad;
					pdata.vpu_usage = gpuinfo.sensors.VPULoad;
				}
				else
				{
					pdata.gpu_usage = 0;
					pdata.vpu_usage = 0;
				}

				pdata.num_threads = processinfo.wThreadCount;
				pdata.phys_mem = dwPhysicalMemCurrentMB;
				pdata.virt_mem = dwVirtualMemCurrentMB;
				perfdata.push_back(pdata);
			}

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
			bEarlyExit = FALSE;

			sOutBuf = utils.StrFormat("Frame (current | last):         %u | %u", uiCurrentFrame, uiLastFrame);
			PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			++uiCursorOffset;

			if ((Settings.bDisplayFPS) && (!Settings.bSimpleMode))
			{
				sOutBuf = utils.StrFormat("FPS (cur | min | max | avg):    %s | %s | %s | %s", utils.StrFormatFPS(dFPSCurrent).c_str(), utils.StrFormatFPS(dFPSMin).c_str(), utils.StrFormatFPS(dFPSMax).c_str(), utils.StrFormatFPS(dFPSAverage).c_str());
				PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
				++uiCursorOffset;
			}

			if ((Settings.bDisplayTPF) && (!Settings.bSimpleMode))
			{
				sOutBuf = utils.StrFormat("TPF (cur | max | min | avg):    %s | %s | %s | %s ms", utils.StrFormatTPF(1000.0 / dFPSCurrent).c_str(), utils.StrFormatTPF(1000.0 / dFPSMin).c_str(), utils.StrFormatTPF(1000.0 / dFPSMax).c_str(), utils.StrFormatTPF(1000.0 / dFPSAverage).c_str());
				PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
				++uiCursorOffset;
			}

			if (!Settings.bSimpleMode)
			{
				sOutBuf = utils.StrFormat("Memory usage (phys | virt):     %u | %u MiB", dwPhysicalMemCurrentMB, dwVirtualMemCurrentMB);
				PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
				++uiCursorOffset;

				sOutBuf = utils.StrFormat("Thread count:                   %u", processinfo.wThreadCount);
				PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
				++uiCursorOffset;

				sOutBuf = utils.StrFormat("CPU usage (current | average):  %u%% | %u%%", uiCPUUsageCur, uiCPUUsageAvg);
				PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
				++uiCursorOffset;

				if (Settings.bDisplayEfficiencyIndex)
				{
					sOutBuf = utils.StrFormat("Efficiency index:               %s", utils.StrFormatTPF(dFPSAverage / (double)uiCPUUsageAvg).c_str());
					PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					++uiCursorOffset;
				}

				if (Settings.bMonitorGPULoad)
				{
					PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n");
					++uiCursorOffset;

					sOutBuf = utils.StrFormat("GPU core clock | memory clock:  %u | %u", gpuinfo.sensors.GPUClock, gpuinfo.sensors.MEMClock);
					PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					++uiCursorOffset;

					sOutBuf = utils.StrFormat("GPU usage (current | average):  %u%% | %u%%", uiGPUUsageCur, uiGPUUsageAvg);
					PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					++uiCursorOffset;

					if (gpuinfo.data.NVVPU)
					{
						sOutBuf = utils.StrFormat("VPU usage (current | average):  %u%% | %u%%", uiVPUUsageCur, uiVPUUsageAvg);
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
			}

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

		sLogBuffer += "\n\n[Runtime info]\n";

		__int64 iMinElapsed = 500;
		if (Settings.bSimpleMode)
			iMinElapsed = -1;

		if ((uiFramesRead >= uiFrameInterval) && (iElapsedMS >= iMinElapsed))
		{
			if (!bFirstScr)
				utils.CursorUp(uiCursorOffset);

			if (uiFirstFrame == uiLastFrame)
			{
				if (uiFirstFrame == 0)
					sOutBuf = utils.StrFormat("Frames processed:               %u", uiFramesRead);
				else
					sOutBuf = utils.StrFormat("Frames processed:               %u (%u)", uiFramesRead, uiFirstFrame);
			}
			else
				sOutBuf = utils.StrFormat("Frames processed:               %u (%u - %u)", uiFramesRead, uiFirstFrame, uiLastFrame);

			PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
			sLogBuffer += sOutBuf + "\n";

			if (!Settings.bSimpleMode)
			{
				if (Settings.bDisplayFPS)
				{
					sOutBuf = utils.StrFormat("FPS (min | max | average):      %s | %s | %s", utils.StrFormatFPS(dFPSMin).c_str(), utils.StrFormatFPS(dFPSMax).c_str(), utils.StrFormatFPS(dFPSAverage).c_str());
					PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					sLogBuffer += sOutBuf + "\n";
				}

				if (Settings.bDisplayTPF)
				{
					sOutBuf = utils.StrFormat("TPF (max | min | average):      %s | %s | %s ms", utils.StrFormatTPF(1000.0 / dFPSMin).c_str(), utils.StrFormatTPF(1000.0 / dFPSMax).c_str(), utils.StrFormatTPF(1000.0 / dFPSAverage).c_str());
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
					sOutBuf = utils.StrFormat("Efficiency index:               %s", utils.StrFormatTPF(dFPSAverage / (double)uiCPUUsageAvg).c_str());
					PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", utils.StrPad(sOutBuf).c_str());
					sLogBuffer += sOutBuf + "\n";
				}

				if (Settings.bMonitorGPULoad)
				{
					PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n");
					sLogBuffer += "\n";

					sOutBuf = utils.StrFormat("GPU core clock | memory clock:  %u | %u", dwMaxGPUClock, dwMaxGPUMemClock);
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
			}

			if ((Settings.bLogEstimatedTime) && (uiFramesRead < uiFramesToProcess))
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
			bRuntimeTooShort = TRUE;
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
	catch (exception& ex)
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

	AVS_clip = 0;
	AVS_main = 0;
	AVS_temp = 0;
	Sleep(100);
	AVS_env->DeleteScriptEnvironment();
	AVS_env = 0;
	AVS_linkage = 0;
	::FreeLibrary(hDLL);

	if (Settings.bCreateLog)
	{
		string sr = CreateLogFile(sAVSFile, sLogBuffer, sGPUInfo, perfdata, sAVSError, gpuinfo.data.NVVPU, bRuntimeTooShort);
		if (sr != "")
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, sr.c_str());
			Pause();
			return -1;
		}
	}

	if (Settings.bCreateCSV && !bRuntimeTooShort && (sAVSError == ""))
	{
		string cr = CreateCSVFile(sAVSFile, perfdata, gpuinfo.data.NVVPU);
		if (cr != "")
		{
			PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, cr.c_str());
			Pause();
			return -1;
		}
	}

	Pause();

	return iRet;
}


string ParseINIFile()
{
	string sRet = "";
	string sProgramPath = "";
	string sINIFile = "";
	string sTemp = "";

	char szPath[MAX_PATH + 1];
	if (::GetModuleFileName(NULL, szPath, MAX_PATH) > 0)
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
	Settings.bUseUTC = TRUE;
	Settings.bLogEstimatedTime = FALSE;
	Settings.bAutoCompleteExtension = FALSE;

	if (!utils.FileExists(sINIFile)) //No ini file present, create the file with defaults
	{
		if (!WriteINIFile(sINIFile))
	  {
			sRet = utils.StrFormat("\nCannot create \"%s\"\n", sINIFile.c_str());
			return sRet;
		}

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
	int iBoolValue = 0;

	while (getline(hINIFile, sCurrentLine))
	{
		utils.StrTrim(sCurrentLine);
		sOrgLine = sCurrentLine;
		utils.StrToLC(sCurrentLine);

		if ((sCurrentLine.substr(0, 1) == "#") || (sCurrentLine == ""))
			continue;

		iBoolValue = -1;
		if (sCurrentLine.length() > 3)
		{
			if (sCurrentLine.substr(sCurrentLine.length() - 2) == "=0")
				iBoolValue = 0;
			if (sCurrentLine.substr(sCurrentLine.length() - 2) == "=1")
				iBoolValue = 1;
		}

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
			if (iBoolValue == -1)
			{
				sRet = utils.StrFormat("\nINI setting is invalid:\n\"%s\"\nThe value must be \'0\' or \'1\'\n", sOrgLine.c_str());
				return sRet;
			}
			Settings.bDisplayFPS = (iBoolValue == 0) ? FALSE : TRUE;
		}

		if (sCurrentLine.substr(0, 10) == "displaytpf")
		{
			if (iBoolValue == -1)
			{
				sRet = utils.StrFormat("\nINI setting is invalid:\n\"%s\"\nThe value must be \'0\' or \'1\'\n", sOrgLine.c_str());
				return sRet;
			}
			Settings.bDisplayTPF = (iBoolValue == 0) ? FALSE : TRUE;
		}

		if (sCurrentLine.substr(0, 22) == "displayefficiencyindex")
		{
			if (iBoolValue == -1)
			{
				sRet = utils.StrFormat("\nINI setting is invalid:\n\"%s\"\nThe value must be \'0\' or \'1\'\n", sOrgLine.c_str());
				return sRet;
			}
			Settings.bDisplayEfficiencyIndex = (iBoolValue == 0) ? FALSE : TRUE;
		}

		if (sCurrentLine.substr(0, 9) == "createlog")
		{
			if (iBoolValue == -1)
			{
				sRet = utils.StrFormat("\nINI setting is invalid:\n\"%s\"\nThe value must be \'0\' or \'1\'\n", sOrgLine.c_str());
				return sRet;
			}
			Settings.bCreateLog = (iBoolValue == 0) ? FALSE : TRUE;
		}

		if (sCurrentLine.substr(0, 9) == "createcsv")
		{
			if (iBoolValue == -1)
			{
				sRet = utils.StrFormat("\nINI setting is invalid:\n\"%s\"\nThe value must be \'0\' or \'1\'\n", sOrgLine.c_str());
				return sRet;
			}
			Settings.bCreateCSV = (iBoolValue == 0) ? FALSE : TRUE;
		}

		if (sCurrentLine.substr(0, 21) == "logfiledatetimesuffix")
		{
			if (iBoolValue == -1)
			{
				sRet = utils.StrFormat("\nINI setting is invalid:\n\"%s\"\nThe value must be \'0\' or \'1\'\n", sOrgLine.c_str());
				return sRet;
			}
			Settings.bLogFileDateTimeSuffix = (iBoolValue == 0) ? FALSE : TRUE;
		}

		if (sCurrentLine.substr(0, 15) == "pausebeforeexit")
		{
			if (iBoolValue == -1)
			{
				sRet = utils.StrFormat("\nINI setting is invalid:\n\"%s\"\nThe value must be \'0\' or \'1\'\n", sOrgLine.c_str());
				return sRet;
			}
			Settings.bPauseBeforeExit = (iBoolValue == 0) ? FALSE : TRUE;
		}

		if (sCurrentLine.substr(0, 17) == "invokedistributor")
		{
			if (iBoolValue == -1)
			{
				sRet = utils.StrFormat("\nINI setting is invalid:\n\"%s\"\nThe value must be \'0\' or \'1\'\n", sOrgLine.c_str());
				return sRet;
			}
			Settings.bInvokeDistributor = (iBoolValue == 0) ? FALSE : TRUE;
		}

		if (sCurrentLine.substr(0, 20) == "allowonlyoneinstance")
		{
			if (iBoolValue == -1)
			{
				sRet = utils.StrFormat("\nINI setting is invalid:\n\"%s\"\nThe value must be \'0\' or \'1\'\n", sOrgLine.c_str());
				return sRet;
			}
			Settings.bAllowOnlyOneInstance = (iBoolValue == 0) ? FALSE : TRUE;
		}

		if (sCurrentLine.substr(0, 14) == "monitorgpuload")
		{
			if (iBoolValue == -1)
			{
				sRet = utils.StrFormat("\nINI setting is invalid:\n\"%s\"\nThe value must be \'0\' or \'1\'\n", sOrgLine.c_str());
				return sRet;
			}
			Settings.bMonitorGPULoad = (iBoolValue == 0) ? FALSE : TRUE;
		}

		if (sCurrentLine.substr(0, 8) == "usecolor")
		{
			if (iBoolValue == -1)
			{
				sRet = utils.StrFormat("\nINI setting is invalid:\n\"%s\"\nThe value must be \'0\' or \'1\'\n", sOrgLine.c_str());
				return sRet;
			}
			Settings.bUseColor = (iBoolValue == 0) ? FALSE : TRUE;
		}

		if (sCurrentLine.substr(0, 12) == "conusestdout")
		{
			if (iBoolValue == -1)
			{
				sRet = utils.StrFormat("\nINI setting is invalid:\n\"%s\"\nThe value must be \'0\' or \'1\'\n", sOrgLine.c_str());
				return sRet;
			}
			Settings.bConUseStdOut = (iBoolValue == 0) ? FALSE : TRUE;

		}

		if (sCurrentLine.substr(0, 16) == "logestimatedtime")
		{
			if (iBoolValue == -1)
			{
				sRet = utils.StrFormat("\nINI setting is invalid:\n\"%s\"\nThe value must be \'0\' or \'1\'\n", sOrgLine.c_str());
				return sRet;
			}
			Settings.bLogEstimatedTime = (iBoolValue == 0) ? FALSE : TRUE;
		}

		if (sCurrentLine.substr(0, 21) == "autocompleteextension")
		{
			if (iBoolValue == -1)
			{
				sRet = utils.StrFormat("\nINI setting is invalid:\n\"%s\"\nThe value must be \'0\' or \'1\'\n", sOrgLine.c_str());
				return sRet;
			}
			Settings.bAutoCompleteExtension = (iBoolValue == 0) ? FALSE : TRUE;
		}

		if (sCurrentLine.substr(0, 15) == "processpriority")
		{
			sTemp = sCurrentLine.substr(16);
			if (utils.IsNumeric(sTemp))
			{
				Settings.nProcessPriority = (BYTE)atol(sTemp.c_str());
				if ((Settings.nProcessPriority < 1) || (Settings.nProcessPriority > 3))
				{
					sRet = utils.StrFormat("\nINI setting is invalid:\n\"%s\"\nThe value must \'1\', \'2\' or \'3\'\n", sOrgLine.c_str());
					return sRet;
				}				
			}
			else
			{
				sRet = utils.StrFormat("\nINI setting is invalid:\n\"%s\"\nThe value must \'1\', \'2\' or \'3\'\n", sOrgLine.c_str());
				return sRet;
			}
		}

		if (sCurrentLine.substr(0, 9) == "timelimit")
		{
			sTemp = sCurrentLine.substr(10);
			if (utils.IsNumeric(sTemp))
			{
				Settings.iTimeLimit = atoi(sTemp.c_str());
				if (Settings.iTimeLimit != -1)
				{
					if ((Settings.iTimeLimit < 1) || (Settings.iTimeLimit > 999999))
					{
						sRet = utils.StrFormat("\nINI setting is invalid:\n\"%s\"\nValue must be \'-1\' (no time limit) or between \'1\' and \'999999\'\n", sOrgLine.c_str());
						return sRet;
					}
				}
			}
			else
			{
				sRet = utils.StrFormat("\nINI setting is invalid:\n\"%s\"\nValue must be \'-1\' (no time limit) or between \'1\' and \'999999\'\n", sOrgLine.c_str());
				return sRet;
			}
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

		if (sCurrentLine.substr(0, 6) == "useutc")
		{
			if (iBoolValue == -1)
			{
				sRet = utils.StrFormat("\nINI setting is invalid:\n\"%s\"\nThe value must be \'0\' or \'1\'\n", sOrgLine.c_str());
				return sRet;
			}
			Settings.bUseUTC = (iBoolValue == 0) ? FALSE : TRUE;
		}
	}

	hINIFile.close();


	//Write INI file with new settings
	if (!WriteINIFile(sINIFile))
  {
		sRet = utils.StrFormat("\nCannot create \"%s\"\n", sINIFile.c_str());
		return sRet;
	}

	return "";
}


BOOL WriteINIFile(string &s_inifile)
{
	string sCurrentLine = "";
	string sComment = "";

	ofstream hINIFile(s_inifile.c_str());
	if (!hINIFile.is_open())
		return FALSE;

	sComment = "# This INI file can be used to set defaults for AVSMeter or configure settings that are not\n";
	sComment += "# implemented as command line switches.\n";
	sComment += "# Note: Command line switches override INI file settings.\n";
	hINIFile << sComment << sCurrentLine << "\n\n";

	sComment = "# \"LogDirectory\"\n";
	sComment += "# The user can specify a directory in which all log files are stored. This has to be a fully qualified\n";
	sComment += "# path, for example \"LogDirectory=c:\\LogFiles\" (without the quotes). The default is an empty string\n";
	sComment += "# which means that all log files are stored in the script directory or, in case the \"-avsinfo\"\n";
	sComment += "# switch was used, the current (working) directory.\n";
	sCurrentLine = utils.StrFormat("LogDirectory=%s\n", Settings.sLogDirectory.c_str());
	hINIFile << sComment << sCurrentLine << "\n\n";

	sComment = "# \"AVSDLL\"\n";
	sComment += "# The user can specify the avisynth.dll to be used. This has to be a fully qualified path, for example\n";
	sComment += "# \"AVSDLL=d:\\apps\\avisynth\\avisynth.dll\" (without the quotes). The default is an empty string which\n";
	sComment += "# means that AVSMeter will attempt to load avisynth.dll through the standard Windows search hierarchy.\n";
	sCurrentLine = utils.StrFormat("AVSDLL=%s\n", Settings.sAVSDLL.c_str());
	hINIFile << sComment << sCurrentLine << "\n\n";

	sComment = "# \"DisplayFPS\" (0 or 1)\n";
	sComment += "# If set to \"1\", frames/second is printed to the console.\n";
	sCurrentLine = utils.StrFormat("DisplayFPS=%u\n", Settings.bDisplayFPS);
	hINIFile << sComment << sCurrentLine << "\n\n";

	sComment = "# \"DisplayTPF\" (0 or 1):\n";
	sComment += "# If set to \"1\", time/frame (in milliseconds) is printed to the console.\n";
	sCurrentLine = utils.StrFormat("DisplayTPF=%u\n", Settings.bDisplayTPF);
	hINIFile << sComment << sCurrentLine << "\n\n";

	sComment = "# \"DisplayEfficiencyIndex\" (0 or 1):\n";
	sComment += "# If set to \"1\", the result of (FPS / CPU usage) is printed to the console. This ratio indicates the\n";
	sComment += "# efficiency of a script (higher = better) and is useful for comparing different versions of a script\n";
	sComment += "# or scripts that are supposed to do the same thing.\n";
	sCurrentLine = utils.StrFormat("DisplayEfficiencyIndex=%u\n", Settings.bDisplayEfficiencyIndex);
	hINIFile << sComment << sCurrentLine << "\n\n";

	sComment = "# \"CreateLog\" (0 or 1):\n";
	sComment += "# Creates a log file with various details about the installed Avisynth version, clip info, runtime\n";
	sComment += "# results, a copy of the script and a performance data table with the measured details about each\n";
	sComment += "# frame/frame interval. The log file is created in the same directory as the script unless the INI\n";
	sComment += "# option \"LogDirectory\" points to a valid directory.\n";
	sComment += "# Note: The numbering of the frames in the performance data section is not zero-based, i.e. the first\n";
	sComment += "# frame is 1, not 0.\n";
	sCurrentLine = utils.StrFormat("CreateLog=%u\n", Settings.bCreateLog);
	hINIFile << sComment << sCurrentLine << "\n\n";

	sComment = "# \"CreateCSV\" (0 or 1):\n";
	sComment += "# Creates a CSV file containing the performance data formatted as comma-separated values for direct\n";
	sComment += "# import in Excel or a similar spread sheet program (OpenOffice, LibreOffice). The CSV file is created\n";
	sComment += "# in the same directory as the script unless the INI option \"LogDirectory\" points to a valid\n";
	sComment += "# directory.\n";
	sComment += "# Note: The numbering of the frames in the .csv file is not zero-based, i.e. the first frame is 1, not 0.\n";
	sCurrentLine = utils.StrFormat("CreateCSV=%u\n", Settings.bCreateCSV);
	hINIFile << sComment << sCurrentLine << "\n\n";

	sComment = "# \"LogFileDateTimeSuffix\" (0 or 1):\n";
	sComment += "# If set to \"1\", adds a date/time suffix to all .log and .csv files.\n";
	sCurrentLine = utils.StrFormat("LogFileDateTimeSuffix=%u\n", Settings.bLogFileDateTimeSuffix);
	hINIFile << sComment << sCurrentLine << "\n\n";

	sComment = "# \"PauseBeforeExit\" (0 or 1):\n";
	sComment += "# If set to \"1\", pauses the program at the end and returns to the console prompt only after pressing \"ESC\".\n";
	sCurrentLine = utils.StrFormat("PauseBeforeExit=%u\n", Settings.bPauseBeforeExit);
	hINIFile << sComment << sCurrentLine << "\n\n";

	sComment = "# \"FrameRange\" (first,last):\n";
	sComment += "# Specify the range of frames to be processed. \"FrameRange=0,-1\" processes all frames in the script.\n";
	sComment += "# \"FrameRange=1500,1999\" processes 500 frames starting at frame 1500.\n";
	sCurrentLine = utils.StrFormat("FrameRange=%I64d,%I64d\n", Settings.iStartFrame, Settings.iStopFrame);
	hINIFile << sComment << sCurrentLine << "\n\n";

	sComment = "# \"AllowOnlyOneInstance\" (0 or 1):\n";
	sComment += "# If set to \"1\", only one instance of AVSMeter can run at the same time.\n";
	sCurrentLine = utils.StrFormat("AllowOnlyOneInstance=%u\n", Settings.bAllowOnlyOneInstance);
	hINIFile << sComment << sCurrentLine << "\n\n";

	sComment = "# \"MonitorGPULoad\" (0 or 1):\n";
	sComment += "# If set to \"1\", enables display of the GPU/VPU usage which of course is only useful if a filter\n";
	sComment += "# that uses the GPU is in the chain. AVSMeter uses GPU-Z to access the graphics card sensors through a\n";
	sComment += "# shared memory space. GPU-Z has to be running (the window can be minimized to the system tray) before\n";
	sComment += "# running AVSMeter.\n";
	sCurrentLine = utils.StrFormat("MonitorGPULoad=%u\n", Settings.bMonitorGPULoad);
	hINIFile << sComment << sCurrentLine << "\n\n";

	sComment = "# \"ProcessPriority\" (1, 2 or 3):\n";
	sComment += "# Specify the process priority for AVSMeter and all attached processes (1: low, 2: normal, 3: high).\n";
	sCurrentLine = utils.StrFormat("ProcessPriority=%u\n", Settings.nProcessPriority);
	hINIFile << sComment << sCurrentLine << "\n\n";

	sComment = "# \"TimeLimit\" (1 - 999999):\n";
	sComment += "# Specify a time limit (in seconds) after which the program stops reading frames and terminates. A\n";
	sComment += "# setting of \"-1\" disables this feature. This setting is ignored when the program is invoked with\n";
	sComment += "# the \"-i\" (\"-info\") switch which only determines the clip properties.\n";
	sCurrentLine = utils.StrFormat("TimeLimit=%I64d\n", Settings.iTimeLimit);
	hINIFile << sComment << sCurrentLine << "\n\n";

	sComment = "# \"InvokeDistributor\" (0 or 1):\n";
	sComment += "# For users of SEt's Avisynth MT: If set to \"1\", adds the \"Distributor\" call internally if the\n";
	sComment += "# script uses MT Mode 1~4 (via \"SetMTMode\"). Note: Setting this value to \"1\" and adding the\n";
	sComment += "# Distributor call in the script will unnecessarily increase the number of threads which leads to\n";
	sComment += "# increased memory usage and suboptimal performance. The \"InvokeDistributor\" setting is ignored when\n";
	sComment += "# using non-MT versions of Avisynth.\n";
	sCurrentLine = utils.StrFormat("InvokeDistributor=%u\n", Settings.bInvokeDistributor);
	hINIFile << sComment << sCurrentLine << "\n\n";

	sComment = "# \"UseUTC\" (0 or 1):\n";
	sComment += "# If set to \"1\", file timestamps are displayed as UTC (Coordinated Universal Time)\n";
	sCurrentLine = utils.StrFormat("UseUTC=%u\n", Settings.bUseUTC);
	hINIFile << sComment << sCurrentLine << "\n\n";

	sComment = "# \"ConUseSTDOUT\" (0 or 1):\n";
	sComment += "# If set to \"1\", directs all console output to stdout. The default setting \"0\" uses stderr except\n";
	sComment += "# the \"command line usage\" message which always goes through stdout.\n";
	sCurrentLine = utils.StrFormat("ConUseSTDOUT=%u\n", Settings.bConUseStdOut);
	hINIFile << sComment << sCurrentLine << "\n\n";

	sComment = "# \"UseColor\" (0 or 1):\n";
	sComment += "# If set to \"0\", coloring of the console output is disabled.\n";
	sCurrentLine = utils.StrFormat("UseColor=%u\n", Settings.bUseColor);
	hINIFile << sComment << sCurrentLine << "\n\n";

	sComment = "# Individual request (http://forum.doom9.org/showpost.php?p=1639610&postcount=96)\n";
	sCurrentLine = utils.StrFormat("LogEstimatedTime=%u\n", Settings.bLogEstimatedTime);
	hINIFile << sComment << sCurrentLine << "\n\n";

	sComment = "# Individual request (http://forum.doom9.org/showpost.php?p=1728756&postcount=330)\n";
	sCurrentLine = utils.StrFormat("AutoCompleteExtension=%u\n", Settings.bAutoCompleteExtension);
	hINIFile << sComment << sCurrentLine << "\n\n";

	hINIFile.flush();
	hINIFile.close();

	return TRUE;
}


string CreateLogFile(string &s_avsfile, string &s_logbuffer, string &s_gpuinfo, vector<CPerformanceData> &cs_pdata, string &s_avserror, BOOL bNVVP, BOOL bOmitPerfData)
{
	string sRet = "";
	string sAVSBuffer = "";
	string sCurrentLine = "";

	if (Settings.sSystemTime == "")
		Settings.sSystemTime = sys.GetFormattedSystemTime();

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

	if ((cs_pdata.size() > 0) && !bOmitPerfData)
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
		for (unsigned int i = 0; i < cs_pdata.size(); i++)
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

	if (s_avserror != "")
		hLogFile << "\n\n[AviSynth error]\n" + s_avserror + "\n";

	hLogFile.flush();
	hLogFile.close();

	return sRet;
}


string CreateCSVFile(string &s_avsfile, vector<CPerformanceData> &cs_pdata, BOOL bNVVP)
{
	string sRet = "";
	string sCurrentLine = "";
	string sCSVFile = "";

	if (Settings.sSystemTime == "")
		Settings.sSystemTime = sys.GetFormattedSystemTime();

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

	if (cs_pdata.size() > 0)
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
		for (unsigned int i = 0; i < cs_pdata.size(); i++)
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

	if (wAttributes && Settings.bUseColor)
		utils.SetRGB(wAttributes);

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

	if (wAttributes && Settings.bUseColor)
		utils.ResetRGB();

	va_end(args);

	return;
}


unsigned int CalculateFrameInterval(string &s_avsfile)
{
	string sError = "";
	unsigned int uiFrameInterval = 10;

	HINSTANCE hDLL;
	if (Settings.sAVSDLL == "")
		hDLL = ::LoadLibrary("avisynth");
	else
		hDLL = ::LoadLibrary(Settings.sAVSDLL.c_str());

	if (!hDLL)
		return uiFrameInterval;

	IScriptEnvironment *(__stdcall *CreateEnvironment)(int) = (IScriptEnvironment *(__stdcall *)(int))::GetProcAddress(hDLL, "CreateScriptEnvironment");
	if (!CreateEnvironment)
	{
		::FreeLibrary(hDLL);
		return uiFrameInterval;
	}

	IScriptEnvironment *AVS_env = CreateEnvironment(AvisynthInfo.iInterfaceVersion);
	if (!AVS_env)
	{
		::FreeLibrary(hDLL);
		return uiFrameInterval;
	}

	AVS_linkage = AVS_env->GetAVSLinkage();
	AVSValue AVS_main;
	AVSValue AVS_temp;
	PClip AVS_clip;
	VideoInfo	AVS_vidinfo;

	try
	{
		_set_se_translator(SE_Translator);

		AVS_main = AVS_env->Invoke("Import", s_avsfile.c_str());

		if (!AVS_main.IsClip()) //not a clip
			AVS_env->ThrowError("Script did not return a video clip:\n(%s)", s_avsfile.c_str());

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
			iMTMode = 0;
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
		if (dAverageFrameTime >  4.000) uiFrameInterval =    5;
		if (dAverageFrameTime > 10.000) uiFrameInterval =    2;
		if (dAverageFrameTime > 20.000) uiFrameInterval =    1;

		/*
		if (dAverageFrameTime >  0.010) uiFrameInterval = 5000;
		if (dAverageFrameTime >  0.025) uiFrameInterval = 2000;
		if (dAverageFrameTime >  0.050) uiFrameInterval = 1000;
		if (dAverageFrameTime >  0.100) uiFrameInterval =  500;
		if (dAverageFrameTime >  0.250) uiFrameInterval =  200;
		if (dAverageFrameTime >  0.500) uiFrameInterval =  100;
		if (dAverageFrameTime >  1.000) uiFrameInterval =   50;
		if (dAverageFrameTime >  2.500) uiFrameInterval =   20;
		if (dAverageFrameTime >  5.000) uiFrameInterval =   10;
		if (dAverageFrameTime > 10.000) uiFrameInterval =    5;
		if (dAverageFrameTime > 25.000) uiFrameInterval =    2;
		if (dAverageFrameTime > 50.000) uiFrameInterval =    1;
		*/
	}
	catch (AvisynthError err)
	{
		sError = utils.StrFormat("%s", (PCSTR)err.msg);
		uiFrameInterval = 10;
	}
	catch (exception& ex)
	{
		sError = ex.what();
		uiFrameInterval = 10;
	}
	catch (...)
	{
		sError = SysErrorMessage();
		uiFrameInterval = 10;
	}

	AVS_clip = 0;
	AVS_main = 0;
	AVS_temp = 0;
	Sleep(100);
	AVS_env->DeleteScriptEnvironment();
	AVS_env = 0;
	AVS_linkage = 0;

	if (!::FreeLibrary(hDLL))
		uiFrameInterval = 10;

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
	PrintConsole(TRUE, COLOR_EMPHASIS, "  -simple [-s]        Simple mode (w/o statistics)\n\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  For more info on the command line switches and INI file\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  settings read the documentation (AVSMeter.html) included\n");
	PrintConsole(TRUE, COLOR_EMPHASIS, "  in the distribution package.\n");

	PrintConsole(TRUE, COLOR_DEFAULT, "\n");
	return;
}


