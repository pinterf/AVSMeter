/*
  This file is part of AVSMeter, (c) Groucho2004, 2026 pinterf.

  AVSMeter is free software. You can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, either
  version 3 of the License, or any later version.

  AVSMeter is distributed in the hope that it will be useful
  but WITHOUT ANY WARRANTY and without the implied warranty
  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with AVSMeter. If not, see <http://www.gnu.org/licenses/>.
*/


#include "common.h"
#include "AvisynthInfo.h"
#include "Utility.h"
#include "SysInfo.h"
#include "ProcessInfo.h"
#include "Timer.h"
#include "version.h"
#include "platform/console.h"
#include "platform/crash_guard.h"
#include "platform/dynlib.h"
#include "platform/file_info.h"
#include "platform/gpu_sensors.h"
#include "platform/process.h"
#include "platform/single_instance.h"
#include "platform/text.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <thread>


#define COLOR_DEFAULT           0
#define COLOR_AVSM_VERSION      FG_HRED | BG_BLACK
#define COLOR_AVISYNTH_VERSION  FG_HGREEN | BG_BLACK
#define COLOR_EMPHASIS          FG_WHITE | BG_BLACK
#define COLOR_ERROR             FG_HCYAN | BG_BLACK

#define REFRESH_INTERVAL               0.20   //seconds
#define MIN_TIME_PER_FRAMEINTERVAL    20.00   //milliseconds
#define MIN_RUNTIME                  500      //milliseconds

#define DEFAULT_AUDIO_CHUNK         1000      //samples per request of an audio-only clip, like ffmpeg
#define MAX_AUDIO_CHUNK          1000000      //samples

enum BenchMode { BENCH_VIDEO = 0, BENCH_AUDIO, BENCH_AV };

struct stSettings
{
  std::string sLogDirectory;
  std::string sAVSDLL;
  bool      bDisplayFPS;
  bool      bDisplayTPF;
  bool      bCreateLog;
  bool      bCreateCSV;
  bool      bPauseBeforeExit;
  int64_t   iStartFrame;
  int64_t   iStopFrame;
  std::string sFrameRange;
  int64_t   iTimeLimit;
  bool      bInvokeDistributor;
  bool      bAllowOnlyOneInstance;
  bool      bUseColor;
  bool      bGPUInfo;
  std::string sGPUSelect;   // -gpu=<selector>, lower case; empty: default GPU
  bool      bConUseStdOut;
  std::string sSystemDateTime;
  bool      bLogEstimatedTime;
  bool      bAutoCompleteExtension;
  bool      bLogFileDateTimeSuffix;
  bool      bDisableFFTWDLLWarning;
  size_t    nLoadPluginInterval;
  int       iBenchMode;
  int64_t   iAudioChunk;
} Settings;


struct stPerfData
{
  unsigned int  frame;
  float         fps_current;
  float         fps_average;
  float         cpu_usage;
  BYTE          gpu_usage;
  BYTE          vpu_usage;
  uint32_t      process_memory;
  uint16_t      num_threads;
};

struct stCmdLine
{
  std::string sCommandLine;
  std::string sAVSFile;
  bool      bModeAVSInfo = false;
  bool      bInfoOnly = false;
  bool      bOmitPreScan = false;
  bool      bLogFunctions = false;
  bool      bHighPriority = false;
};

struct stRunStats
{
  unsigned int  uiFirstFrame = 0;
  unsigned int  uiLastFrame = 0;
  unsigned int  uiFramesToProcess = 0;
  unsigned int  uiFramesRead = 0;
  bool          bFirstScr = true;
  unsigned int  uiCursorOffset = 0;
  int64_t       iElapsedMS = 0;
  double        dElapsedSec = 0.0;
  int64_t       iEstimatedMS = 0;
  uint32_t      dwMemPeakMB = 0;
  uint16_t      wThreadCount = 0;
  double        dCPUUsageAvg = 0;
  unsigned int  uiGPUUsageAvg = 0;
  unsigned int  uiVPUUsageAvg = 0;
  double        dGPUPowerConsumptionAvg = 0.0;
  double        dFPSAverage = 0.0;
  double        dFPSMin = 1.0e+20;
  double        dFPSMax = 0.0;
};

// What one benchmark step (unit) requests: a video frame, its audio samples, or both.
// Audio-only clips have no frames, there a unit is a chunk of samples.
struct stBenchUnits
{
  bool          bGetFrame = true;
  bool          bGetAudio = false;
  bool          bFrameUnits = true;
  int64_t       iChunk = 0;          //samples per GetAudio call, 0: one call per frame
  unsigned int  uiUnits = 0;
  int64_t       iSamplesRead = 0;
  int64_t       iFirstSample = -1;
  int64_t       iLastSample = -1;
  std::vector<uint8_t> vBuffer;
  VideoInfo     vi;

  const char *UnitName() const { return bFrameUnits ? "Frame" : "Chunk"; }
  const char *RateName() const { return bFrameUnits ? "FPS" : "CPS"; }
  const char *TimeName() const { return bFrameUnits ? "TPF" : "TPC"; }
};

typedef IScriptEnvironment * __stdcall CREATE_ENV(int);

static CUtils utils;
static CTimer timer;
static CAvisynthInfo AvisynthInfo;
static CSysInfo sys;


unsigned int CalculateFrameInterval(std::string &s_avsfile, std::string &s_error);
std::string  CreateLogFile(std::string &s_avsfile, std::string &s_logbuffer, std::string &s_gpuinfo, std::vector<stPerfData> &cs_pdata, std::string &s_avserror, bool bNVVP, bool bOmitstPerfData, bool bChunkUnits);
std::string  CreateCSVFile(std::string &s_avsfile, std::vector<stPerfData> &cs_pdata, bool bNVVP, bool bChunkUnits);
std::string  ParseINIFile(std::string &s_warning);
bool         WriteINIFile(std::string &s_inifile);
void         PrintUsage();
void         PollKeys();
void         PrintConsole(bool bUseStdOut, uint16_t wAttributes, const char *fmt, ...);
std::string  Pad(std::string s_line);
static void  SetDefaultSettings();
static bool  ParseCommandLine(const std::vector<std::string> &args, stCmdLine &cmd);
static void  AppendSystemInfo(std::string &sLogBuffer, platform::GpuSensors &gpuinfo);
static int   RunAVSInfoMode(const std::string &sAVSMVersion, const stCmdLine &cmd, platform::GpuSensors &gpuinfo);
static std::string FormatPluginDirLine(const std::string &sPluginDir);
static void  AppendScriptLogHeader(std::string &sLogBuffer, const std::string &sAVSMVersion, const stCmdLine &cmd, platform::GpuSensors &gpuinfo);
static void  ReportClipInfo(const VideoInfo &AVS_vidinfo, bool bAudioOnly, int64_t iMilliSeconds, bool bIsSETMTVersion, int iMTMode, std::string &sLogBuffer);
static void  InitBenchUnits(const VideoInfo &vi, IScriptEnvironment *AVS_env, const std::string &sAVSFile, stBenchUnits &bu);
static void  ProcessUnit(const PClip &AVS_clip, IScriptEnvironment *AVS_env, unsigned int uiUnit, stBenchUnits &bu);
static void  RunBenchmark(const PClip &AVS_clip, IScriptEnvironment *AVS_env, platform::GpuSensors &gpuinfo, unsigned int uiFrameInterval, unsigned int uiLogFrameInterval, std::vector<stPerfData> &perfdata, bool &bEarlyExit, stRunStats &rs, stBenchUnits &bu);
static void  ReportRuntime(const stRunStats &rs, const stBenchUnits &bu, platform::GpuSensors &gpuinfo, unsigned int uiFrameInterval, std::string &sLogBuffer, bool &bRuntimeTooShort);



