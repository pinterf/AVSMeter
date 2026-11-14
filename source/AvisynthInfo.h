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

  bool    GetInfo(string s_AVSDLL, string &s_ErrorMsg, bool b_ExtPlugCheck);
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
  bool    bIs64BitAVSDLL;
  bool    bIsAVSPlus;
  bool    bIsMTVersion;
  size_t  nLoadPlugInterval;

private:
  CUtils              utils;
  string              sAVSDLL;
  bool                IsGruntFunc(string s_function);
  void                EnumPluginDirs();
  void                EnumPluginDLLs();
  void                AddScriptFunctions(string s_avsi_file);
  void                CheckFunctionDuplicates();
  bool                GetPluginType(string s_dll, string &s_Msg, bool &b_Is64BitDLL);
  void                TestLoadPlugins();
  void                GetDLLDependencies(string s_dll, string &s_dependencies, string &s_failed_dependencies, string &s_hint, bool &fftw_fail);
  bool                CheckFFTW(string s_dll, string &s_failed_deps);
  bool                IsRuntimeInstalled(string s_version);
  int64_t             FileSize(string s_file);
  bool                Is64BitOS();
  typedef             IScriptEnvironment * __stdcall CREATE_ENV(int);
};


#endif //_AVISYNTHINFO_H

