/*
  This file is part of AVSMeter, (c) Groucho2004.

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


#if !defined(_AVISYNTHINFO_H)
#define _AVISYNTHINFO_H

#include "common.h"
#include "exception.h"
#include "Utility.h"
#include "avs_headers/avisynth.h"

class CAvisynthInfo
{
public:
  CAvisynthInfo();
  virtual ~CAvisynthInfo();

  BOOL    GetInfo(string s_AVSDLL, string &s_ErrorMsg, BOOL b_ExtPlugCheck);
  int     iInterfaceVersion;
  vector  <string> vPluginDirs;
  vector  <string> vInternalFunctions;
  vector  <string> vPluginFunctions;
  vector  <string> vScriptFunctions;
  vector  <string> vAllFunctions;
  vector  <string> vPlugins;
  vector  <string> vPluginErrors;
  string  sDLLPath;
  string  sFileVersion;
  string  sProductVersion;
  string  sVersionString;
  string  sVersionNumber;
  string  sTimeStamp;
  BOOL    bIs64BitAVSDLL;
  BOOL    bIsAVSPlus;
  BOOL    bIsMTVersion;
  size_t  nLoadPlugInterval;

private:
  CUtils              utils;
  string              sAVSDLL;
  BOOL                IsGruntFunc(string s_function);
  void                EnumPluginDirs();
  void                EnumPluginDLLs();
  void                AddScriptFunctions(string s_avsi_file);
  void                CheckFunctionDuplicates();
  BOOL                GetPluginType(string s_dll, string &s_Msg, BOOL &b_Is64BitDLL);
  void                TestLoadPlugins();
  void                GetDLLDependencies(string s_dll, string &s_dependencies, string &s_failed_dependencies, string &s_hint, BOOL &fftw_fail);
  BOOL                CheckFFTW(string s_dll, string &s_failed_deps);
  BOOL                IsRuntimeInstalled(string s_version);
  __int64             FileSize(string s_file);
  BOOL                Is64BitOS();
  typedef             IScriptEnvironment * __stdcall CREATE_ENV(int);
};


#endif //_AVISYNTHINFO_H