int main(int argc, char* argv[])
{
  platform::SuppressErrorDialogs();

  SetDefaultSettings();

  std::string sINIWarning;
  std::string sINIRet = ParseINIFile(sINIWarning);

  AvisynthInfo.nLoadPlugInterval = Settings.nLoadPluginInterval;

  int iRet = 0;

  std::string sAVSMVersion = VERSION_STR;
  sAVSMVersion += utils.StrFormat(" (%s)", kArchName);

  PrintConsole(Settings.bConUseStdOut, COLOR_AVSM_VERSION, "\nAVSMeter %s, %s\n", sAVSMVersion.c_str(), COPYRIGHT_STR);

  if (sINIRet != "")
  {
    PrintConsole(true, COLOR_ERROR, sINIRet.c_str());
    PollKeys();
    return -1;
  }

  if (sINIWarning != "")
    PrintConsole(true, COLOR_ERROR, sINIWarning.c_str());

  std::vector<stPerfData> perfdata;
  std::string sOutBuf = "";
  std::string sLogBuffer = "";
  bool   bRuntimeTooShort = false;
  std::string sGPUInfo = "";
  bool bEarlyExit = true;
  std::string sAVSError = "";
  unsigned int uiFrameInterval = 1;
  unsigned int uiLogFrameInterval = 1;
  std::string sErrorMsg = "";

  Settings.sSystemDateTime = "";

  if (Settings.bAllowOnlyOneInstance)
  {
    const char *pszLock = PROCESS_64 ? "__avsmeter64__single__instance__lock__" : "__avsmeter32__single__instance__lock__";
    if (!platform::AcquireSingleInstance(pszLock))
    {
      PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nError: One instance of AVSMeter is already running\n");
      PollKeys();
      return -1;
    }
  }

  std::vector<std::string> args = platform::CommandLineArgs(argc, argv);
  if (args.size() < 2)
  {
    PrintUsage();
    PollKeys();
    return -1;
  }

  stCmdLine cmd;
  if (!ParseCommandLine(args, cmd))
  {
    PollKeys();
    return -1;
  }


  if (cmd.bHighPriority && !platform::SetHighPriority())
  {
    std::string sReason = platform::LastErrorMessage();
    PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nWarning: Cannot set high process priority%s%s\n", sReason.empty() ? "" : ": ", sReason.c_str());
  }

  PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("Query Avisynth info...").c_str());

  bool bRet = false;
  if (cmd.bModeAVSInfo)
    bRet = AvisynthInfo.GetInfo(Settings.sAVSDLL, sErrorMsg, true);
  else
    bRet = AvisynthInfo.GetInfo(Settings.sAVSDLL, sErrorMsg, false);

  sys.GetCPUID();

  if (!bRet)
  {
    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());
    PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\n%s\n", sErrorMsg.c_str());
    PollKeys();
    return -1;
  }

  if (!cmd.bModeAVSInfo)
  {
    PrintConsole(Settings.bConUseStdOut, COLOR_AVISYNTH_VERSION, "\r%s", AvisynthInfo.sVersionString.c_str());
    PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, " (%s)\n", AvisynthInfo.sFileVersion.c_str());
  }

  platform::GpuSensors gpuinfo;
  if (Settings.bGPUInfo)
  {
    gpuinfo.Init(Settings.sGPUSelect);
    if (!gpuinfo.bInitialized)
    {
      PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());
      if (!gpuinfo.sNote.empty())
        PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\nWarning: %s\n", gpuinfo.sNote.c_str());
      PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nError: %s\n", gpuinfo.sError.c_str());
      PollKeys();
      return -1;
    }
  }

  PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());

  if (Settings.bGPUInfo && !gpuinfo.sNote.empty())
    PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\nWarning: %s\n", gpuinfo.sNote.c_str());

  if (cmd.bModeAVSInfo)
    return RunAVSInfoMode(sAVSMVersion, cmd, gpuinfo);

  if (cmd.sAVSFile == "")
  {
    PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nError: No script file specified\n");
    PollKeys();
    return -1;
  }

  if (!cmd.bInfoOnly)
  {
    if (!cmd.bOmitPreScan)
    {
      uiFrameInterval = CalculateFrameInterval(cmd.sAVSFile, sErrorMsg);
      if (sErrorMsg != "")
      {
        PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", Pad("").c_str());
        PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "%s\n", sErrorMsg.c_str());
        PollKeys();
        return -1;
      }
    }

    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());
  }


  platform::LibHandle hDLL;
  if (Settings.sAVSDLL == "")
    hDLL = platform::LoadLib(platform::kAvsLibName);
  else
    hDLL = platform::LoadLib(Settings.sAVSDLL.c_str());


  if (!hDLL)
  {
    PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nError: Cannot load %s:\n%s\n", platform::kAvsLibFileName, utils.SysErrorMessage().c_str());
    PollKeys();
    return -1;
  }


  IScriptEnvironment *AVS_env = 0;
  bool bChunkUnits = false;
  try
  {
    platform::TranslateCrashes();

    CREATE_ENV *CreateEnvironment = (CREATE_ENV *)platform::GetLibSymbol(hDLL, "CreateScriptEnvironment");
    if (!CreateEnvironment)
    {
      PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nError: Failed to load CreateScriptEnvironment()\n");
      PollKeys();
      return -1;
    }

    AVS_env = CreateEnvironment(AvisynthInfo.iInterfaceVersion);

    if (!AVS_env)
    {
      PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nError: Could not create IScriptenvironment\n");
      PollKeys();
      return -1;
    }

    AVS_linkage = AVS_env->GetAVSLinkage();
    AVSValue AVS_main;
    AVSValue AVS_temp;
    PClip AVS_clip;
    VideoInfo   AVS_vidinfo;

    AVS_main = InvokeWithPath(AVS_env, "Import", cmd.sAVSFile, AvisynthInfo.bImportUtf8);

    if (!AVS_main.IsClip())
      AVS_env->ThrowError("\"%s\":\nScript did not return a clip", cmd.sAVSFile.c_str());

    bool bIsSETMTVersion = true;
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
      bIsSETMTVersion = false;
    }

    AVS_clip = AVS_main.AsClip();
    AVS_vidinfo = AVS_clip->GetVideoInfo();

    bool bAudioOnly = false;
    if (!AVS_vidinfo.HasVideo() && AVS_vidinfo.HasAudio())
      bAudioOnly = true;

    unsigned int uiFrames = 0;
    int64_t iMilliSeconds = 0;
    if (!bAudioOnly)
    {
      uiFrames = (unsigned int)AVS_vidinfo.num_frames;
      iMilliSeconds = (int64_t)((((double)uiFrames * (double)AVS_vidinfo.fps_denominator * 1000.0) / (double)AVS_vidinfo.fps_numerator) + 0.5);
    }

    AppendScriptLogHeader(sLogBuffer, sAVSMVersion, cmd, gpuinfo);

    ReportClipInfo(AVS_vidinfo, bAudioOnly, iMilliSeconds, bIsSETMTVersion, iMTMode, sLogBuffer);

    if (cmd.bInfoOnly)
    {
      AVS_clip = 0;
      AVS_main = 0;
      AVS_temp = 0;
      AVS_env->DeleteScriptEnvironment();
      AVS_env = 0;

      if (Settings.bCreateLog)
      {
        std::string sLogRet = CreateLogFile(cmd.sAVSFile, sLogBuffer, sGPUInfo, perfdata, sAVSError, false, bRuntimeTooShort, false);
        if (sLogRet != "")
          PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, sLogRet.c_str());
      }

      AVS_linkage = 0;
      platform::FreeLib(hDLL);

      PollKeys();
      return 0;
    }

    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n");

    stBenchUnits bu;
    InitBenchUnits(AVS_vidinfo, AVS_env, cmd.sAVSFile, bu);
    bChunkUnits = !bu.bFrameUnits;

    stRunStats rs;
    rs.uiLastFrame = bu.uiUnits - 1;

    if (Settings.iStopFrame == -1)
      Settings.iStopFrame = (int64_t)bu.uiUnits - 1;

    if ((Settings.iStartFrame >= 0) && (Settings.iStartFrame <= Settings.iStopFrame) && (Settings.iStopFrame < (int64_t)bu.uiUnits))
    {
      rs.uiFirstFrame = (unsigned int)Settings.iStartFrame;
      rs.uiLastFrame = (unsigned int)Settings.iStopFrame;
      rs.uiFramesToProcess = rs.uiLastFrame - rs.uiFirstFrame + 1;

      sOutBuf = utils.StrFormat("%s (current | last):         %u | %u", bu.UnitName(), rs.uiFirstFrame, rs.uiLastFrame);
      PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\r", Pad(sOutBuf).c_str());
    }
    else
    {
      PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", Pad("").c_str());
      AVS_env->ThrowError("Invalid frame range specified:\n\"%s\"\n", Settings.sFrameRange.c_str());
    }


    uiLogFrameInterval = uiFrameInterval;
    while ((rs.uiFramesToProcess / uiLogFrameInterval) > 10000)
      uiLogFrameInterval *= 10;

    perfdata.reserve((rs.uiFramesToProcess / uiLogFrameInterval) + 100);

    if (cmd.bOmitPreScan)
    {
      while ((rs.uiFramesToProcess / uiFrameInterval) > 1000000)
        uiFrameInterval *= 10;
    }

    RunBenchmark(AVS_clip, AVS_env, gpuinfo, uiFrameInterval, uiLogFrameInterval, perfdata, bEarlyExit, rs, bu);

    ReportRuntime(rs, bu, gpuinfo, uiFrameInterval, sLogBuffer, bRuntimeTooShort);

    AVS_clip = 0;
    AVS_main = 0;
    AVS_temp = 0;
    AVS_env->DeleteScriptEnvironment();
    AVS_env = 0;
    AVS_linkage = 0;
  }
  catch (AvisynthError err)
  {
    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());
    sAVSError = platform::ToUtf8(err.msg);
    iRet = -1;

    if (bEarlyExit == false)
    {
      platform::ConsoleCursorUp(2);
      PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", Pad("").c_str());
      PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", Pad("").c_str());
      platform::ConsoleCursorUp(2);
      PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\r%s\n", sAVSError.c_str());
    }
    else
      PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\n%s\n", sAVSError.c_str());
  }
  catch (std::exception& ex)
  {
    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());
    iRet = -1;
    if (bEarlyExit == false)
    {
      platform::ConsoleCursorUp(2);
      PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", Pad("").c_str());
      PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", Pad("").c_str());
      platform::ConsoleCursorUp(2);
    }

    sAVSError = ex.what();
    if (sAVSError == "")
      PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nUnknown exception\n");
    else
      PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\n%s\n", sAVSError.c_str());

  }
  catch (...)
  {
    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());
    iRet = -1;
    if (bEarlyExit == false)
    {
      platform::ConsoleCursorUp(2);
      PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", Pad("").c_str());
      PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", Pad("").c_str());
      platform::ConsoleCursorUp(2);
    }

    sAVSError = utils.SysErrorMessage();
    if (sAVSError == "")
      PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nUnknown exception\n");
    else
      PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\n%s\n", sAVSError.c_str());
  }

  platform::FreeLib(hDLL);

  if (Settings.bCreateLog)
  {
    std::string sr = CreateLogFile(cmd.sAVSFile, sLogBuffer, sGPUInfo, perfdata, sAVSError, gpuinfo.data.NVVPU, bRuntimeTooShort, bChunkUnits);
    if (sr != "")
    {
      PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, sr.c_str());
      PollKeys();
      return -1;
    }
  }

  if (Settings.bCreateCSV && !bRuntimeTooShort && (sAVSError == ""))
  {
    std::string cr = CreateCSVFile(cmd.sAVSFile, perfdata, gpuinfo.data.NVVPU, bChunkUnits);
    if (cr != "")
    {
      PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, cr.c_str());
      PollKeys();
      return -1;
    }
  }

  PollKeys();

  return iRet;
}


static void SetDefaultSettings()
{
  Settings.sLogDirectory = "";
  Settings.sAVSDLL = "";
  Settings.bDisplayFPS = true;
  Settings.bDisplayTPF = false;
  Settings.bCreateLog = false;
  Settings.bCreateCSV = false;
  Settings.bLogFileDateTimeSuffix = false;
  Settings.bPauseBeforeExit = false;
  Settings.iStartFrame = 0;
  Settings.iStopFrame = -1;
  Settings.bInvokeDistributor = true;
  Settings.bAllowOnlyOneInstance = true;
  Settings.bUseColor = true;
  Settings.bGPUInfo = false;
  Settings.sGPUSelect = "";
  Settings.bConUseStdOut = false;
  Settings.iTimeLimit = -1;
  Settings.bLogEstimatedTime = false;
  Settings.bAutoCompleteExtension = false;
  Settings.bDisableFFTWDLLWarning = false;
  Settings.nLoadPluginInterval = 40;
  Settings.iBenchMode = BENCH_VIDEO;
  Settings.iAudioChunk = 0;
}


static bool ParseCommandLine(const std::vector<std::string> &args, stCmdLine &cmd)
{
  std::string sCmdToken = "";
  bool CLSwitches_timelimit = false;
  bool CLSwitches_highpriority = false;
  bool CLSwitches_range = false;
  bool CLSwitches_csv = false;
  bool CLSwitches_info = false;
  bool CLSwitches_o = false;
  bool CLSwitches_lf = false;
  bool CLSwitches_audio = false;
  bool CLSwitches_av = false;
  bool CLSwitches_chunk = false;

  std::string sArg = "";
  std::string sArgTest = "";
  std::string sTemp = "";
  size_t arg_len = 0;
  for (size_t iArg = 1; iArg < args.size(); iArg++)
  {
    sArg = args[iArg];
    sArgTest = sArg;
    utils.StrTrim(sArgTest);
    sCmdToken = sArgTest;
    utils.StrToLC(sArgTest);
    arg_len = sArgTest.length();

    if (sArgTest == "-avsinfo")
    {
      cmd.sCommandLine += sCmdToken + " ";
      cmd.bModeAVSInfo = true;
      continue;
    }

    if (sArgTest == "-o")
    {
      cmd.sCommandLine += sCmdToken + " ";
      CLSwitches_o = true;
      cmd.bOmitPreScan = true;
      continue;
    }

    if (sArgTest == "-lf")
    {
      cmd.sCommandLine += sCmdToken + " ";
      CLSwitches_lf = true;
      cmd.bLogFunctions = true;
      Settings.bCreateLog = true;
      continue;
    }

    if (sArgTest == "-p")
    {
      cmd.sCommandLine += sCmdToken + " ";
      Settings.bPauseBeforeExit = true;
      continue;
    }

    if ((sArgTest == "-i") || (sArgTest == "-info"))
    {
      cmd.sCommandLine += sCmdToken + " ";
      CLSwitches_info = true;
      cmd.bInfoOnly = true;
      continue;
    }

    if ((sArgTest == "-l") || (sArgTest == "-log"))
    {
      cmd.sCommandLine += sCmdToken + " ";
      Settings.bCreateLog = true;
      continue;
    }

    if (sArgTest == "-csv")
    {
      cmd.sCommandLine += sCmdToken + " ";
      CLSwitches_csv = true;
      Settings.bCreateCSV = true;
      continue;
    }

    if ((sArgTest == "-gpu") || (sArgTest.substr(0, 5) == "-gpu="))
    {
      cmd.sCommandLine += sCmdToken + " ";
      if (sArgTest == "-gpu=")
      {
        PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nError: Invalid parameter format: \"%s\"\n", sArg.c_str());
        PrintUsage();
        return false;
      }
      Settings.bGPUInfo = true;
      Settings.sGPUSelect = (sArgTest.length() > 5) ? sArgTest.substr(5) : "";
      continue;
    }

    if (sArgTest.substr(0, 7) == "-range=")
    {
      cmd.sCommandLine += sCmdToken + " ";
      CLSwitches_range = true;
      Settings.iStartFrame = 0xFFFFFFFFFF;
      Settings.iStopFrame = 0xFFFFFFFFFF;

      sTemp = sArgTest.substr(7);
      if (sTemp.length() < 3)
      {
        PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nError: Invalid parameter format: \"%s\"\n", sArg.c_str());
        PrintUsage();
        return false;
      }

      size_t spos = sTemp.find(",");
      if ((spos < 1) || (spos > 27))
      {
        PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nError: Invalid parameter format: \"%s\"\n", sArg.c_str());
        PrintUsage();
        return false;
      }

      Settings.sFrameRange = "FrameRange=" + sTemp;

      if ((utils.IsNumeric(sTemp.substr(0, spos))) && (utils.IsNumeric(sTemp.substr(spos + 1))))
      {
        Settings.iStartFrame = std::strtoll(sTemp.substr(0, spos).c_str(), nullptr, 10);
        Settings.iStopFrame = std::strtoll(sTemp.substr(spos + 1).c_str(), nullptr, 10);
      }
      else
      {
        PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nError: Invalid parameter format: \"%s\"\n", sArg.c_str());
        PrintUsage();
        return false;
      }
      continue;
    }

    if (sArgTest == "-hp")
    {
      cmd.sCommandLine += sCmdToken + " ";
      CLSwitches_highpriority = true;
      cmd.bHighPriority = true;
      continue;
    }

    if (sArgTest == "-audio")
    {
      cmd.sCommandLine += sCmdToken + " ";
      CLSwitches_audio = true;
      Settings.iBenchMode = BENCH_AUDIO;
      continue;
    }

    if (sArgTest == "-av")
    {
      cmd.sCommandLine += sCmdToken + " ";
      CLSwitches_av = true;
      Settings.iBenchMode = BENCH_AV;
      continue;
    }

    if (sArgTest.substr(0, 7) == "-chunk=")
    {
      cmd.sCommandLine += sCmdToken + " ";
      CLSwitches_chunk = true;
      sTemp = sArgTest.substr(7);
      if (utils.IsNumeric(sTemp) && (sTemp.length() <= 7))
        Settings.iAudioChunk = std::strtoll(sTemp.c_str(), nullptr, 10);
      if ((Settings.iAudioChunk < 1) || (Settings.iAudioChunk > MAX_AUDIO_CHUNK))
      {
        PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid parameter value: \"%s\"\nValue must be between \'1\' and \'%d\'\n", sArg.c_str(), MAX_AUDIO_CHUNK);
        return false;
      }
      continue;
    }

    if (sArgTest.substr(0, 11) == "-timelimit=")
    {
      cmd.sCommandLine += sCmdToken + " ";
      CLSwitches_timelimit = true;
      sTemp = sArgTest.substr(11);
      if (utils.IsNumeric(sTemp))
      {
        Settings.iTimeLimit = atoi(sTemp.c_str());
        if ((Settings.iTimeLimit < 1) || (Settings.iTimeLimit > 999999))
        {
          PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid parameter value: \"%s\"\nValue must be between \'1\' and \'999999\'\n", sArg.c_str());
          return false;
        }
      }
      else
      {
        PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nInvalid parameter value: \"%s\"\nValue must be \'-1\' (no time limit) or between \'1\' and \'999999\'\n", sArg.c_str());
        return false;
      }

      continue;
    }

    if (arg_len > 4)
    {
      if (sArgTest.substr(sArgTest.length() - 4) == ".avs")
      {
        cmd.sAVSFile = platform::AbsolutePath(sArg);
        if (cmd.sAVSFile != "")
        {
          if (!platform::FileExists(cmd.sAVSFile))
          {
            PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nError: File not found: \"%s\"\n", cmd.sAVSFile.c_str());
            return false;
          }
          else
            continue;
        }
      }
    }

    if ((Settings.bAutoCompleteExtension) && (cmd.sAVSFile == ""))
    {
      if (arg_len > 0)
      {
        sTemp = sArg + ".avs";
        cmd.sAVSFile = platform::AbsolutePath(sTemp);
        if (cmd.sAVSFile != "")
        {
          if (!platform::FileExists(cmd.sAVSFile))
          {
            PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nError: File not found: \"%s\"\n", sTemp.c_str());
            return false;
          }
          else
            continue;
        }
      }
    }

    PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nError: Invalid argument: \"%s\"\n", sArg.c_str());
    PrintUsage();
    return false;
  }

  utils.StrTrim(cmd.sCommandLine);


  if (cmd.bModeAVSInfo)
  {
    if (CLSwitches_info)
    {
      PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nError: Invalid switch in combination with \'-avsinfo\': \"-info [-i]\"\n");
      PrintUsage();
      return false;
    }

    if (CLSwitches_highpriority)
    {
      PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nError: Invalid switch in combination with \'-avsinfo\': \"-hp\"\n");
      PrintUsage();
      return false;
    }

    if (CLSwitches_timelimit)
    {
      PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nError: Invalid switch in combination with \'-avsinfo\': \"-timelimit\"\n");
      PrintUsage();
      return false;
    }

    if (CLSwitches_range)
    {
      PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nError: Invalid switch in combination with \'-avsinfo\': \"-range\"\n");
      PrintUsage();
      return false;
    }

    if (CLSwitches_csv)
    {
      PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nError: Invalid switch in combination with \'-avsinfo\': \"-csv\"\n");
      PrintUsage();
      return false;
    }

    if (CLSwitches_o)
    {
      PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nError: Invalid switch in combination with \'-avsinfo\': \"-o\"\n");
      PrintUsage();
      return false;
    }

    if (CLSwitches_audio || CLSwitches_av || CLSwitches_chunk)
    {
      PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nError: Invalid switch in combination with \'-avsinfo\': \"%s\"\n", CLSwitches_audio ? "-audio" : (CLSwitches_av ? "-av" : "-chunk"));
      PrintUsage();
      return false;
    }

    if (cmd.sAVSFile != "")
    {
      PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nError: Script specified together with \'avsinfo\'\n");
      PrintUsage();
      return false;
    }
  }
  else
  {
    if (CLSwitches_lf)
    {
      PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nError: Switch \'-lf\' must be used in combination with \'avsinfo\'\n");
      PrintUsage();
      return false;
    }

    if (CLSwitches_audio && CLSwitches_av)
    {
      PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nError: Switches \'-audio\' and \'-av\' cannot be combined\n");
      PrintUsage();
      return false;
    }

    if (CLSwitches_chunk && !CLSwitches_audio && !CLSwitches_av)
    {
      PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nError: Switch \'-chunk\' must be used in combination with \'-audio\' or \'-av\'\n");
      PrintUsage();
      return false;
    }
  }

  return true;
}


