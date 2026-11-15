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
#include "Utility.h"
#include "avs_headers/avisynth.h"

class CAvisynthInfo
{
public:
  CAvisynthInfo();
  virtual ~CAvisynthInfo();

  bool                     GetInfo(std::string s_AVSDLL, std::string &s_ErrorMsg, bool b_ExtPlugCheck);
  int                      iInterfaceVersion;
  std::vector<std::string> vPluginDirs;
  std::vector<std::string> vInternalFunctions;
  std::vector<std::string> vPluginFunctions;
  std::vector<std::string> vScriptFunctions;
  std::vector<std::string> vAllFunctions;
  std::vector<std::string> vPlugins;
  std::vector<std::string> vPluginErrors;
  std::string              sDLLPath;
  std::string              sFileVersion;
  std::string              sProductVersion;
  std::string              sVersionString;
  std::string              sVersionNumber;
  std::string              sTimeStamp;
  bool                     bIs64BitAVSDLL;
  bool                     bIsAVSPlus;
  bool                     bIsMTVersion;
  bool                     bImportUtf8;      // Import(utf8=true): AviSynth+ r2487+
  bool                     bLoadPluginUtf8;  // LoadPlugin(utf8=true): AviSynth+ 3.7.6+
  size_t                   nLoadPlugInterval;

private:
  CUtils              utils;
  std::string         sAVSDLL;
  std::vector<std::string> vAutoloadDirs;
  bool                bAutoloadDirsKnown;
  bool                IsGruntFunc(std::string s_function);
  void                EnumPluginDirs();
  void                EnumPluginDLLs();
  void                AddScriptFunctions(std::string s_avsi_file);
  void                CheckFunctionDuplicates();
  bool                GetPluginType(std::string s_dll, std::string &s_Msg, bool &b_Is64BitDLL);
  void                TestLoadPlugins();
  void                GetDLLDependencies(std::string s_dll, std::string &s_dependencies, std::string &s_failed_dependencies, std::string &s_hint, bool &fftw_fail);
  bool                CheckFFTW(std::string s_dll, std::string &s_failed_deps);
  typedef             IScriptEnvironment * __stdcall CREATE_ENV(int);
};


// Invokes a function taking a UTF-8 file name (Import, LoadPlugin): ASCII as is, else
// with utf8=true if b_utf8, else the ANSI path if representable (ThrowError if not).
AVSValue InvokeWithPath(IScriptEnvironment *AVS_env, const char *psz_function, const std::string &s_path, bool b_utf8);


#endif //_AVISYNTHINFO_H