static void AppendSystemInfo(std::string &sLogBuffer, platform::GpuSensors &gpuinfo)
{
  std::string sOutBuf = "";

  sLogBuffer += "\n[OS/Hardware info]\n";
  sOutBuf = utils.StrFormat("Operating system:           %s\n\n", sys.GetOSVersion().c_str());
  sLogBuffer += sOutBuf;

  if (sys.bCPUIDSuccess)
  {
    if (sys.cpudata.CPUCodeName.empty())
      sOutBuf = utils.StrFormat("CPU:                        %s\n", sys.cpudata.CPUBrandString.c_str());
    else
      sOutBuf = utils.StrFormat("CPU:                        %s / %s\n", sys.cpudata.CPUBrandString.c_str(), sys.cpudata.CPUCodeName.c_str());
    sLogBuffer += sOutBuf;
    sOutBuf = utils.StrFormat("                            %s\n", sys.cpudata.CPUSupportedInstructionSets.c_str());
    sLogBuffer += sOutBuf;
    sOutBuf = utils.StrFormat("                            %u physical cores / %u logical cores\n", sys.cpudata.CPUCores, sys.cpudata.CPULogicalCores);
    sLogBuffer += sOutBuf;
  }

  if (Settings.bGPUInfo)
  {
    sOutBuf = utils.StrFormat("\nVideo card:                 %s", gpuinfo.data.CardName.c_str());
    sLogBuffer += sOutBuf + "\n";
    sOutBuf = utils.StrFormat("GPU version:                %s", gpuinfo.data.GPUName.c_str());
    sLogBuffer += sOutBuf + "\n";
    const std::string &sMem = gpuinfo.data.MemSize;
    bool bMiB = !sMem.empty() && (sMem[0] >= '0') && (sMem[0] <= '9');
    sOutBuf = utils.StrFormat("Video memory size:          %s%s", sMem.c_str(), bMiB ? " MiB" : "");
    sLogBuffer += sOutBuf + "\n";
    if (!gpuinfo.data.OpenCLVersion.empty())
    {
      sOutBuf = utils.StrFormat("OpenCL version:             %s", gpuinfo.data.OpenCLVersion.c_str());
      sLogBuffer += sOutBuf + "\n";
    }
    if (!gpuinfo.data.CudaVersion.empty())
    {
      sOutBuf = utils.StrFormat("CUDA version:               %s", gpuinfo.data.CudaVersion.c_str());
      sLogBuffer += sOutBuf + "\n";
    }
    sOutBuf = utils.StrFormat("Graphics driver version:    %s\n", gpuinfo.data.DriverVersion.c_str());
    sLogBuffer += sOutBuf;
  }
}


static int RunAVSInfoMode(const std::string &sAVSMVersion, const stCmdLine &cmd, platform::GpuSensors &gpuinfo)
{
  std::string sOutBuf = "";
  std::string sLogBuffer = "";

  sOutBuf = utils.StrFormat("Log file created with:      AVSMeter %s\n", sAVSMVersion.c_str());
  sLogBuffer += sOutBuf;

  if (cmd.sCommandLine != "")
  {
    sOutBuf = utils.StrFormat("Command line switches:      %s", cmd.sCommandLine.c_str());
    sLogBuffer += sOutBuf + "\n";
  }


  AppendSystemInfo(sLogBuffer, gpuinfo);

  sLogBuffer += "\n\n[Avisynth info]";
  sOutBuf = utils.StrFormat("\nVersionString:              %s\n", AvisynthInfo.sVersionString.c_str());
  sLogBuffer += sOutBuf;
  PrintConsole(true, COLOR_EMPHASIS, sOutBuf.c_str());

  sOutBuf = utils.StrFormat("VersionNumber:              %s\n", AvisynthInfo.sVersionNumber.c_str());
  sLogBuffer += sOutBuf;
  PrintConsole(true, COLOR_EMPHASIS, sOutBuf.c_str());

  sOutBuf = utils.StrFormat("File / Product version:     %s / %s\n", AvisynthInfo.sFileVersion.c_str(), AvisynthInfo.sProductVersion.c_str());
  sLogBuffer += sOutBuf;
  PrintConsole(true, COLOR_EMPHASIS, sOutBuf.c_str());

  sOutBuf = utils.StrFormat("Interface Version:          %d\n", AvisynthInfo.iInterfaceVersion);
  sLogBuffer += sOutBuf;
  PrintConsole(true, COLOR_EMPHASIS, sOutBuf.c_str());

  sOutBuf = utils.StrFormat("Multi-threading support:    %s\n", AvisynthInfo.bIsMTVersion ? ("Yes") : ("No"));
  sLogBuffer += sOutBuf;
  PrintConsole(true, COLOR_EMPHASIS, sOutBuf.c_str());

  sOutBuf = utils.StrFormat("%-28s%s\n", (std::string(platform::kAvsLibLabel) + " location:").c_str(), AvisynthInfo.sDLLPath.c_str());
  sLogBuffer += sOutBuf;
  PrintConsole(true, COLOR_EMPHASIS, sOutBuf.c_str());

  sOutBuf = utils.StrFormat("%-28s%s\n", (std::string(platform::kAvsLibLabel) + " time stamp:").c_str(), AvisynthInfo.sTimeStamp.c_str());
  sLogBuffer += sOutBuf;
  PrintConsole(true, COLOR_EMPHASIS, sOutBuf.c_str());

  if (AvisynthInfo.vPluginDirs.size() > 0)
  {
    std::string sPluginDir = "";
    for (unsigned int uiPlugDir = 0; uiPlugDir < AvisynthInfo.vPluginDirs.size(); uiPlugDir++)
    {
      sPluginDir = AvisynthInfo.vPluginDirs[uiPlugDir];
      if (sPluginDir != "")
      {
        sOutBuf = FormatPluginDirLine(sPluginDir) + "\n";
        sLogBuffer += sOutBuf;
        PrintConsole(true, COLOR_EMPHASIS, sOutBuf.c_str());
      }
    }
  }

  PrintConsole(true, COLOR_DEFAULT, "\n");
  sLogBuffer += "\n";

  //Plugin info
  if (AvisynthInfo.vPlugins.size() > 0)
  {
    unsigned int uiPlugin = 0;
    std::string sPluginDLL = "";
    std::string sPluginType = "";
    std::string sOldPluginType = "";
    std::string sPluginVersion = "";
    size_t spos = 0;

    for (uiPlugin = 0; uiPlugin < AvisynthInfo.vPlugins.size(); uiPlugin++)
    {
      sPluginDLL = AvisynthInfo.vPlugins[uiPlugin];
      spos = sPluginDLL.find("|");
      sPluginType = sPluginDLL.substr(0, spos);
      utils.StrTrim(sPluginType);
      BYTE HColor = COLOR_AVSM_VERSION;

      if (sPluginType != sOldPluginType)
      {
        if ((sPluginType.find("32 Bit") != std::string::npos) && (PROCESS_64))
          HColor = BG_BLACK | FG_HMAGENTA;
        else if ((sPluginType.find("64 Bit") != std::string::npos) && (!PROCESS_64))
          HColor = BG_BLACK | FG_HMAGENTA;
        else if (sPluginType.find(" Bit, ") != std::string::npos) //built for another CPU
          HColor = BG_BLACK | FG_HMAGENTA;
        else
          HColor = COLOR_AVSM_VERSION;

        sOutBuf = utils.StrFormat("\n[%s]", sPluginType.c_str());
        sLogBuffer += sOutBuf;
        PrintConsole(true, HColor, sOutBuf.c_str());

        if ((sPluginType.find("32 Bit") != std::string::npos) || (sPluginType.find("64 Bit") != std::string::npos))
          sOutBuf = "  [Version, Time stamp]\n";
        else
          sOutBuf = "  [Time stamp]\n";

        sLogBuffer += sOutBuf;
        PrintConsole(true, FG_HGREEN | BG_BLACK, sOutBuf.c_str());

        sOldPluginType = sPluginType;
      }

      sPluginDLL = sPluginDLL.substr(spos + 1);

      sOutBuf = utils.StrFormat("%s", sPluginDLL.c_str());
      sLogBuffer += sOutBuf;
      PrintConsole(true, COLOR_EMPHASIS, sOutBuf.c_str());

      sPluginVersion = platform::FileVersion(sPluginDLL);

      if ((sPluginType.find("32 Bit") != std::string::npos) || (sPluginType.find("64 Bit") != std::string::npos))
        sOutBuf = utils.StrFormat("  [%s, %s]\n", sPluginVersion.c_str(), utils.GetFileDateStamp(sPluginDLL).c_str());
      else
        sOutBuf = utils.StrFormat("  [%s]\n", utils.GetFileDateStamp(sPluginDLL).c_str());

      auto itShadowed = AvisynthInfo.mShadowedBy.find(sPluginDLL);
      if (itShadowed != AvisynthInfo.mShadowedBy.end())
      {
        sOutBuf.pop_back();
        PrintConsole(true, FG_HGREEN | BG_BLACK, sOutBuf.c_str());
        sLogBuffer += sOutBuf;
        sOutBuf = utils.StrFormat("  (shadowed by %s, not autoloaded)\n", itShadowed->second.c_str());
        PrintConsole(true, BG_BLACK | FG_HMAGENTA, sOutBuf.c_str());
      }
      else
        PrintConsole(true, FG_HGREEN | BG_BLACK, sOutBuf.c_str());

      sLogBuffer += sOutBuf;
    }
  }


  //Plugin errors
  if (AvisynthInfo.vPluginErrors.size() > 0)
  {
    unsigned int err = 0;
    std::string sPluginError = "";
    unsigned int errmaxlen = 0;
    unsigned int cpos = 0;

    for (err = 0; err < AvisynthInfo.vPluginErrors.size(); err++)
    {
      sPluginError = AvisynthInfo.vPluginErrors[err];

      if ((Settings.bDisableFFTWDLLWarning) && (sPluginError.find("FFTW") != std::string::npos))
        AvisynthInfo.vPluginErrors[err] = "";

      for (cpos = 0; cpos < sPluginError.length(); cpos++)
      {
        if (sPluginError[cpos] == '\n')
          break;
      }
      if (cpos > errmaxlen)
        errmaxlen = cpos;
    }

    bool bErrorsWarnings = false;
    for (err = 0; err < AvisynthInfo.vPluginErrors.size(); err++)
    {
      sPluginError = AvisynthInfo.vPluginErrors[err];
      if (sPluginError != "")
      {
        bErrorsWarnings = true;
        break;
      }
    }

    if (bErrorsWarnings)
    {
      std::string sLineSep(errmaxlen, '_');
      sOutBuf = "\n\n\n[Plugin errors/warnings]\n";
      sLogBuffer += sOutBuf;
      PrintConsole(true, COLOR_AVSM_VERSION, sOutBuf.c_str());
      sOutBuf = sLineSep + "\n";
      sLogBuffer += sOutBuf;
      PrintConsole(true, COLOR_ERROR, sOutBuf.c_str());

      for (err = 0; err < AvisynthInfo.vPluginErrors.size(); err++)
      {
        sPluginError = AvisynthInfo.vPluginErrors[err];
        sOutBuf = utils.StrFormat("\n%s\n", sPluginError.c_str());
        sOutBuf += sLineSep + "\n";
        sLogBuffer += sOutBuf;
        PrintConsole(true, COLOR_ERROR, sOutBuf.c_str());
      }
    }
  }

  std::string sFunction = "";
  if ((cmd.bLogFunctions) && (Settings.bCreateLog))
  {
    //Internal Functions
    if (AvisynthInfo.vInternalFunctions.size() > 0)
    {
      sFunction = "";
      sOutBuf = "\n\n\n[Internal (core) functions]\n";
      sLogBuffer += sOutBuf;

      for (unsigned int IFunc = 0; IFunc < AvisynthInfo.vInternalFunctions.size(); IFunc++)
      {
        sFunction = AvisynthInfo.vInternalFunctions[IFunc];
        sOutBuf = utils.StrFormat("    %s\n", sFunction.c_str());
        sLogBuffer += sOutBuf;
      }
    }

    //External (plugin) Functions
    if (AvisynthInfo.vPluginFunctions.size() > 0)
    {
      sFunction = "";
      sOutBuf = "\n\n[External (plugin) functions]\n";
      sLogBuffer += sOutBuf;

      for (unsigned int PFunc = 0; PFunc < AvisynthInfo.vPluginFunctions.size(); PFunc++)
      {
        sFunction = AvisynthInfo.vPluginFunctions[PFunc];
        sOutBuf = utils.StrFormat("    %s\n", sFunction.c_str());
        sLogBuffer += sOutBuf;
      }
    }

    //Script Functions
    if (AvisynthInfo.vScriptFunctions.size() > 0)
    {
      sFunction = "";
      sOutBuf = "\n\n[Script functions]\n";
      sLogBuffer += sOutBuf;

      for (unsigned int PFunc = 0; PFunc < AvisynthInfo.vScriptFunctions.size(); PFunc++)
      {
        sFunction = AvisynthInfo.vScriptFunctions[PFunc];
        sOutBuf = utils.StrFormat("    %s\n", sFunction.c_str());
        sLogBuffer += sOutBuf;
      }
    }
  }


  if (Settings.bCreateLog)
  {
    if (Settings.sSystemDateTime == "")
      Settings.sSystemDateTime = sys.GetFormattedSystemDateTime();

    std::string sAVSInfoFile = "";
    std::error_code ec;
    std::string sCurrentDir = platform::PathToUtf8(std::filesystem::current_path(ec));
    if (!ec)
    {
      if (Settings.sLogDirectory == "")
      {
        if (Settings.bLogFileDateTimeSuffix)
          sAVSInfoFile = utils.StrFormat("%s%cavsinfo_%s [%s].log", sCurrentDir.c_str(), platform::kPathSep, kArchName, Settings.sSystemDateTime.c_str());
        else
          sAVSInfoFile = utils.StrFormat("%s%cavsinfo_%s.log", sCurrentDir.c_str(), platform::kPathSep, kArchName);
      }
      else
      {
        if (Settings.bLogFileDateTimeSuffix)
          sAVSInfoFile = utils.StrFormat("%s%cavsinfo [%s].log", Settings.sLogDirectory.c_str(), platform::kPathSep, Settings.sSystemDateTime.c_str());
        else
          sAVSInfoFile = utils.StrFormat("%s%cavsinfo.log", Settings.sLogDirectory.c_str(), platform::kPathSep);
      }

      std::ofstream hAVSInfoFile(platform::PathFromUtf8(sAVSInfoFile));
      if (hAVSInfoFile.is_open())
     {
        hAVSInfoFile << sLogBuffer;
        hAVSInfoFile.flush();
        hAVSInfoFile.close();
      }
    }
  }

  PollKeys();
  return 0;
}


static void AppendScriptLogHeader(std::string &sLogBuffer, const std::string &sAVSMVersion, const stCmdLine &cmd, platform::GpuSensors &gpuinfo)
{
  std::string sOutBuf = "";

  sOutBuf = utils.StrFormat("Log file created with:      AVSMeter %s", sAVSMVersion.c_str());
  sLogBuffer += sOutBuf + "\n";

  sOutBuf = utils.StrFormat("Script file:                %s", cmd.sAVSFile.c_str());
  sLogBuffer += sOutBuf + "\n";

  if (cmd.sCommandLine != "")
  {
    sOutBuf = utils.StrFormat("Command line switches:      %s", cmd.sCommandLine.c_str());
    sLogBuffer += sOutBuf + "\n";
  }


  if (Settings.sLogDirectory != "")
  {
    sOutBuf = utils.StrFormat("Log file directory:         %s", Settings.sLogDirectory.c_str());
    sLogBuffer += sOutBuf + "\n";
  }


  AppendSystemInfo(sLogBuffer, gpuinfo);

  sLogBuffer += "\n\n[Avisynth info]";
  sOutBuf = utils.StrFormat("\nVersionString:              %s", AvisynthInfo.sVersionString.c_str());
  sLogBuffer += sOutBuf + "\n";

  sOutBuf = utils.StrFormat("VersionNumber:              %s", AvisynthInfo.sVersionNumber.c_str());
  sLogBuffer += sOutBuf + "\n";

  sOutBuf = utils.StrFormat("File / Product version:     %s / %s", AvisynthInfo.sFileVersion.c_str(), AvisynthInfo.sProductVersion.c_str());
  sLogBuffer += sOutBuf + "\n";

  sOutBuf = utils.StrFormat("Interface Version:          %d", AvisynthInfo.iInterfaceVersion);
  sLogBuffer += sOutBuf + "\n";

  sOutBuf = utils.StrFormat("Multi-threading support:    %s", AvisynthInfo.bIsMTVersion ? ("Yes") : ("No"));
  sLogBuffer += sOutBuf + "\n";

  sOutBuf = utils.StrFormat("%-28s%s", (std::string(platform::kAvsLibLabel) + " location:").c_str(), AvisynthInfo.sDLLPath.c_str());
  sLogBuffer += sOutBuf + "\n";

  sOutBuf = utils.StrFormat("%-28s%s", (std::string(platform::kAvsLibLabel) + " time stamp:").c_str(), AvisynthInfo.sTimeStamp.c_str());
  sLogBuffer += sOutBuf + "\n";

  for (unsigned int uiPlugDir = 0; uiPlugDir < AvisynthInfo.vPluginDirs.size(); uiPlugDir++)
  {
    std::string sTemp = AvisynthInfo.vPluginDirs[uiPlugDir];
    if (sTemp != "")
    {
      sLogBuffer += FormatPluginDirLine(sTemp) + "\n";
    }
  }
}


// "label:\tdir" -> value aligned at column 28 like the other info lines, at least one space
static std::string FormatPluginDirLine(const std::string &sPluginDir)
{
  size_t spos = sPluginDir.find(":\t");
  if (spos == std::string::npos)
    return sPluginDir;

  std::string sLabel = sPluginDir.substr(0, spos + 1);
  sLabel.resize(std::max<size_t>(sLabel.size() + 1, 28), ' ');
  return sLabel + sPluginDir.substr(spos + 2);
}


// Same names as AviSynth+ Info() (channel_layout_to_str, ffmpeg layout names)
static std::string ChannelLayoutString(unsigned int uiMask)
{
  static const struct { const char *name; unsigned int mask; } layouts[] = {
    { "mono", 0x4 },        { "stereo", 0x3 },       { "2.1", 0xB },           { "3.0", 0x7 },
    { "3.0(back)", 0x103 }, { "4.0", 0x107 },        { "quad", 0x33 },         { "quad(side)", 0x603 },
    { "3.1", 0xF },         { "5.0", 0x37 },         { "5.0(side)", 0x607 },   { "4.1", 0x10F },
    { "5.1", 0x3F },        { "5.1(side)", 0x60F },  { "6.0", 0x707 },         { "6.0(front)", 0x6C3 },
    { "hexagonal", 0x137 }, { "6.1", 0x70F },        { "6.1(back)", 0x13F },   { "6.1(front)", 0x6CB },
    { "7.0", 0x637 },       { "7.0(front)", 0x6C7 }, { "7.1", 0x63F },         { "7.1(wide)", 0xFF },
    { "7.1(wide-side)", 0x6CF }, { "7.1(top)", 0x503F }, { "octagonal", 0x737 }, { "cube", 0x2D033 }
  };
  static const char *speakers[] = {
    "FL", "FR", "FC", "LFE", "BL", "BR", "FLC", "FRC", "BC",
    "SL", "SR", "TC", "TFL", "TFC", "TFR", "TBL", "TBC", "TBR"
  };

  if (uiMask == 0x80000000)
    return "speaker_all";

  for (const auto &l : layouts)
    if (l.mask == uiMask)
      return l.name;

  std::string sNames;
  int iChannels = 0;
  for (unsigned int i = 0; i < 32; i++)
  {
    if (!(uiMask & (1u << i)))
      continue;
    if (iChannels++)
      sNames += "+";
    sNames += (i < sizeof(speakers) / sizeof(speakers[0])) ? speakers[i] : "NONE";
  }

  if (iChannels == 0)
    return "n/a";

  return utils.StrFormat("%d channels (%s)", iChannels, sNames.c_str());
}


static void ReportClipInfo(const VideoInfo &AVS_vidinfo, bool bAudioOnly, int64_t iMilliSeconds, bool bIsSETMTVersion, int iMTMode, std::string &sLogBuffer)
{
  std::string sOutBuf = "";

  PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", Pad("").c_str());

  sLogBuffer += "\n\n[Clip info]\n";

  if (!bAudioOnly)
    sOutBuf = utils.StrFormat("Number of frames:              %11u", AVS_vidinfo.num_frames);
  else
    sOutBuf = "Number of frames:                      n/a";

  PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
  sLogBuffer += sOutBuf + "\n";

  if (!bAudioOnly)
    sOutBuf = utils.StrFormat("Length (hh:mm:ss.ms):      %s", timer.FormatTimeString(iMilliSeconds, true).c_str());
  else
    sOutBuf = "Length (hh:mm:ss.ms):                  n/a";

  PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
  sLogBuffer += sOutBuf + "\n";

  if (!bAudioOnly)
    sOutBuf = utils.StrFormat("Frame width:                   %11u", AVS_vidinfo.width);
  else
    sOutBuf = "Frame width:                           n/a";

  PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
  sLogBuffer += sOutBuf + "\n";

  if (!bAudioOnly)
    sOutBuf = utils.StrFormat("Frame height:                  %11u", AVS_vidinfo.height);
  else
    sOutBuf = "Frame height:                          n/a";

  PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
  sLogBuffer += sOutBuf + "\n";

  if (!bAudioOnly)
  {
    if (AVS_vidinfo.IsFieldBased())
    {
      if (AVS_vidinfo.IsTFF())
        sOutBuf = utils.StrFormat("Framerate:                     %11.3f (%u/%u, TFF)", (double)AVS_vidinfo.fps_numerator / (double)AVS_vidinfo.fps_denominator, AVS_vidinfo.fps_numerator, AVS_vidinfo.fps_denominator);
      else if (AVS_vidinfo.IsBFF())
        sOutBuf = utils.StrFormat("Framerate:                     %11.3f (%u/%u, BFF)", (double)AVS_vidinfo.fps_numerator / (double)AVS_vidinfo.fps_denominator, AVS_vidinfo.fps_numerator, AVS_vidinfo.fps_denominator);
      else
        sOutBuf = utils.StrFormat("Framerate:                     %11.3f (%u/%u)", (double)AVS_vidinfo.fps_numerator / (double)AVS_vidinfo.fps_denominator, AVS_vidinfo.fps_numerator, AVS_vidinfo.fps_denominator);
    }
    else
      sOutBuf = utils.StrFormat("Framerate:                     %11.3f (%u/%u)", (double)AVS_vidinfo.fps_numerator / (double)AVS_vidinfo.fps_denominator, AVS_vidinfo.fps_numerator, AVS_vidinfo.fps_denominator);
  }
  else
    sOutBuf = "Framerate:                             n/a";

  PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
  sLogBuffer += sOutBuf + "\n";

  sOutBuf = "Colorspace:                     ";
  PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n%s", sOutBuf.c_str());
  sLogBuffer += sOutBuf;

  if (!AVS_vidinfo.HasVideo())
    sOutBuf = "       n/a";
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
      case VideoInfo::CS_RGBP8:       sOutBuf = "     RGBP8"; break;
      case VideoInfo::CS_RGBP10:      sOutBuf = "    RGBP10"; break;
      case VideoInfo::CS_RGBP12:      sOutBuf = "    RGBP12"; break;
      case VideoInfo::CS_RGBP14:      sOutBuf = "    RGBP14"; break;
      case VideoInfo::CS_RGBP16:      sOutBuf = "    RGBP16"; break;
      case VideoInfo::CS_RGBPS:       sOutBuf = "     RGBPS"; break;
      case VideoInfo::CS_RGBAP8:      sOutBuf = "    RGBAP8"; break;
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
      case VideoInfo::CS_RAW32:       sOutBuf = "     RAW32"; break;
      case VideoInfo::CS_YUV9:        sOutBuf = "      YUV9"; break;
      default:                        sOutBuf = "   Unknown";
    }
  }

  PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "%s\n", sOutBuf.c_str());
  sLogBuffer += sOutBuf + "\n";

  if (bIsSETMTVersion)
  {
    sOutBuf = utils.StrFormat("Active MT Mode:                          %d", iMTMode);
    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "%s\n", sOutBuf.c_str());
    sLogBuffer += sOutBuf + "\n";
  }

  sOutBuf = "Audio channels:        ";
  sLogBuffer += sOutBuf;
  if (AVS_vidinfo.HasAudio())
    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "%s", sOutBuf.c_str());

  if (AVS_vidinfo.nchannels)
    sOutBuf = utils.StrFormat("%19d", AVS_vidinfo.nchannels);
  else
    sOutBuf = "                n/a";

  sLogBuffer += sOutBuf + "\n";
  if (AVS_vidinfo.HasAudio())
    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "%s\n", sOutBuf.c_str());

  sOutBuf = "Audio channel layout:  ";
  sLogBuffer += sOutBuf;
  if (AVS_vidinfo.HasAudio())
    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "%s", sOutBuf.c_str());

  if (AVS_vidinfo.HasAudio() && AVS_vidinfo.IsChannelMaskKnown())
    sOutBuf = utils.StrFormat("%19s", ChannelLayoutString(AVS_vidinfo.GetChannelMask()).c_str());
  else
    sOutBuf = "                n/a";

  sLogBuffer += sOutBuf + "\n";
  if (AVS_vidinfo.HasAudio())
    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "%s\n", sOutBuf.c_str());

  sOutBuf = "Audio bits/sample:     ";
  sLogBuffer += sOutBuf;
  if (AVS_vidinfo.HasAudio())
    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "%s", sOutBuf.c_str());

  switch (AVS_vidinfo.sample_type)
  {
    case SAMPLE_INT8:  sOutBuf = "                  8";         break;
    case SAMPLE_INT16: sOutBuf = "                 16";         break;
    case SAMPLE_INT24: sOutBuf = "                 24";         break;
    case SAMPLE_INT32: sOutBuf = "                 32";         break;
    case SAMPLE_FLOAT: sOutBuf = "                 32 (Float)"; break;
    default:           sOutBuf = "                n/a";
  }

  sLogBuffer += sOutBuf + "\n";
  if (AVS_vidinfo.HasAudio())
    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "%s\n", sOutBuf.c_str());

  sOutBuf = "Audio sample rate:     ";
  sLogBuffer += sOutBuf;
  if (AVS_vidinfo.HasAudio())
    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "%s", sOutBuf.c_str());

  if (AVS_vidinfo.audio_samples_per_second)
    sOutBuf = utils.StrFormat("%19d", AVS_vidinfo.audio_samples_per_second);
  else
    sOutBuf = "                n/a";

  sLogBuffer += sOutBuf + "\n";
  if (AVS_vidinfo.HasAudio())
    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "%s\n", sOutBuf.c_str());

  sOutBuf = "Audio samples:         ";
  sLogBuffer += sOutBuf;
  if (AVS_vidinfo.HasAudio())
    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "%s", sOutBuf.c_str());

  if (AVS_vidinfo.num_audio_samples)
    sOutBuf = utils.StrFormat("%19lld", (long long)AVS_vidinfo.num_audio_samples);
  else
    sOutBuf = "                n/a";

  sLogBuffer += sOutBuf + "\n";
  if (AVS_vidinfo.HasAudio())
    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "%s\n", sOutBuf.c_str());
}


// First audio sample of frame n (0 <= n <= 2^31): floor(n * rate * den / num), like
// VideoInfo::AudioSamplesFromFrames, but split into quotient and remainder so that degenerate
// rates (huge fps_denominator) cannot overflow int64. rate * den < 2^63, n * remainder < 2^63,
// the remainder term is below n; saturates instead of wrapping.
static int64_t FirstSampleOfFrame(const VideoInfo &vi, int64_t n)
{
  if ((vi.fps_numerator == 0) || (vi.audio_samples_per_second <= 0) || (n <= 0))
    return 0;
  uint64_t uiRateDen = (uint64_t)vi.audio_samples_per_second * vi.fps_denominator;
  uint64_t uiQuot = uiRateDen / vi.fps_numerator;
  uint64_t uiRem = uiRateDen % vi.fps_numerator;
  if ((uint64_t)n > (uint64_t)INT64_MAX / (uiQuot + 1))
    return INT64_MAX;
  return (int64_t)((uint64_t)n * uiQuot + ((uint64_t)n * uiRem) / vi.fps_numerator);
}


static void InitBenchUnits(const VideoInfo &vi, IScriptEnvironment *AVS_env, const std::string &sAVSFile, stBenchUnits &bu)
{
  bu.vi = vi;
  bu.bGetFrame = (Settings.iBenchMode != BENCH_AUDIO);
  bu.bGetAudio = (Settings.iBenchMode != BENCH_VIDEO);
  bu.bFrameUnits = vi.HasVideo();
  bu.iChunk = Settings.iAudioChunk;

  if (bu.bGetFrame && !vi.HasVideo())
    AVS_env->ThrowError("Script did not return a video clip:\n%s%s", sAVSFile.c_str(), vi.HasAudio() ? "\nUse \"-audio\" for clips without video" : "");

  if (bu.bGetAudio && !vi.HasAudio())
    AVS_env->ThrowError("Script did not return an audio clip:\n%s", sAVSFile.c_str());

  if (bu.bFrameUnits)
    bu.uiUnits = (unsigned int)vi.num_frames;
  else
  {
    if (bu.iChunk == 0)
      bu.iChunk = DEFAULT_AUDIO_CHUNK;
    int64_t iUnits = (vi.num_audio_samples + bu.iChunk - 1) / bu.iChunk;
    if (iUnits > 0x7FFFFFFF)
      AVS_env->ThrowError("Too many audio chunks (%lld), use a larger \"-chunk\"", (long long)iUnits);
    bu.uiUnits = (unsigned int)iUnits;
  }

  if (bu.bGetAudio)
  {
    // The samples of one frame differ by one at most: floor((n+1)*r) - floor(n*r).
    // Very low frame rates would need huge buffers, so frame spans are split at MAX_AUDIO_CHUNK.
    int64_t iMaxCount = bu.iChunk;
    if (iMaxCount == 0)
    {
      int64_t iPerFrame = FirstSampleOfFrame(vi, 1);
      iMaxCount = (iPerFrame >= MAX_AUDIO_CHUNK) ? MAX_AUDIO_CHUNK : iPerFrame + 1;
    }
    bu.vBuffer.resize((size_t)(iMaxCount * vi.BytesPerAudioSample()));
  }
}


// Frame units request exactly the samples of frame n (like ffmpeg), so consecutive frames
// neither overlap nor leave gaps; -chunk splits them into smaller GetAudio calls.
static void ProcessUnit(const PClip &AVS_clip, IScriptEnvironment *AVS_env, unsigned int uiUnit, stBenchUnits &bu)
{
  if (bu.bGetFrame)
  {
    PVideoFrame src_frame = AVS_clip->GetFrame((int)uiUnit, AVS_env);
  }

  if (!bu.bGetAudio)
    return;

  int64_t iStart = 0;
  int64_t iEnd = 0;
  if (bu.bFrameUnits)
  {
    iStart = FirstSampleOfFrame(bu.vi, uiUnit);
    iEnd = FirstSampleOfFrame(bu.vi, (int64_t)uiUnit + 1);
  }
  else
  {
    iStart = (int64_t)uiUnit * bu.iChunk;
    iEnd = iStart + bu.iChunk;
  }

  if (iEnd > bu.vi.num_audio_samples)
    iEnd = bu.vi.num_audio_samples;

  if (iEnd <= iStart)
    return;

  int64_t iStep = bu.iChunk ? bu.iChunk : (std::min)(iEnd - iStart, (int64_t)MAX_AUDIO_CHUNK);
  for (int64_t iPos = iStart; iPos < iEnd; iPos += iStep)
  {
    int64_t iCount = (std::min)(iStep, iEnd - iPos);
    AVS_clip->GetAudio(bu.vBuffer.data(), iPos, iCount, AVS_env);
    bu.iSamplesRead += iCount;
  }

  if (bu.iFirstSample < 0)
    bu.iFirstSample = iStart;
  bu.iLastSample = iEnd - 1;
}


static std::string AudioSamplesText(const stBenchUnits &bu)
{
  if (bu.iSamplesRead == 0)
    return "0";
  return std::to_string((long long)bu.iSamplesRead) + " (" + std::to_string((long long)bu.iFirstSample) + " - " + std::to_string((long long)bu.iLastSample) + ")";
}


static double AudioRealtimeFactor(const stBenchUnits &bu, double dSeconds)
{
  if ((dSeconds <= 0.0) || (bu.vi.audio_samples_per_second <= 0))
    return 0.0;
  return (double)bu.iSamplesRead / (double)bu.vi.audio_samples_per_second / dSeconds;
}


static void RunBenchmark(const PClip &AVS_clip, IScriptEnvironment *AVS_env, platform::GpuSensors &gpuinfo, unsigned int uiFrameInterval, unsigned int uiLogFrameInterval, std::vector<stPerfData> &perfdata, bool &bEarlyExit, stRunStats &rs, stBenchUnits &bu)
{
  std::string sOutBuf = "";


  CProcessInfo processinfo;

  uint32_t dwMemCurrentMB = 0;

  double dCPUUsageCur = 0;
  double dCPUUsageAcc = 0;
  unsigned int uiGPUUsageCur = 0;
  unsigned int uiGPUUsageAcc = 0;
  unsigned int uiVPUUsageCur = 0;
  unsigned int uiVPUUsageAcc = 0;
  double dGPUPowerConsumptionCur = 0.0;
  double dGPUPowerConsumptionAcc = 0.0;

  unsigned int uiIntervalCounter = 0;

  unsigned int uiCurrentFrame = 0;
  double dFPSCurrent = 0.0;

  if (Settings.bGPUInfo)
  {
    gpuinfo.ReadSensors();
    if (gpuinfo.sensors.ReadError)
      AVS_env->ThrowError("Error reading GPU sensors\n");
  }

  processinfo.Update();

  double dStartTime = timer.GetTimer();
  double dCurrentTime = dStartTime;
  double dLastDisplayTime = dStartTime;
  double dLastIntervalTime = dStartTime;

  for (uiCurrentFrame = rs.uiFirstFrame; uiCurrentFrame <= rs.uiLastFrame; uiCurrentFrame++)
  {
    ProcessUnit(AVS_clip, AVS_env, uiCurrentFrame, bu);
    ++rs.uiFramesRead;

    if (((rs.uiFramesRead % uiFrameInterval) != 0) && (rs.uiFramesRead != rs.uiFramesToProcess))
      continue;

    dCurrentTime = timer.GetTimer();

    processinfo.Update();

    ++uiIntervalCounter;

    if ((Settings.bGPUInfo) && (uiIntervalCounter > 0))
    {
      gpuinfo.ReadSensors();
      if (gpuinfo.sensors.ReadError)
        AVS_env->ThrowError("Error reading GPU sensors\n");

      uiGPUUsageCur = (unsigned int)gpuinfo.sensors.GPULoad;
      uiGPUUsageAcc += uiGPUUsageCur;
      rs.uiGPUUsageAvg = (unsigned int)(((double)uiGPUUsageAcc / (double)uiIntervalCounter) + 0.5);
      uiVPUUsageCur = (unsigned int)gpuinfo.sensors.VPULoad;
      uiVPUUsageAcc += uiVPUUsageCur;
      rs.uiVPUUsageAvg = (unsigned int)(((double)uiVPUUsageAcc / (double)uiIntervalCounter) + 0.5);

      dGPUPowerConsumptionCur = gpuinfo.sensors.PowerConsumption;
      dGPUPowerConsumptionAcc += dGPUPowerConsumptionCur;
      rs.dGPUPowerConsumptionAvg = dGPUPowerConsumptionAcc / (double)uiIntervalCounter;
    }

    rs.dElapsedSec = dCurrentTime - dStartTime;
    rs.iElapsedMS = (int64_t)((rs.dElapsedSec * 1000.0) + 0.5);
    rs.iEstimatedMS = (int64_t)((double)rs.uiFramesToProcess * (double)rs.iElapsedMS / (double)rs.uiFramesRead);

    dCPUUsageCur = processinfo.dCPUUsage;
    dCPUUsageAcc += dCPUUsageCur;
    if (uiIntervalCounter > 0)
      rs.dCPUUsageAvg = dCPUUsageAcc / (double)uiIntervalCounter;

    dwMemCurrentMB = processinfo.dwMemMB;

    if (dwMemCurrentMB > rs.dwMemPeakMB)
      rs.dwMemPeakMB = dwMemCurrentMB;

    rs.dFPSAverage = (double)rs.uiFramesRead / (dCurrentTime - dStartTime);

    if ((rs.uiFramesRead % uiFrameInterval) != 0)
      continue;

    if ((dCurrentTime - dLastIntervalTime) > 0.000001)
      dFPSCurrent = (double)uiFrameInterval / (dCurrentTime - dLastIntervalTime);
    else
      dFPSCurrent = (double)uiFrameInterval / 0.000001;

    if (dFPSCurrent > rs.dFPSMax)
      rs.dFPSMax = dFPSCurrent;
    if (dFPSCurrent < rs.dFPSMin)
      rs.dFPSMin = dFPSCurrent;


    if (((rs.uiFramesRead % uiLogFrameInterval) == 0) || (rs.uiFramesRead == rs.uiFramesToProcess))
    {
      stPerfData pdata;
      pdata.frame = uiCurrentFrame;
      pdata.fps_current = (float)dFPSCurrent;
      pdata.fps_average = (float)rs.dFPSAverage;
      pdata.cpu_usage = (float)processinfo.dCPUUsage;

      if (Settings.bGPUInfo)
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
      pdata.process_memory = dwMemCurrentMB;
      perfdata.push_back(pdata);
    }

    dLastIntervalTime = dCurrentTime;

    if ((dCurrentTime - dLastDisplayTime) < REFRESH_INTERVAL)
      continue;

    dLastDisplayTime = dCurrentTime;

    if (!rs.bFirstScr)
    {
      platform::ConsoleCursorUp(rs.uiCursorOffset);
      rs.uiCursorOffset = 0;
    }

    rs.bFirstScr = false;
    bEarlyExit = false;

    sOutBuf = utils.StrFormat("%s (current | last):             %u | %u", bu.UnitName(), uiCurrentFrame + 1, rs.uiLastFrame);
    PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
    ++rs.uiCursorOffset;

    if (Settings.bDisplayFPS)
    {
      sOutBuf = utils.StrFormat("%s (cur | min | max | avg):        %s | %s | %s | %s", bu.RateName(), utils.StrFormatFPS(dFPSCurrent).c_str(), utils.StrFormatFPS(rs.dFPSMin).c_str(), utils.StrFormatFPS(rs.dFPSMax).c_str(), utils.StrFormatFPS(rs.dFPSAverage).c_str());
      PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
      ++rs.uiCursorOffset;
    }

    if (Settings.bDisplayTPF)
    {
      sOutBuf = utils.StrFormat("%s (cur | max | min | avg):        %s | %s | %s | %s ms", bu.TimeName(), utils.StrFormatTPF(1000.0 / dFPSCurrent).c_str(), utils.StrFormatTPF(1000.0 / rs.dFPSMin).c_str(), utils.StrFormatTPF(1000.0 / rs.dFPSMax).c_str(), utils.StrFormatTPF(1000.0 / rs.dFPSAverage).c_str());
      PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
      ++rs.uiCursorOffset;
    }

    if (bu.bGetAudio)
    {
      sOutBuf = utils.StrFormat("Audio (samples | realtime):         %lld | %.2fx", (long long)bu.iSamplesRead, AudioRealtimeFactor(bu, dCurrentTime - dStartTime));
      PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
      ++rs.uiCursorOffset;
    }

    sOutBuf = utils.StrFormat("Process memory usage:               %u MiB", dwMemCurrentMB);
    PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
    ++rs.uiCursorOffset;

    sOutBuf = utils.StrFormat("Thread count:                       %u", processinfo.wThreadCount);
    PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
    ++rs.uiCursorOffset;

    sOutBuf = utils.StrFormat("CPU usage (current | average):      %.1f%% | %.1f%%", dCPUUsageCur, rs.dCPUUsageAvg);
    PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
    ++rs.uiCursorOffset;

    if (Settings.bGPUInfo)
    {
      PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n");
      ++rs.uiCursorOffset;

      sOutBuf = utils.StrFormat("GPU usage (current | average):      %u%% | %u%%", uiGPUUsageCur, rs.uiGPUUsageAvg);
      PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
      ++rs.uiCursorOffset;

      if (gpuinfo.data.NVVPU)
      {
        sOutBuf = utils.StrFormat("VPU usage (current | average):      %u%% | %u%%", uiVPUUsageCur, rs.uiVPUUsageAvg);
        PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
        ++rs.uiCursorOffset;
      }

      if (gpuinfo.data.GeneralMem)
      {
        sOutBuf = utils.StrFormat("GPU memory usage:                   %u MiB", gpuinfo.sensors.MemoryUsedGeneral);
        PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
        ++rs.uiCursorOffset;
      }

      if (gpuinfo.data.DedicatedMem)
      {
        sOutBuf = utils.StrFormat("GPU memory usage (Dedicated):       %u MiB", gpuinfo.sensors.MemoryUsedDedicated);
        PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
        ++rs.uiCursorOffset;
      }

      if (gpuinfo.data.DynamicMem)
      {
        sOutBuf = utils.StrFormat("GPU memory usage (Dynamic):         %u MiB", gpuinfo.sensors.MemoryUsedDynamic);
        PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
        ++rs.uiCursorOffset;
      }

      if (gpuinfo.data.Power)
        sOutBuf = utils.StrFormat("GPU Power Consumption (cur | avg):  %.1f W | %.1f W", dGPUPowerConsumptionCur, rs.dGPUPowerConsumptionAvg);
      else
        sOutBuf = "GPU Power Consumption (cur | avg):  n/a (no sensor data)";
      PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
      ++rs.uiCursorOffset;
    }

    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n");
    ++rs.uiCursorOffset;

    sOutBuf = utils.StrFormat("Time (elapsed | estimated):         %s | %s", timer.FormatTimeString(rs.iElapsedMS, false).c_str(), timer.FormatTimeString(rs.iEstimatedMS, false).c_str());
    PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
    ++rs.uiCursorOffset;

    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\nPress \'Esc\' to cancel the process...\n\n");
    ++rs.uiCursorOffset;
    ++rs.uiCursorOffset;
    ++rs.uiCursorOffset;

    if (Settings.iTimeLimit != -1)
    {
      if (rs.iElapsedMS >= (Settings.iTimeLimit * 1000))
      {
        rs.uiLastFrame = uiCurrentFrame;
        break;
      }
    }

    if (platform::KeyPressed())
    {
      if (platform::ReadKey() == 0x1B) //ESC
      {
        rs.uiLastFrame = uiCurrentFrame;
        break;
      }
    }
  }

  rs.wThreadCount = processinfo.wThreadCount;

  if (Settings.bGPUInfo)
    gpuinfo.Release();
}


static void ReportRuntime(const stRunStats &rs, const stBenchUnits &bu, platform::GpuSensors &gpuinfo, unsigned int uiFrameInterval, std::string &sLogBuffer, bool &bRuntimeTooShort)
{
  std::string sOutBuf = "";

  sLogBuffer += "\n\n[Runtime info]\n";

  if (rs.iElapsedMS >= MIN_RUNTIME)
  {
    if (!rs.bFirstScr)
      platform::ConsoleCursorUp(rs.uiCursorOffset);

    if (rs.uiFirstFrame == rs.uiLastFrame)
    {
      if (rs.uiFirstFrame == 0)
        sOutBuf = utils.StrFormat("%ss processed:                   %u", bu.UnitName(), rs.uiFramesRead);
      else
        sOutBuf = utils.StrFormat("%ss processed:                   %u (%u)", bu.UnitName(), rs.uiFramesRead, rs.uiFirstFrame);
    }
    else
      sOutBuf = utils.StrFormat("%ss processed:                   %u (%u - %u)", bu.UnitName(), rs.uiFramesRead, rs.uiFirstFrame, rs.uiLastFrame);

    PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
    sLogBuffer += sOutBuf + "\n";

    if (bu.bGetAudio)
    {
      sOutBuf = "Audio samples processed:            " + AudioSamplesText(bu);
      PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
      sLogBuffer += sOutBuf + "\n";
    }

    if (rs.uiFramesRead >= uiFrameInterval)
    {
      if (Settings.bDisplayFPS)
      {
        sOutBuf = utils.StrFormat("%s (min | max | average):          %s | %s | %s", bu.RateName(), utils.StrFormatFPS(rs.dFPSMin).c_str(), utils.StrFormatFPS(rs.dFPSMax).c_str(), utils.StrFormatFPS(rs.dFPSAverage).c_str());
        PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
        sLogBuffer += sOutBuf + "\n";
      }

      if (Settings.bDisplayTPF)
      {
        sOutBuf = utils.StrFormat("%s (max | min | average):          %s | %s | %s ms", bu.TimeName(), utils.StrFormatTPF(1000.0 / rs.dFPSMin).c_str(), utils.StrFormatTPF(1000.0 / rs.dFPSMax).c_str(), utils.StrFormatTPF(1000.0 / rs.dFPSAverage).c_str());
        PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
        sLogBuffer += sOutBuf + "\n";
      }

      if (bu.bGetAudio)
      {
        sOutBuf = utils.StrFormat("Audio speed (realtime):             %.2fx", AudioRealtimeFactor(bu, (double)rs.iElapsedMS / 1000.0));
        PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
        sLogBuffer += sOutBuf + "\n";
      }

      sOutBuf = utils.StrFormat("Process memory usage (max):         %u MiB", rs.dwMemPeakMB);
      PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
      sLogBuffer += sOutBuf + "\n";

      sOutBuf = utils.StrFormat("Thread count:                       %u", rs.wThreadCount);
      PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
      sLogBuffer += sOutBuf + "\n";

      sOutBuf = utils.StrFormat("CPU usage (average):                %.1f%%", rs.dCPUUsageAvg);
      PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
      sLogBuffer += sOutBuf + "\n";

      if (Settings.bGPUInfo)
      {
        PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n");
        sLogBuffer += "\n";

        sOutBuf = utils.StrFormat("GPU usage (average):                %u%%", rs.uiGPUUsageAvg);
        PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
        sLogBuffer += sOutBuf + "\n";

        if (gpuinfo.data.NVVPU)
        {
          sOutBuf = utils.StrFormat("VPU usage (average):                %u%%", rs.uiVPUUsageAvg);
          PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
          sLogBuffer += sOutBuf + "\n";
        }

        if (gpuinfo.data.GeneralMem)
        {
          sOutBuf = utils.StrFormat("GPU memory usage:                   %u MiB", gpuinfo.sensors.MemoryUsedGeneral);
          PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
          sLogBuffer += sOutBuf + "\n";
        }

        if (gpuinfo.data.DedicatedMem)
        {
          sOutBuf = utils.StrFormat("GPU memory usage (Dedicated):       %u MiB", gpuinfo.sensors.MemoryUsedDedicated);
          PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
          sLogBuffer += sOutBuf + "\n";
        }

        if (gpuinfo.data.DynamicMem)
        {
          sOutBuf = utils.StrFormat("GPU memory usage (Dynamic):         %u MiB", gpuinfo.sensors.MemoryUsedDynamic);
          PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
          sLogBuffer += sOutBuf + "\n";
        }

        if (gpuinfo.data.Power)
          sOutBuf = utils.StrFormat("GPU Power Consumption (average):    %.1f W", rs.dGPUPowerConsumptionAvg);
        else
          sOutBuf = "GPU Power Consumption (average):    n/a (no sensor data)";
        PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
        sLogBuffer += sOutBuf + "\n";
      }

      PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\n");
      sLogBuffer += "\n";
    }

    if ((Settings.bLogEstimatedTime) && (rs.uiFramesRead < rs.uiFramesToProcess))
      sOutBuf = utils.StrFormat("Time (elapsed | estimated):         %s | %s", timer.FormatTimeString(rs.iElapsedMS, false).c_str(), timer.FormatTimeString(rs.iEstimatedMS, false).c_str());
    else
      sOutBuf = utils.StrFormat("Time (elapsed):                     %s", timer.FormatTimeString(rs.iElapsedMS, false).c_str());

    PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
    sLogBuffer += sOutBuf + "\n";

    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", Pad("").c_str());
    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\n", Pad("").c_str());
    platform::ConsoleCursorUp(2);
  }
  else
  {
    if (!rs.bFirstScr)
    {
      for (unsigned int u = 0; u < rs.uiCursorOffset; u++)
      {
        PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", Pad("").c_str());
        platform::ConsoleCursorUp(1);
      }
    }

    sOutBuf = "Script runtime is too short for meaningful measurements\n";
    if (bu.bGetAudio)
      sOutBuf += "Use a longer clip (e.g. Loop) or a smaller \"-chunk\"\n";
    sOutBuf += "\n";
    PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\r%s", Pad(sOutBuf).c_str());
    sLogBuffer += sOutBuf;

    if (rs.uiFirstFrame == rs.uiLastFrame)
    {
      if (rs.uiFirstFrame == 0)
        sOutBuf = utils.StrFormat("%ss processed:               %u", bu.UnitName(), rs.uiFramesRead);
      else
        sOutBuf = utils.StrFormat("%ss processed:               %u (%u)", bu.UnitName(), rs.uiFramesRead, rs.uiFirstFrame);
    }
    else
      sOutBuf = utils.StrFormat("%ss processed:               %u (%u - %u)", bu.UnitName(), rs.uiFramesRead, rs.uiFirstFrame, rs.uiLastFrame);

    PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
    sLogBuffer += sOutBuf + "\n";

    if (bu.bGetAudio)
    {
      sOutBuf = "Audio samples processed:        " + AudioSamplesText(bu);
      PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
      sLogBuffer += sOutBuf + "\n";

      if ((bu.iSamplesRead > 0) && (rs.dElapsedSec > 0.0))
      {
        sOutBuf = utils.StrFormat("Audio speed (realtime):         %.2fx (imprecise)", AudioRealtimeFactor(bu, rs.dElapsedSec));
        PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
        sLogBuffer += sOutBuf + "\n";
      }
    }

    if ((Settings.bLogEstimatedTime) && (rs.uiFramesRead < rs.uiFramesToProcess))
      sOutBuf = utils.StrFormat("Time (elapsed | estimated):     %s | %s", timer.FormatTimeString(rs.iElapsedMS, false).c_str(), timer.FormatTimeString(rs.iEstimatedMS, false).c_str());
    else
      sOutBuf = utils.StrFormat("Time (elapsed):                 %s", timer.FormatTimeString(rs.iElapsedMS, false).c_str());

    PrintConsole(Settings.bConUseStdOut, COLOR_EMPHASIS, "\r%s\n", Pad(sOutBuf).c_str());
    sLogBuffer += sOutBuf + "\n";

    bRuntimeTooShort = true;
  }
}


// Returns a fatal error. An INI file that cannot be read or created is only a warning:
// the defaults are used.
std::string ParseINIFile(std::string &s_warning)
{
  std::string sRet = "";
  std::string sTemp = "";
  s_warning = "";

  std::string sINIFile = platform::IniFilePath();
  if (sINIFile == "")
  {
    s_warning = "\nWarning: No location for the INI file, using defaults\n";
    return "";
  }

  std::string sCurrentLine = "";

  if (!platform::FileExists(sINIFile)) //No ini file present, create the file with defaults
  {
    if (!WriteINIFile(sINIFile))
      s_warning = utils.StrFormat("\nWarning: Cannot create \"%s\", using defaults\n", sINIFile.c_str());

    return "";
  }

  std::ifstream hINIFile(platform::PathFromUtf8(sINIFile));
  if (!hINIFile.is_open())
  {
    s_warning = utils.StrFormat("\nWarning: Cannot open \"%s\", using defaults\n", sINIFile.c_str());
    return "";
  }

  std::string sOrgLine = "";
  sCurrentLine = "";
  size_t spos = 0;
  int iBoolValue = 0;

  while (std::getline(hINIFile, sCurrentLine))
  {
    sCurrentLine = platform::ToUtf8(sCurrentLine);
    utils.StrTrim(sCurrentLine);
    sOrgLine = sCurrentLine;
    utils.StrToLC(sCurrentLine);
    utils.StrTrim(sCurrentLine);

    if ((sCurrentLine.substr(0, 1) == "#") || (sCurrentLine == ""))
      continue;

    if (sCurrentLine.substr(0, 12) == "logdirectory")
    {
      Settings.sLogDirectory = sOrgLine.substr(13);
      Settings.sLogDirectory.erase(Settings.sLogDirectory.find_last_not_of(platform::kPathSeparators) + 1); //remove trailing separators
      utils.StrTrim(Settings.sLogDirectory);
      if ((!platform::DirectoryExists(Settings.sLogDirectory)) && (Settings.sLogDirectory != ""))
      {
        sRet = utils.StrFormat("\nError: Log directory specified in \"%s\" does not exist:\n\"%s\"\n", sINIFile.c_str(), Settings.sLogDirectory.c_str());
        return sRet;
      }
      continue;
    }
    if (sCurrentLine.substr(0, 6) == "avsdll")
    {
      Settings.sAVSDLL = sOrgLine.substr(7);
      utils.StrTrim(Settings.sAVSDLL);
      if ((!platform::FileExists(Settings.sAVSDLL)) && (Settings.sAVSDLL != ""))
      {
        sRet = utils.StrFormat("\nThe file specified in \"%s\" does not exist:\nAVSDLL=%s\n", sINIFile.c_str(), Settings.sAVSDLL.c_str());
        return sRet;
      }
      continue;
    }

    if (sCurrentLine.substr(0, 18) == "loadplugininterval")
    {
      sTemp = sCurrentLine.substr(19);
      if (utils.IsNumeric(sTemp))
      {
        Settings.nLoadPluginInterval = (BYTE)atol(sTemp.c_str());
        if ((Settings.nLoadPluginInterval < 1) || (Settings.nLoadPluginInterval > 49))
        {
          sRet = utils.StrFormat("\nError: INI setting is invalid:\n\"%s\"\nThe value must be between \'1\' and \'49\'\n", sOrgLine.c_str());
          return sRet;
        }
      }
      else
      {
        sRet = utils.StrFormat("\nINI setting is invalid:\n\"%s\"\nThe value must be between \'1\' and \'49\'\n", sOrgLine.c_str());
        return sRet;
      }
      continue;
    }


    iBoolValue = -1;
    if (sCurrentLine.length() > 3)
    {
      if (sCurrentLine.substr(sCurrentLine.length() - 2) == "=0") iBoolValue = 0;
      if (sCurrentLine.substr(sCurrentLine.length() - 2) == "=1") iBoolValue = 1;
    }

    if (iBoolValue != -1)
    {
      if (sCurrentLine.substr(0, 9) == "createlog")
        Settings.bCreateLog = (iBoolValue == 0) ? false : true;

      if (sCurrentLine.substr(0, 9) == "createcsv")
        Settings.bCreateCSV = (iBoolValue == 0) ? false : true;

      if (sCurrentLine.substr(0, 10) == "displayfps")
        Settings.bDisplayFPS = (iBoolValue == 0) ? false : true;

      if (sCurrentLine.substr(0, 10) == "displaytpf")
        Settings.bDisplayTPF = (iBoolValue == 0) ? false : true;

      if (sCurrentLine.substr(0, 21) == "logfiledatetimesuffix")
        Settings.bLogFileDateTimeSuffix = (iBoolValue == 0) ? false : true;

      if (sCurrentLine.substr(0, 15) == "pausebeforeexit")
        Settings.bPauseBeforeExit = (iBoolValue == 0) ? false : true;

      if (sCurrentLine.substr(0, 17) == "invokedistributor")
        Settings.bInvokeDistributor = (iBoolValue == 0) ? false : true;

      if (sCurrentLine.substr(0, 20) == "allowonlyoneinstance")
        Settings.bAllowOnlyOneInstance = (iBoolValue == 0) ? false : true;

      if (sCurrentLine.substr(0, 8) == "usecolor")
        Settings.bUseColor = (iBoolValue == 0) ? false : true;

      if (sCurrentLine.substr(0, 12) == "conusestdout")
        Settings.bConUseStdOut = (iBoolValue == 0) ? false : true;

      if (sCurrentLine.substr(0, 16) == "logestimatedtime")
        Settings.bLogEstimatedTime = (iBoolValue == 0) ? false : true;

      if (sCurrentLine.substr(0, 21) == "autocompleteextension")
        Settings.bAutoCompleteExtension = (iBoolValue == 0) ? false : true;

      if (sCurrentLine.substr(0, 21) == "disablefftwdllwarning")
        Settings.bDisableFFTWDLLWarning = (iBoolValue == 0) ? false : true;
    }
  }

  hINIFile.close();

  //Rewrite to add settings missing from the file; the settings were read, so a read-only file is fine
  WriteINIFile(sINIFile);

  return "";
}


bool WriteINIFile(std::string &s_inifile)
{
  std::string sSettings = "";
  std::string sComment = "";

  std::ofstream hINIFile(platform::PathFromUtf8(s_inifile));
  if (!hINIFile.is_open())
    return false;

  sComment =  "# This INI file can be used to set defaults for AVSMeter and/or configure\n";
  sComment += "# settings that are not implemented as command line switches.\n";
  sComment += "# See \"AVSMeter.html\" for documentation of the various settings\n";
  sComment += "# Note: If applicable, command line switches override INI file settings.\n\n";
  hINIFile << sComment;

  sSettings = utils.StrFormat("DisplayFPS=%u\n", Settings.bDisplayFPS);
  sSettings += utils.StrFormat("DisplayTPF=%u\n\n", Settings.bDisplayTPF);

  sSettings += utils.StrFormat("CreateLog=%u\n", Settings.bCreateLog);
  sSettings += utils.StrFormat("CreateCSV=%u\n", Settings.bCreateCSV);
  sSettings += utils.StrFormat("LogDirectory=%s\n", Settings.sLogDirectory.c_str());
  sSettings += utils.StrFormat("LogFileDateTimeSuffix=%u\n", Settings.bLogFileDateTimeSuffix);
  sSettings += utils.StrFormat("AVSDLL=%s\n\n", Settings.sAVSDLL.c_str());

  sSettings += utils.StrFormat("AllowOnlyOneInstance=%u\n", Settings.bAllowOnlyOneInstance);
  sSettings += utils.StrFormat("ConUseSTDOUT=%u\n", Settings.bConUseStdOut);
  sSettings += utils.StrFormat("UseColor=%u\n", Settings.bUseColor);
  sSettings += utils.StrFormat("PauseBeforeExit=%u\n\n", Settings.bPauseBeforeExit);

  sSettings += utils.StrFormat("DisableFFTWDLLWarning=%u\n", Settings.bDisableFFTWDLLWarning);
  sSettings += utils.StrFormat("InvokeDistributor=%u\n", Settings.bInvokeDistributor);
  sSettings += utils.StrFormat("LogEstimatedTime=%u\n", Settings.bLogEstimatedTime);
  sSettings += utils.StrFormat("AutoCompleteExtension=%u\n", Settings.bAutoCompleteExtension);
  sSettings += utils.StrFormat("LoadPluginInterval=%u\n", Settings.nLoadPluginInterval);

  hINIFile << sSettings;

  hINIFile.flush();
  hINIFile.close();

  return true;
}


// Audio-only clips are measured in chunks; the column names keep their width.
static std::string PerfHeader(std::string s, bool bChunkUnits)
{
  if (!bChunkUnits)
    return s;
  for (size_t nPos = 0; (nPos = s.find("rame", nPos)) != std::string::npos; nPos += 4)
  {
    if ((nPos > 0) && ((s[nPos - 1] == 'F') || (s[nPos - 1] == 'f')))
    {
      s.replace(nPos - 1, 5, (s[nPos - 1] == 'F') ? "Chunk" : "chunk");
    }
  }
  return s;
}


std::string CreateLogFile(std::string &s_avsfile, std::string &s_logbuffer, std::string &s_gpuinfo, std::vector<stPerfData> &cs_pdata, std::string &s_avserror, bool bNVVP, bool bOmitstPerfData, bool bChunkUnits)
{
  std::string sRet = "";
  std::string sAVSBuffer = "";
  std::string sCurrentLine = "";

  std::ifstream hAVSFile(platform::PathFromUtf8(s_avsfile));
  if (!hAVSFile.is_open())
  {
    sRet = utils.StrFormat("\nCannot open \"%s\"\n", s_avsfile.c_str());
    return sRet;
  }

  while (std::getline(hAVSFile, sCurrentLine))
    sAVSBuffer += sCurrentLine + "\n";

  hAVSFile.close();
  sAVSBuffer = platform::ToUtf8(sAVSBuffer);
  utils.StrTrim(sAVSBuffer);

  std::string sLogFile = "";
  size_t ilen = s_avsfile.length();
  std::ofstream hLogFile;


  Settings.sSystemDateTime = sys.GetFormattedSystemDateTime();
  if (ilen > 4)
  {
    if (Settings.bLogFileDateTimeSuffix)
      sLogFile = s_avsfile.substr(0, ilen - 4) + " [" + Settings.sSystemDateTime + "].log";
    else
      sLogFile = s_avsfile.substr(0, ilen - 4) + ".log";
  }
  else
  {
    if (Settings.bLogFileDateTimeSuffix)
      sLogFile = s_avsfile + " [" + Settings.sSystemDateTime + "].log";
    else
      sLogFile = s_avsfile + ".log";
  }

  if (Settings.sLogDirectory != "")
  {
    size_t nPos = sLogFile.find_last_of(platform::kPathSeparators);
    if (nPos != std::string::npos)
      sLogFile = Settings.sLogDirectory + platform::kPathSep + sLogFile.substr(nPos + 1);
  }

  hLogFile.open(platform::PathFromUtf8(sLogFile));
  if (!hLogFile.is_open())
  {
    sRet = utils.StrFormat("\nCannot create \"%s\"\n", sLogFile.c_str());
    return sRet;
  }


  std::string sLog = "";

  sLog = s_logbuffer + "\n";
  hLogFile << sLog;
  sLog = "\n[Script]\n" + sAVSBuffer + "\n\n";
  hLogFile << sLog;

  if (s_avserror != "")
    hLogFile << "\n[Errors]\n" + platform::ToUtf8(s_avserror) + "\n\n";

  if (Settings.bGPUInfo)
    hLogFile << s_gpuinfo;

  if ((cs_pdata.size() > 0) && !bOmitstPerfData)
  {
    if (Settings.bGPUInfo)
    {
      if (bNVVP)
        sLog = "\n[Performance data]\n       Frame    Frames/sec   Time/frame(ms)   CPU(%)   GPU(%)   VPU(%)   Threads   Memory(MiB)\n";
      else
        sLog = "\n[Performance data]\n       Frame    Frames/sec   Time/frame(ms)   CPU(%)   GPU(%)   Threads   Memory(MiB)\n";
    }
    else
      sLog = "\n[Performance data]\n       Frame    Frames/sec   Time/frame(ms)   CPU(%)   Threads   Memory(MiB)\n";

    sLog = PerfHeader(sLog, bChunkUnits);

    hLogFile << sLog;

    std::string stemp1 = "";
    std::string stemp2 = "";
    unsigned int uiFrame = 0;
    for (unsigned int i = 0; i < cs_pdata.size(); i++)
    {
      uiFrame = cs_pdata[i].frame + 1;
      stemp1 = utils.StrFormat("%u", uiFrame);
      std::string spad((12 - stemp1.length()), ' ');
      if (Settings.bGPUInfo)
      {
        if (bNVVP)
          stemp2 = utils.StrFormat("%s%u %13.3f %16.6f %8.1f %8u %8u %9u %13u", spad.c_str(), uiFrame, cs_pdata[i].fps_current, 1000.0 / cs_pdata[i].fps_current, cs_pdata[i].cpu_usage, cs_pdata[i].gpu_usage, cs_pdata[i].vpu_usage, cs_pdata[i].num_threads, cs_pdata[i].process_memory);
        else
          stemp2 = utils.StrFormat("%s%u %13.3f %16.6f %8.1f %8u %9u %13u", spad.c_str(), uiFrame, cs_pdata[i].fps_current, 1000.0 / cs_pdata[i].fps_current, cs_pdata[i].cpu_usage, cs_pdata[i].gpu_usage, cs_pdata[i].num_threads, cs_pdata[i].process_memory);
      }
      else
        stemp2 = utils.StrFormat("%s%u %13.3f %16.6f %8.1f %9u %13u", spad.c_str(), uiFrame, cs_pdata[i].fps_current, 1000.0 / cs_pdata[i].fps_current, cs_pdata[i].cpu_usage, cs_pdata[i].num_threads, cs_pdata[i].process_memory);

      hLogFile << stemp2 + "\n";
    }
  }

  hLogFile.flush();
  hLogFile.close();

  return sRet;
}


std::string CreateCSVFile(std::string &s_avsfile, std::vector<stPerfData> &cs_pdata, bool bNVVP, bool bChunkUnits)
{
  std::string sRet = "";

  std::string sCSVFile = "";
  size_t ilen = s_avsfile.length();
  std::ofstream hCSVFile;

  Settings.sSystemDateTime = sys.GetFormattedSystemDateTime();
  if (ilen > 4)
  {
    if (Settings.bLogFileDateTimeSuffix)
      sCSVFile = s_avsfile.substr(0, ilen - 4) + " [" + Settings.sSystemDateTime + "].csv";
    else
      sCSVFile = s_avsfile.substr(0, ilen - 4) + ".csv";
  }
  else
  {
    if (Settings.bLogFileDateTimeSuffix)
      sCSVFile = s_avsfile + " [" + Settings.sSystemDateTime + "].csv";
    else
      sCSVFile = s_avsfile + ".csv";
  }

  if (Settings.sLogDirectory != "")
  {
    size_t nPos = sCSVFile.find_last_of(platform::kPathSeparators);
    if (nPos != std::string::npos)
      sCSVFile = Settings.sLogDirectory + platform::kPathSep + sCSVFile.substr(nPos + 1);
  }

  hCSVFile.open(platform::PathFromUtf8(sCSVFile));
  if (!hCSVFile.is_open())
  {
    sRet = utils.StrFormat("\nCannot create \"%s\"\n", sCSVFile.c_str());
    return sRet;
  }


  std::string sCSV = "";

  if (cs_pdata.size() > 1)
  {
    if (Settings.bGPUInfo)
    {
      if (bNVVP)
        sCSV = "Frame,Frames/sec,Frames/sec(average),Time/frame(ms),Time/frame(average)(ms),CPU(%),GPU(%),VPU(%),Threads,Memory(MiB)\n";
      else
        sCSV = "Frame,Frames/sec,Frames/sec(average),Time/frame(ms),Time/frame(average)(ms),CPU(%),GPU(%),Threads,Memory(MiB)\n";
    }
    else
      sCSV = "Frame,Frames/sec,Frames/sec(average),Time/frame(ms),Time/frame(average)(ms),CPU(%),Threads,Memory(MiB)\n";

    sCSV = PerfHeader(sCSV, bChunkUnits);
    hCSVFile << sCSV;

    std::string stemp = "";
    unsigned int uiFrame = 0;
    for (unsigned int i = 0; i < cs_pdata.size(); i++)
    {
      uiFrame = cs_pdata[i].frame + 1;
      if (Settings.bGPUInfo)
      {
        if (bNVVP)
          stemp = utils.StrFormat("%u,%.3f,%.3f,%.6f,%.6f,%.1f,%u,%u,%u,%u", uiFrame, cs_pdata[i].fps_current, cs_pdata[i].fps_average, 1000.0 / cs_pdata[i].fps_current, 1000.0 / cs_pdata[i].fps_average, cs_pdata[i].cpu_usage, cs_pdata[i].gpu_usage, cs_pdata[i].vpu_usage, cs_pdata[i].num_threads, cs_pdata[i].process_memory);
        else
          stemp = utils.StrFormat("%u,%.3f,%.3f,%.6f,%.6f,%.1f,%u,%u,%u", uiFrame, cs_pdata[i].fps_current, cs_pdata[i].fps_average, 1000.0 / cs_pdata[i].fps_current, 1000.0 / cs_pdata[i].fps_average, cs_pdata[i].cpu_usage, cs_pdata[i].gpu_usage, cs_pdata[i].num_threads, cs_pdata[i].process_memory);
      }
      else
        stemp = utils.StrFormat("%u,%.3f,%.3f,%.6f,%.6f,%.1f,%u,%u", uiFrame, cs_pdata[i].fps_current, cs_pdata[i].fps_average, 1000.0 / cs_pdata[i].fps_current, 1000.0 / cs_pdata[i].fps_average, cs_pdata[i].cpu_usage, cs_pdata[i].num_threads, cs_pdata[i].process_memory);

      hCSVFile << stemp + "\n";
    }
  }

  hCSVFile.flush();
  hCSVFile.close();

  return sRet;
}


void PollKeys()
{
  if (Settings.bPauseBeforeExit)
  {
    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\nPress any key to exit...");

    for (;;)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      if (platform::ReadKey())
        break;
    }

    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s", Pad("").c_str());
    platform::ConsoleCursorUp(1);
  }

  return;
}


void PrintConsole(bool bUseStdOut, uint16_t wAttributes, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  std::string buffer = StrVFormat(fmt, args);
  va_end(args);

  if (wAttributes && Settings.bUseColor)
    platform::ConsoleSetColors(wAttributes);

  platform::ConsoleWrite(bUseStdOut, buffer);

  if (wAttributes && Settings.bUseColor)
    platform::ConsoleResetColors();

  return;
}


std::string Pad(std::string s_line)
{
  int iPadLen = platform::ConsoleWidth() - (int)s_line.length();
  if ((iPadLen > 0) && (iPadLen <= 1000))
    s_line.append(iPadLen - 1, ' ');

  return s_line;
}


unsigned int CalculateFrameInterval(std::string &s_avsfile, std::string &s_error)
{
  s_error = "";
  unsigned int uiFrameInterval = 10;

  platform::LibHandle hDLL;



  if (Settings.sAVSDLL == "")
    hDLL = platform::LoadLib(platform::kAvsLibName);
  else
    hDLL = platform::LoadLib(Settings.sAVSDLL.c_str());


  if (!hDLL)
  {
    s_error = std::string("Cannot load ") + platform::kAvsLibFileName;
    return uiFrameInterval;
  }

  IScriptEnvironment *AVS_env = 0;

  try
  {
    platform::TranslateCrashes();

    CREATE_ENV *CreateEnvironment = (CREATE_ENV *)platform::GetLibSymbol(hDLL, "CreateScriptEnvironment");
    if (!CreateEnvironment)
    {
      PrintConsole(Settings.bConUseStdOut, COLOR_ERROR, "\nError: Failed to load CreateScriptEnvironment()\n");
      PollKeys();
      return -1;
    }

    AVS_env = CreateEnvironment(AvisynthInfo.iInterfaceVersion);

    if (!AVS_env)
    {
      s_error = "Could not create IScriptenvironment";
      platform::FreeLib(hDLL);
      return uiFrameInterval;
    }


    AVS_linkage = AVS_env->GetAVSLinkage();
    AVSValue AVS_main;
    AVSValue AVS_temp;
    PClip AVS_clip;
    VideoInfo   AVS_vidinfo;

    AVS_main = InvokeWithPath(AVS_env, "Import", s_avsfile, AvisynthInfo.bImportUtf8);

    if (!AVS_main.IsClip()) //not a clip
      AVS_env->ThrowError("Script did not return a video clip:\n%s", s_avsfile.c_str());

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

    stBenchUnits bu;
    InitBenchUnits(AVS_vidinfo, AVS_env, s_avsfile, bu);
    unsigned int uiTotalFrames = bu.uiUnits;

    unsigned int uiFrame = 0;
    double TDelta = 0.0;
    double T0 = timer.GetSTDTimer();
    uint64_t D0 = 0;
    while ((TDelta < 5.0) && (uiFrame < uiTotalFrames))
    {
      if ((timer.GetSTDTimerMS() - D0) >= 100)
      {
        PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\rPre-scanning script (F%u / T%.2lf)\r", uiFrame, TDelta);
        D0 = timer.GetSTDTimerMS();

        if (platform::KeyPressed())
        {
          if (platform::ReadKey() == 0x1B) //ESC
            AVS_env->ThrowError("\'ESC\' pressed, cancelled.");
        }
      }

      ProcessUnit(AVS_clip, AVS_env, uiFrame, bu);
      ++uiFrame;
      TDelta = timer.GetSTDTimer() - T0;
    }

    double dAverageFrameTime = 0.0;
    if (TDelta >= ((double)MIN_RUNTIME / 1000.0))
      dAverageFrameTime = (1000.0 * TDelta) / (double)uiFrame; //convert to milliseconds


    uiFrameInterval = 1000;
    if (dAverageFrameTime > (MIN_TIME_PER_FRAMEINTERVAL /  500.0)) uiFrameInterval =  500;
    if (dAverageFrameTime > (MIN_TIME_PER_FRAMEINTERVAL /  200.0)) uiFrameInterval =  200;
    if (dAverageFrameTime > (MIN_TIME_PER_FRAMEINTERVAL /  100.0)) uiFrameInterval =  100;
    if (dAverageFrameTime > (MIN_TIME_PER_FRAMEINTERVAL /   50.0)) uiFrameInterval =   50;
    if (dAverageFrameTime > (MIN_TIME_PER_FRAMEINTERVAL /   20.0)) uiFrameInterval =   20;
    if (dAverageFrameTime > (MIN_TIME_PER_FRAMEINTERVAL /   10.0)) uiFrameInterval =   10;
    if (dAverageFrameTime > (MIN_TIME_PER_FRAMEINTERVAL))          uiFrameInterval =    1;

    AVS_clip = 0;
    AVS_main = 0;
    AVS_temp = 0;
    std::this_thread::sleep_for(std::chrono::milliseconds(DSE_DELAY));
    AVS_env->DeleteScriptEnvironment();
    AVS_env = 0;
    AVS_linkage = 0;

    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());
  }
  catch (AvisynthError err)
  {
    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());
    s_error = platform::ToUtf8(err.msg);
    uiFrameInterval = 10;
  }
  catch (std::exception& ex)
  {
    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());
    s_error = ex.what();
    uiFrameInterval = 10;
  }
  catch (...)
  {
    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());
    s_error = utils.SysErrorMessage();
    uiFrameInterval = 10;
  }

  if (!platform::FreeLib(hDLL))
  {
    PrintConsole(Settings.bConUseStdOut, COLOR_DEFAULT, "\r%s\r", Pad("").c_str());
    s_error = std::string("Cannot unload ") + platform::kAvsLibFileName;
    uiFrameInterval = 10;
  }

  return uiFrameInterval;
}


void PrintUsage()
{
  PrintConsole(true, BG_BLACK | FG_HYELLOW, "\nUsage1:  AVSMeter script.avs [switches]\n");
  PrintConsole(true, BG_BLACK | FG_HYELLOW, "Usage2:  AVSMeter -avsinfo   [switches]\n\n");
  PrintConsole(true, BG_BLACK | FG_HGREEN, "Switches:\n");

  PrintConsole(true, COLOR_EMPHASIS, "  -avsinfo            Displays extended Avisynth info\n");
  PrintConsole(true, COLOR_EMPHASIS, "  -info   [-i]        Displays clip info\n");
  PrintConsole(true, COLOR_EMPHASIS, "  -log    [-l]        Creates log file\n");
  PrintConsole(true, COLOR_EMPHASIS, "  -csv                Creates csv file\n");
#ifdef _WIN32
  PrintConsole(true, COLOR_EMPHASIS, "  -gpu                Displays GPU/VPU usage (requires GPU-Z)\n");
#else
  PrintConsole(true, COLOR_EMPHASIS, "  -gpu[=sel]          Displays GPU/VPU usage (sel: nvidia, amd, intel,\n");
  PrintConsole(true, COLOR_EMPHASIS, "                      or n: NVIDIA GPU index)\n");
#endif
  PrintConsole(true, COLOR_EMPHASIS, "  -range=first,last   Sets frame range\n");
  PrintConsole(true, COLOR_EMPHASIS, "  -audio              Requests audio only (GetAudio)\n");
  PrintConsole(true, COLOR_EMPHASIS, "  -av                 Requests video and the audio of each frame\n");
  PrintConsole(true, COLOR_EMPHASIS, "  -chunk=n            Samples per audio request (default: one frame,\n");
  PrintConsole(true, COLOR_EMPHASIS, "                      %d for clips without video)\n", DEFAULT_AUDIO_CHUNK);
  PrintConsole(true, COLOR_EMPHASIS, "  -timelimit=n        Sets time limit (seconds)\n");
  PrintConsole(true, COLOR_EMPHASIS, "  -hp                 Sets process priority to high\n");
  PrintConsole(true, COLOR_EMPHASIS, "  -o                  Omits script pre-scanning\n");
  PrintConsole(true, COLOR_EMPHASIS, "  -lf                 Adds internal/external functions to the avsinfo*.log file\n");
  PrintConsole(true, COLOR_EMPHASIS, "  -p                  Pauses the program at the end and returns after pressing a key.\n\n\n");

  PrintConsole(true, COLOR_EMPHASIS, "  For more info on the command line switches and INI file\n");
  PrintConsole(true, COLOR_EMPHASIS, "  settings read the documentation (AVSMeter.html) included\n");
  PrintConsole(true, COLOR_EMPHASIS, "  in the distribution package.\n");

  PrintConsole(true, COLOR_DEFAULT, "\n");
  return;
}

