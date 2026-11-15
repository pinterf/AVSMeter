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


#include "AvisynthInfo.h"
#include "platform/avs_registry.h"
#include "platform/binary_info.h"
#include "platform/crash_guard.h"
#include "platform/dynlib.h"
#include "platform/file_info.h"
#include "platform/os_info.h"
#include "platform/text.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iterator>
#include <system_error>

namespace
{
  const char PATH_SEP = (char)std::filesystem::path::preferred_separator;
#ifdef _WIN32
const char *const UNCATEGORIZED_LIBS = "Uncategorized DLLs";
#else
const char *const UNCATEGORIZED_LIBS = "Uncategorized libraries";
#endif

// Category label for a GetPluginType() result.
std::string PluginCategory(const std::string &s_type)
{
  if (s_type == "AVSC") return "C-Interface Plugins (classic)";
  if (s_type == "AVSCINIT12") return "C-Interface Plugins (classic + V11+)";
  if (s_type == "AVSCINIT2") return "C-Interface Plugins (V11+ only)";
  if (s_type == "AVSC20") return "C-Interface Plugins (cdecl, obsolete)";
  if (s_type == "AVSCPP26") return "C++ 2.6 Plugins";
  if (s_type == "AVSCPP25") return "C++ 2.5 Plugins";
  if (s_type == "AVSCPP20") return "C++ 2.0 Plugins";
  return UNCATEGORIZED_LIBS;
}
}


const AVS_Linkage *AVS_linkage = 0;


AVSValue InvokeWithPath(IScriptEnvironment *AVS_env, const char *psz_function, const std::string &s_path, bool b_utf8)
{
  bool bAscii = std::all_of(s_path.begin(), s_path.end(), [](char c) { return (unsigned char)c < 0x80; });
  if (bAscii)
    return AVS_env->Invoke(psz_function, s_path.c_str());

  if (b_utf8)
  {
    AVSValue avsArgs[2] = { s_path.c_str(), true };
    const char *pszNames[2] = { NULL, "utf8" };
    return AVS_env->Invoke(psz_function, AVSValue(avsArgs, 2), pszNames);
  }

  std::string sLegacy;
  if (!platform::LegacyPath(s_path, sLegacy))
    AVS_env->ThrowError("%s: \"%s\":\nPath not supported by this AviSynth version (not UTF-8 capable)", psz_function, s_path.c_str());

  return AVS_env->Invoke(psz_function, sLegacy.c_str());
}


CAvisynthInfo::CAvisynthInfo()
{
}

CAvisynthInfo::~CAvisynthInfo()
{
}


bool CAvisynthInfo::GetInfo(std::string s_AVSDLL, std::string &s_ErrorMsg, bool b_ExtPlugCheck)
{
  sDLLPath = "";
  bIs64BitAVSDLL = false;
  bIsMTVersion = false;
  bIsAVSPlus = false;
  bImportUtf8 = false;
  bLoadPluginUtf8 = false;
  iInterfaceVersion = 0;
  sFileVersion = "";
  sProductVersion = "";
  sVersionString = "Unknown Avisynth Version";
  bAutoloadDirsKnown = false;
  vAutoloadDirs.clear();
  bool bAVSLinkage = false;
  bool bSuccess = true;
  s_ErrorMsg = "";

  platform::BinaryInfo bi;
  bool bLoaded = platform::InspectBinary((s_AVSDLL == "") ? platform::kAvsLibName : s_AVSDLL, bi);

  //Need a local copy
  sAVSDLL = s_AVSDLL;

  if (!bLoaded)
  {
    s_ErrorMsg = utils.SysErrorMessage();
    if (s_ErrorMsg != "")
      s_ErrorMsg = utils.StrFormat("Error: Cannot load %s.\nThe system returned: \"%s\"\n\nAvisynth (%s) is probably not installed.", platform::kAvsLibFileName, s_ErrorMsg.c_str(), PROCESS_64 ? "x64" : "x86");
    else
      s_ErrorMsg = std::string("Error: Cannot load ") + platform::kAvsLibFileName;

    return false;
  }

  sDLLPath = bi.path;

  if (bi.is64Bit)
  {
    bIs64BitAVSDLL = true;
    if (!PROCESS_64) //trying to load 64 bit avisynth.dll with 32 bit AVSMeter
    {
      s_ErrorMsg = utils.StrFormat("Error: AVSMeter (x86) cannot load a 64 Bit %s.", platform::kAvsLibFileName);
      return false;
    }
  }
  else //32 Bit Avisynth DLL
  {
    if (PROCESS_64) //trying to load 32 bit avisynth.dll with 64 bit AVSMeter
    {
      s_ErrorMsg = utils.StrFormat("Error: AVSMeter (x64) cannot load a 32 Bit %s.", platform::kAvsLibFileName);
      return false;
    }
  }

  for (std::string sName : bi.exports)
  {
    utils.StrToLC(sName);
    if (sName.find("avs_linkage") != std::string::npos)
    {
      bAVSLinkage = true;
      break;
    }
  }


  if (!bAVSLinkage)
  {
    std::string sFileVersion = platform::FileVersion(sDLLPath);

    if (sFileVersion.substr(0, 3) == "2.6")
      s_ErrorMsg = utils.StrFormat("Error: The installed Avisynth version is a 2.6 Alpha or RC:\n%s\n\nUpdate to Avisynth 2.6 Release or Avisynth+\n\n", platform::NativeSystemPath(sDLLPath).c_str());
    else
      s_ErrorMsg = utils.StrFormat("Error: Avisynth version is too old:\n%s (Version: %s)\n\nUpdate to Avisynth 2.6 or Avisynth+\n\n", platform::NativeSystemPath(sDLLPath).c_str(), sFileVersion.c_str());

    return false;
  }


  sDLLPath = platform::NativeSystemPath(sDLLPath);
  sFileVersion = platform::FileVersion(sDLLPath);
  sProductVersion = platform::ProductVersion(sDLLPath);
  sTimeStamp = utils.GetFileTimeStamp(sDLLPath);

  std::string sDeps = "";
  std::string sFailedDeps = "";
  std::string sNote = "";
  std::string sHint = "";
  bool bFFTWFail = false;
  GetDLLDependencies(sDLLPath, sDeps, sFailedDeps, sHint, bFFTWFail);

  if (sFailedDeps != "")
  {
    s_ErrorMsg = utils.StrFormat("Error: Cannot load %s\n\nDependencies that could not be loaded:\n%s", platform::kAvsLibFileName, sFailedDeps.c_str());
    if (sHint != "")
      s_ErrorMsg += utils.StrFormat("\n\nNote:\n  %s\n", sHint.c_str());

    return false;
  }

  platform::LibHandle hDLL;
  if (sAVSDLL == "")
    hDLL = platform::LoadLib(platform::kAvsLibName);
  else
    hDLL = platform::LoadLib(sAVSDLL.c_str());


  if (!hDLL)
  {
    s_ErrorMsg = utils.SysErrorMessage();
    if (s_ErrorMsg != "")
      s_ErrorMsg = std::string("Error: Cannot load ") + platform::kAvsLibFileName + ":\n" + s_ErrorMsg;
    else
      s_ErrorMsg = std::string("Error: Cannot load ") + platform::kAvsLibFileName;

    return false;
  }

  IScriptEnvironment *AVS_env = 0;

  try
  {
    platform::TranslateCrashes();

    CREATE_ENV *CreateEnvironment = (CREATE_ENV *)platform::GetLibSymbol(hDLL, "CreateScriptEnvironment");
    if (!CreateEnvironment)
    {
      s_ErrorMsg = "Error: Failed to load CreateScriptEnvironment()";
      platform::FreeLib(hDLL);
      return false;
    }

    iInterfaceVersion = 16;
    while (!AVS_env)
    {
      iInterfaceVersion--;

      if (iInterfaceVersion < 5)
      {
        s_ErrorMsg = "Error: Cannot create IScriptenvironment";
        platform::FreeLib(hDLL);
        return 0;
      }

      AVS_env = CreateEnvironment(iInterfaceVersion);
    }


    AVS_linkage = AVS_env->GetAVSLinkage();
    AVSValue foo;

    // Run the plugin autoload now, on our terms: a plugin that throws while autoloading (e.g. wrong
    // calling convention or bitness) would otherwise abort the whole query from a later lookup.
    // The autoload is marked done before it starts, so it throws only once.
    try
    {
      AVS_env->FunctionExists("AVSMeter_TriggerAutoload");
    }
    catch (AvisynthError err)
    {
      std::string sAutoloadError = platform::ToUtf8(err.msg);
      utils.StrTrim(sAutoloadError);
      vPluginErrors.push_back("Plugin autoload stopped: " + sAutoloadError +
        "\n\nNote: Plugins after this one were not autoloaded, their functions are missing from the lists");
    }

    if (AVS_env->FunctionExists("AutoloadPlugins"))
      bIsAVSPlus = true;
    if ((AVS_env->FunctionExists("SetMTMode")) || (AVS_env->FunctionExists("Prefetch")))
      bIsMTVersion = true;

    try
    {
      std::string sTemp = utils.StrFormat("%f", AVS_env->Invoke("VersionNumber", AVSValue(&foo, 0)).AsFloat());
      size_t x;
      for (x = sTemp.length() - 1; x > 3; x--)
      {
        if (sTemp[x] != '0')
          break;
      }
      sVersionNumber = sTemp.substr(0, x + 1);
    }
    catch (IScriptEnvironment::NotFound)
    {
      sVersionNumber = "Error: Cannot invoke \"VersionNumber\"";
    }

    try
    {
      sVersionString = utils.StrFormat("%s", AVS_env->Invoke("VersionString", AVSValue(&foo, 0)).AsString());
    }
    catch (IScriptEnvironment::NotFound)
    {
      sVersionString = "Error: Cannot invoke \"VersionString\"";
    }

    if (bIsAVSPlus)
    {
      // Revision as in "AviSynth+ 3.7.6 (r4929, ...)"; release branches are numbered differently, use the file version where possible
      size_t pos = sVersionString.find("(r");
      bImportUtf8 = (pos != std::string::npos) && (atoi(sVersionString.c_str() + pos + 2) >= 2487);

      int iMajor = 0, iMinor = 0, iPatch = 0;
      sscanf(sFileVersion.c_str(), "%d.%d.%d", &iMajor, &iMinor, &iPatch);
      bLoadPluginUtf8 = (iMajor * 10000 + iMinor * 100 + iPatch) >= 30706;
    }

    if (AVS_env->FunctionExists("ListAutoloadDirs")) //Avisynth+ 3.7.4 and later
    {
      try
      {
        // AviSynth+ 3.7.6+: ANSI by default ('?' for unmappable chars), UTF-8 on request
        AVSValue utf8_arg = true;
        const char *utf8_name = "utf8";
        AVSValue sDirs = bLoadPluginUtf8 ?
          AVS_env->Invoke("ListAutoloadDirs", AVSValue(&utf8_arg, 1), &utf8_name) :
          AVS_env->Invoke("ListAutoloadDirs", AVSValue(&foo, 0));
        utils.StrTokenize(sDirs.AsString(""), vAutoloadDirs, "\n", false);
        for (std::string &sDir : vAutoloadDirs)
          sDir = platform::ToUtf8(sDir); // ANSI before AviSynth+ 3.7.6
        bAutoloadDirsKnown = true;
      }
      catch (IScriptEnvironment::NotFound)
      {
        vAutoloadDirs.clear();
      }
      catch (AvisynthError)
      {
        vAutoloadDirs.clear();
      }
    }

    std::string sInternalFunctions = "";
    try
    {
      foo = AVS_env->GetVar("$InternalFunctions$");
      sInternalFunctions = foo.AsString();
      utils.StrTokenize(sInternalFunctions, vInternalFunctions, " ", true);
      std::sort(vInternalFunctions.begin(), vInternalFunctions.end(), CompareNoCase);
    }
    catch(...)
    {
      sInternalFunctions = "";
    }

    std::string sDLLFunctions = "";
    try
    {
      foo = AVS_env->GetVar("$PluginFunctions$");
      sDLLFunctions = foo.AsString();
      utils.StrTokenize(sDLLFunctions, vPluginFunctions, " ", true);
      std::sort(vPluginFunctions.begin(), vPluginFunctions.end(), CompareNoCase);
    }
    catch(...)
    {
      sDLLFunctions = "";
    }

    foo = 0;
    AVS_env->DeleteScriptEnvironment();
    AVS_env = 0;
    AVS_linkage = 0;
  }
  catch (AvisynthError err)
  {
    bSuccess = false;
    s_ErrorMsg = platform::ToUtf8(err.msg);
  }
  catch (std::exception& ex)
  {
    bSuccess = false;
    s_ErrorMsg = ex.what();
  }
  catch (...)
  {
    bSuccess = false;
    s_ErrorMsg = utils.SysErrorMessage();
    if (s_ErrorMsg != "")
      s_ErrorMsg = std::string("Error: Cannot load ") + platform::kAvsLibFileName + ":\n" + s_ErrorMsg;
    else
      s_ErrorMsg = std::string("Error: Cannot load ") + platform::kAvsLibFileName;
  }

  platform::FreeLib(hDLL);

  if (!bSuccess)
    return false;


  std::string sFunction = "";
  for (size_t InternalFunctions = 0; InternalFunctions < vInternalFunctions.size(); InternalFunctions++)
  {
    sFunction = vInternalFunctions[InternalFunctions];
    vAllFunctions.push_back(sFunction + "|" + "[InternalFunction]");
  }

  for (size_t PluginFunctions = 0; PluginFunctions < vPluginFunctions.size(); PluginFunctions++)
  {
    sFunction = vPluginFunctions[PluginFunctions];
    vAllFunctions.push_back(sFunction + "|" + "[PluginFunction]");
  }


  EnumPluginDirs();

  if (b_ExtPlugCheck)
  {
    EnumPluginDLLs();
    TestLoadPlugins();
  }

  return bSuccess;
}


static std::string TrimDirSeparators(std::string s_dir)
{
  //keep the separator of a drive root ("C:\\")
  while ((s_dir.length() > 1) && ((s_dir.back() == '/') || (s_dir.back() == '\\')) && (s_dir[s_dir.length() - 2] != ':'))
    s_dir.pop_back();

  return s_dir;
}


static std::string ComparableDir(std::string s_dir)
{
#ifdef _WIN32
  replace(s_dir.begin(), s_dir.end(), '/', '\\');
  std::transform(s_dir.begin(), s_dir.end(), s_dir.begin(), [](unsigned char c) { return (char)(((c >= 'A') && (c <= 'Z')) ? c + ('a' - 'A') : c); });
#endif
  return TrimDirSeparators(s_dir);
}


void CAvisynthInfo::EnumPluginDirs()
{
  vPluginDirs.clear();
  const char *pszArch = PROCESS_64 ? "x64" : "x86";

  std::vector<std::string> vRegLabels;
  std::vector<std::string> vRegDirs;
  for (const platform::RegisteredPluginDir &regDir : platform::RegisteredPluginDirs())
  {
    std::string sPluginDir = regDir.path;
    utils.StrTrim(sPluginDir);

    if ((sPluginDir == "") || ((regDir.valueName == "PluginDir+") && !bIsAVSPlus))
      continue;

    vRegLabels.push_back(utils.StrFormat("%-12s (%s, %s)", regDir.valueName.c_str(), regDir.hive.c_str(), pszArch));
    vRegDirs.push_back(sPluginDir);
  }

  if (!bAutoloadDirsKnown)
  {
    for (size_t i = 0; i < vRegDirs.size(); i++)
      vPluginDirs.push_back(vRegLabels[i] + ":\t" + vRegDirs[i]);

    if (vPluginDirs.size() < 1)
      vPluginErrors.push_back("No plugin directory references found in the registry.\nPlugin auto-loading disabled.");

    return;
  }

  //Avisynth+ reports the directories it actually searches, in search order.
  //Show the registry label and spelling where a directory comes from the registry.
  for (const std::string &sDir : vAutoloadDirs)
  {
    std::string sLabel = utils.StrFormat("Autoload dir (AVS+, %s)", pszArch);
    std::string sShownDir = TrimDirSeparators(sDir);

    for (size_t i = 0; i < vRegDirs.size(); i++)
    {
      if ((vRegDirs[i] != "") && (ComparableDir(vRegDirs[i]) == ComparableDir(sDir)))
      {
        sLabel = vRegLabels[i];
        sShownDir = vRegDirs[i];
        vRegDirs[i] = "";
        break;
      }
    }

    vPluginDirs.push_back(sLabel + ":\t" + sShownDir);
  }

  if (vPluginDirs.size() < 1)
    vPluginErrors.push_back("Avisynth+ reports no plugin autoload directories.\nPlugin auto-loading disabled.");

  return;
}


void CAvisynthInfo::EnumPluginDLLs()
{
  std::string sDir = "";
  std::string sMessage = "";
  bool bIs64BitPlugin = false;
  std::set<std::string> mDuplicates;
  //Like Avisynth+: per base name only the first loaded plugin / imported script is autoloaded,
  //in directory search order (known with ListAutoloadDirs)
  std::map<std::string, std::string> mAutoloaded;
  std::map<std::string, std::string> mImported;

  for (size_t uiPlugDir = 0; uiPlugDir < vPluginDirs.size(); uiPlugDir++)
  {
    sDir = vPluginDirs[uiPlugDir];
    if (sDir != "")
    {
      size_t spos = sDir.find(":\t");
      sDir = sDir.substr(spos + 2);

      std::string sCurrentFile = "";
      std::string sCurrentFileKey = "";
      std::error_code ec;
      for (std::filesystem::directory_iterator it(platform::PathFromUtf8(sDir), ec), end; !ec && it != end; it.increment(ec))
      {
        std::error_code ecType;
        if (it->is_directory(ecType))
          continue;

        std::string sName = platform::PathToUtf8(it->path().filename());

        sCurrentFile = sDir + PATH_SEP + sName;

        //Like Avisynth+: path::extension() (a bare ".so" has none), case-sensitive on POSIX
        std::string sExt = platform::PathToUtf8(it->path().extension());
        sCurrentFileKey = sCurrentFile;
#ifdef _WIN32
        utils.StrToLC(sExt);
        utils.StrToLC(sCurrentFileKey);
#endif

        if (mDuplicates.find(sCurrentFileKey) != mDuplicates.end())
          continue;

        mDuplicates.insert(sCurrentFileKey);

        std::string sBaseKey = platform::PathToUtf8(it->path().stem());
#ifdef _WIN32
        utils.StrToLC(sBaseKey);
#endif

        if (sExt == platform::kPluginExt)
        {
          if (!GetPluginType(sCurrentFile, sMessage, bIs64BitPlugin))
          {
            if (sMessage != "")
              vPluginErrors.push_back(sMessage);
          }
          else
          {
            vPlugins.push_back(PluginCategory(sMessage) + (bIs64BitPlugin ? " (64 Bit)|" : " (32 Bit)|") + sCurrentFile);

            if (bAutoloadDirsKnown)
            {
              auto itLoaded = mAutoloaded.find(sBaseKey);
              if (itLoaded != mAutoloaded.end())
                mShadowedBy[sCurrentFile] = itLoaded->second;
              else if (IsAutoloadable(sMessage, bIs64BitPlugin))
                mAutoloaded[sBaseKey] = sCurrentFile;
            }
          }
        }
        else if (sExt == ".avsi")
        {
          vPlugins.push_back("Scripts (AVSI)|" + sCurrentFile);

          auto itImported = mImported.find(sBaseKey);
          if (bAutoloadDirsKnown && (itImported != mImported.end()))
            mShadowedBy[sCurrentFile] = itImported->second;
          else
          {
            mImported[sBaseKey] = sCurrentFile;
            AddScriptFunctions(sCurrentFile);
          }
        }
        else
          vPlugins.push_back("Uncategorized files|" + sCurrentFile);
      }
    }
  }

  std::sort(vPlugins.begin(), vPlugins.end(), CompareNoCase);

  if (!mShadowedBy.empty())
  {
    std::string sShadowed = "Shadowed, not autoloaded (an earlier autoload directory has the same base name):\n";
    for (const auto &shadowed : mShadowedBy)
      sShadowed += utils.StrFormat("\n\"%s\"\n  shadowed by \"%s\"", shadowed.first.c_str(), shadowed.second.c_str());
    vPluginErrors.push_back(sShadowed);
  }

  CheckFunctionDuplicates();

  return;
}


//A plugin of this type and bitness is loaded by the Avisynth+ autoload (and shadows later ones)
bool CAvisynthInfo::IsAutoloadable(const std::string &s_type, bool b_Is64BitDLL)
{
  if (b_Is64BitDLL != PROCESS_64)
    return false;
  if ((s_type == "AVSCPP26") || (s_type == "AVSCPP25") || (s_type == "AVSC") || (s_type == "AVSCINIT12"))
    return true;
  return (s_type == "AVSCINIT2") && (iInterfaceVersion >= 11);
}


void CAvisynthInfo::AddScriptFunctions(std::string s_avsi_file)
{
  std::string sOrgLine = "";
  std::string sCurrentLine = "";
  size_t BRPos = 0;
  std::string sFunctionName = "";
  std::string sFunctionNameLC = "";
  std::string sAVSIError = "";

  std::ifstream h_avsi_file(platform::PathFromUtf8(s_avsi_file));
  if (!h_avsi_file.is_open())
    return;

  while (std::getline(h_avsi_file, sCurrentLine))
  {
    sCurrentLine = platform::ToUtf8(sCurrentLine);
    utils.StrTrim(sCurrentLine);
    sOrgLine = sCurrentLine;
    utils.StrToLC(sCurrentLine);

    if (sCurrentLine.substr(0, 8) == "function")
    {
      BRPos = sCurrentLine.find("(");
      if (BRPos > 9)
      {
        sFunctionName = sOrgLine.substr(8, BRPos - 8);
        utils.StrTrim(sFunctionName);
        vAllFunctions.push_back(sFunctionName + "|" + s_avsi_file);
        vScriptFunctions.push_back(sFunctionName);
      }
    }
  }

  h_avsi_file.close();

  return;
}


void CAvisynthInfo::CheckFunctionDuplicates()
{
  std::string sArrayEntry_i = "";
  std::string sArrayEntry_y = "";
  std::string sFunctionLC_i = "";
  std::string sFunctionLC_i_Old = "";
  std::string sFunction_i = "";
  std::string sOrigin_i = "";
  std::string sAVSIError_i = "";
  size_t spos_i = 0;
  std::string sFunction_y = "";
  std::string sFunctionLC_y = "";
  std::string sOrigin_y = "";
  std::string sAVSIError = "";
  std::string sGruntTest = "";

  size_t spos_y = 0;
  std::set<std::string> stDups;
  std::sort(vAllFunctions.begin(), vAllFunctions.end(), CompareNoCase);

  for (size_t i = 0; i < vAllFunctions.size() - 1; i++)
  {
    sArrayEntry_i = vAllFunctions[i];
    spos_i = sArrayEntry_i.find("|");
    sOrigin_i = sArrayEntry_i.substr(spos_i + 1);
    sFunction_i = sArrayEntry_i.substr(0, spos_i);
    sFunctionLC_i = sFunction_i;
    utils.StrToLC(sFunctionLC_i);

    for (size_t y = i + 1; y < vAllFunctions.size(); y++)
    {
      sArrayEntry_y = vAllFunctions[y];
      spos_y = sArrayEntry_y.find("|");
      sOrigin_y = sArrayEntry_y.substr(spos_y + 1);
      sFunction_y = sArrayEntry_y.substr(0, spos_y);
      sFunctionLC_y = sFunction_y;
      utils.StrToLC(sFunctionLC_y);


      if (sOrigin_y == "[PluginFunction]")
      {
        if (IsGruntFunc(sFunctionLC_y))
          continue;
      }

      if (sFunctionLC_i == sFunctionLC_y)
      {
        utils.StrToLC(sArrayEntry_i);
        utils.StrToLC(sArrayEntry_y);

        if ((stDups.find(sArrayEntry_i) == stDups.end()) && (stDups.find(sArrayEntry_y) == stDups.end()))
        {
          sAVSIError += utils.StrFormat("\"%s\" : \"%s\"\n", sFunction_i.c_str(), sOrigin_i.c_str());
          sAVSIError += utils.StrFormat("\"%s\" : \"%s\"\n", sFunction_y.c_str(), sOrigin_y.c_str());
          stDups.insert(sArrayEntry_i);
          stDups.insert(sArrayEntry_y);
        }
      }
    }
  }

  if (sAVSIError != "")
  {
    sAVSIError = "Function duplicates:                                                            \n\n" + sAVSIError;
    vPluginErrors.push_back(sAVSIError);
  }

  return;
}


bool CAvisynthInfo::GetPluginType(std::string s_dll, std::string &s_Msg, bool &b_Is64BitDLL)
{
  b_Is64BitDLL = false;
  s_Msg = "UNCATEGORIZED";

  platform::BinaryInfo bi;
  if (!platform::InspectBinary(s_dll, bi))
  {
    s_Msg = utils.SysErrorMessage();
    if (s_Msg != "")
      s_Msg = utils.StrFormat("Error loading \"%s\":\n%s", s_dll.c_str(), s_Msg.c_str());
    else
      s_Msg = utils.StrFormat("Error loading \"%s\"", s_dll.c_str());

    return false;
  }

  b_Is64BitDLL = bi.is64Bit;
  if (b_Is64BitDLL != PROCESS_64)
    return true;

  // C plugin entry points, by the names AviSynth+ looks up (PluginManager TryAsAvsC/TryAsAvsPreV11C).
  // init2 (V11+): the plugin handles 64-bit AVS_Value types; AviSynth+ calls it first.
  // init: classic C-Interface plugin (stdcall avisynth_c 0.20 API, built in since AviSynth 2.54).
  // Undecorated on 32-bit Windows: a cdecl plugin for the external avisynth_c.dll of the
  // AviSynth 2.53 era, unless it also has init2 (AviSynth+ accepts the undecorated name then).
  bool bCppInit3 = false, bCppInit2 = false, bCppInit = false;
  bool bCInit = false, bCInit2 = false, bCInitUndecorated = false;

  for (std::string sExportFunc : bi.exports)
  {
    utils.StrToLC(sExportFunc); //convert to lower case for comparison

    if (sExportFunc.find("avisynthplugininit3") != std::string::npos) //CPP 2.6, 32 or 64 bit
      bCppInit3 = true;
    else if (sExportFunc.find("avisynthplugininit2") != std::string::npos) //CPP 2.5, 32 or 64 bit
      bCppInit2 = true;
#ifdef _WIN32
    else if ((sExportFunc.find("avisynthplugininit") != std::string::npos) && !b_Is64BitDLL) //CPP 2.0
      bCppInit = true;

    if ((sExportFunc == "_avisynth_c_plugin_init@4") || (sExportFunc == "avisynth_c_plugin_init@4"))
      bCInit = true;
    else if (sExportFunc == "avisynth_c_plugin_init")
    {
      if (b_Is64BitDLL)
        bCInit = true;
      else
        bCInitUndecorated = true;
    }

    if ((sExportFunc == "avisynth_c_plugin_init2") || (sExportFunc == "_avisynth_c_plugin_init2@4") || (sExportFunc == "avisynth_c_plugin_init2@4"))
      bCInit2 = true;
#else
    //No stdcall decoration and no 2.0 plugin interfaces on POSIX
    if (sExportFunc == "avisynth_c_plugin_init")
      bCInit = true;
    else if (sExportFunc == "avisynth_c_plugin_init2")
      bCInit2 = true;
#endif
  }

  if (bCppInit3)
    s_Msg = "AVSCPP26";
  else if (bCppInit2)
    s_Msg = "AVSCPP25";
  else if (bCInit2)
    s_Msg = (bCInit || bCInitUndecorated) ? "AVSCINIT12" : "AVSCINIT2";
  else if (bCInit)
    s_Msg = "AVSC";
  else if (bCInitUndecorated)
    s_Msg = "AVSC20";
  else if (bCppInit)
    s_Msg = "AVSCPP20";

  return true;
}


void CAvisynthInfo::TestLoadPlugins()
{
  platform::LibHandle hDLL;
  if (sAVSDLL == "")
    hDLL = platform::LoadLib(platform::kAvsLibName);
  else
    hDLL = platform::LoadLib(sAVSDLL.c_str());

  if (!hDLL)
    return;

  std::string sPlugLoadError = "";
  std::string sPlugLoadErrorLC = "";
  std::string sNote = "";
  std::string sPlugin = "";
  std::string sPluginType = "";
  size_t uiPlugin = 0;
  size_t spos = 0;
  std::string sDependencies = "";
  std::string sFailedDependencies = "";
  std::string sHint = "";
  std::string sMsg = "";
  std::string sTemp = "";
  bool bFFTWFail = false;

  IScriptEnvironment *AVS_env = 0;

  for (uiPlugin = 0; uiPlugin < vPlugins.size(); uiPlugin++)
  {
    if (AVS_env == 0)
    {
      try
      {
        platform::TranslateCrashes();

        CREATE_ENV *CreateEnvironment = (CREATE_ENV *)platform::GetLibSymbol(hDLL, "CreateScriptEnvironment");
        if (!CreateEnvironment)
        {
          platform::FreeLib(hDLL);
          return;
        }

        AVS_env = CreateEnvironment(iInterfaceVersion);

        if (!AVS_env)
        {
          platform::FreeLib(hDLL);
          return;
        }
      }
      catch (std::exception& ex)
      {
        platform::FreeLib(hDLL);
        std::string dummy = ex.what();
        return;
      }

      AVS_linkage = AVS_env->GetAVSLinkage();

      // Test each plugin on its own: a plugin calling a function while loading triggers the autoload,
      // and a broken autoloaded plugin's error would be blamed on the plugin under test
      if (bIsAVSPlus)
      {
        try
        {
          AVSValue foo;
          AVS_env->Invoke("ClearAutoloadDirs", AVSValue(&foo, 0));
        }
        catch (...)
        {
        }
      }
    }

    sPlugin = vPlugins[uiPlugin];
    spos = sPlugin.find("|");
    if (spos < 3)
      continue;

    sPluginType = sPlugin.substr(0, spos);

    if (sPluginType.find("Plugins") == std::string::npos)
      continue;

    sPlugin = sPlugin.substr(spos + 1);

    sDependencies = "";
    sFailedDependencies = "";
    sHint = "";
    bFFTWFail = false;
    GetDLLDependencies(sPlugin, sDependencies, sFailedDependencies, sHint, bFFTWFail);

    sPlugLoadError = "";
    sNote = "";

    try
    {
      if (bFFTWFail)
      {
        sPlugLoadError = utils.StrFormat("\"%s\" requires the FFTW library for some functions.", sPlugin.c_str());
        if (sHint != "")
          sPlugLoadError += utils.StrFormat("\n\nNote: %s", sHint.c_str());

        vPluginErrors.push_back(sPlugLoadError);
        sPlugLoadError = "";
        sNote = "";
      }

      InvokeWithPath(AVS_env, "LoadPlugin", sPlugin, bLoadPluginUtf8);
    }
    catch (AvisynthError err)
    {
      sPlugLoadError = platform::ToUtf8(err.msg);
      utils.StrTrim(sPlugLoadError);
      sPlugLoadErrorLC = sPlugLoadError;
      utils.StrToLC(sPlugLoadErrorLC);

      if (sFailedDependencies != "")
      {
        sNote += utils.StrFormat("\n\nDependencies that could not be loaded:\n%s", sFailedDependencies.c_str());
        if (sHint != "")
          sNote += utils.StrFormat("\n\nNote: %s", sHint.c_str());
      }

      if ((sPlugLoadErrorLC.find("proc not found") != std::string::npos) || (sPlugLoadErrorLC.find("the specified procedure could not be found") != std::string::npos))
        sNote += "\n\nNote: You may need a newer OS version in order to use this plugin";

      if ((sPluginType.rfind("C-Interface Plugins (classic", 0) == 0) && (!bIsAVSPlus))
        sNote += "\n\nNote: C-Plugins must be loaded explicitly with \"LoadCPlugin()\"";

      if ((sPluginType.rfind("C-Interface Plugins (V11+ only)", 0) == 0) && (!bIsAVSPlus || (iInterfaceVersion < 11)))
        sNote += "\n\nNote: This plugin has only the \"avisynth_c_plugin_init2\" entry point, it requires Avisynth+ with interface version 11 or newer";

      if ((sPluginType.rfind("C-Interface Plugins (cdecl", 0) == 0) && (bIsAVSPlus))
        sNote += "\n\nNote: cdecl C-Interface plugins (external avisynth_c.dll, AviSynth 2.53) are not supported by Avisynth+";

      if ((sPluginType.rfind("C++ 2.0 Plugins", 0) == 0) && (bIsAVSPlus))
        sNote += "\n\nNote: C++ 2.0 plugins are not supported by Avisynth+";

      sPlugLoadError += sNote;

      if (sPlugLoadError != "")
        vPluginErrors.push_back(sPlugLoadError);
    }

    if (((uiPlugin % nLoadPlugInterval) == 0) && (AVS_env != 0))
    {
      AVS_env->DeleteScriptEnvironment();
      AVS_env = 0;
      AVS_linkage = 0;
    }
  }

  if (AVS_env != 0)
  {
    AVS_env->DeleteScriptEnvironment();
    AVS_env = 0;
    AVS_linkage = 0;
  }

  platform::FreeLib(hDLL);

  return;
}


void CAvisynthInfo::GetDLLDependencies(std::string s_dll, std::string &s_dependencies, std::string &s_failed_dependencies, std::string &s_hint, bool &fftw_fail)
{
  s_failed_dependencies = "";
  std::string sTemp = "";

  platform::BinaryInfo bi;
  if (!platform::InspectBinary(s_dll, bi))
    return;

  s_dependencies = s_dll + ":\n";
  std::string sDepDLL = "";

  for (const std::string &sImport : bi.imports)
  {
    const char *szDepName = sImport.c_str();
    sTemp = utils.StrFormat("%s", szDepName);
    utils.StrToLC(sTemp);
    utils.StrTrim(sTemp);

    sDepDLL = utils.StrFormat("  %s\n", szDepName);
    s_dependencies += "  " + sDepDLL;

    if ((sTemp == "msvcm80.dll") || (sTemp == "msvcp80.dll") || (sTemp == "msvcr80.dll"))
    {
      if (!platform::IsVCRuntimeInstalled("2005", bIs64BitAVSDLL))
        s_failed_dependencies += sDepDLL;

      continue;
    }

    if ((sTemp == "msvcm90.dll") || (sTemp == "msvcp90.dll") || (sTemp == "msvcr90.dll"))
    {
      if (!platform::IsVCRuntimeInstalled("2008", bIs64BitAVSDLL))
        s_failed_dependencies += sDepDLL;

      continue;
    }

    platform::LibHandle hDLL = platform::LoadLib(szDepName);
    if (!hDLL)
      s_failed_dependencies += sDepDLL;
    else
      platform::FreeLib(hDLL);
  }

  if (!CheckFFTW(s_dll, s_failed_dependencies))
    fftw_fail = true;
  else
    fftw_fail = false;

  s_failed_dependencies.erase(s_failed_dependencies.find_last_not_of("\n") + 1);
  sTemp = s_failed_dependencies;
  utils.StrToLC(sTemp);

  s_hint = "";
  if ((sTemp.find("msvcm80.dll") != std::string::npos) ||
      (sTemp.find("msvcp80.dll") != std::string::npos) ||
      (sTemp.find("msvcr80.dll") != std::string::npos) ||
      (sTemp.find("vcomp.dll") != std::string::npos))
        s_hint += "Visual Studio 2005 Runtime doesn't seem to be installed\n";

  if ((sTemp.find("msvcm90.dll") != std::string::npos) ||
      (sTemp.find("msvcp90.dll") != std::string::npos) ||
      (sTemp.find("msvcr90.dll") != std::string::npos) ||
      (sTemp.find("vcomp90.dll") != std::string::npos))
        s_hint += "Visual Studio 2008 Runtime doesn't seem to be installed\n";

  if ((sTemp.find("msvcm100.dll") != std::string::npos) ||
      (sTemp.find("msvcp100.dll") != std::string::npos) ||
      (sTemp.find("msvcr100.dll") != std::string::npos) ||
      (sTemp.find("vcomp100.dll") != std::string::npos))
        s_hint += "Visual Studio 2010 Runtime doesn't seem to be installed\n";

  if ((sTemp.find("msvcm110.dll") != std::string::npos) ||
      (sTemp.find("msvcp110.dll") != std::string::npos) ||
      (sTemp.find("msvcr110.dll") != std::string::npos) ||
      (sTemp.find("vcomp110.dll") != std::string::npos))
        s_hint += "Visual Studio 2012 Runtime doesn't seem to be installed\n";

  if ((sTemp.find("msvcm120.dll") != std::string::npos) ||
      (sTemp.find("msvcp120.dll") != std::string::npos) ||
      (sTemp.find("msvcr120.dll") != std::string::npos) ||
      (sTemp.find("vcomp120.dll") != std::string::npos))
        s_hint += "Visual Studio 2013 Runtime doesn't seem to be installed\n";

  if ((sTemp.find("msvcm140.dll") != std::string::npos) ||
      (sTemp.find("msvcp140.dll") != std::string::npos) ||
      (sTemp.find("msvcr140.dll") != std::string::npos) ||
      (sTemp.find("vcomp140.dll") != std::string::npos) ||
      (sTemp.find("vcruntime140.dll") != std::string::npos) ||
      (sTemp.find("api-ms-win-crt") != std::string::npos))
        s_hint += "Visual Studio 2015/2017/2019 Runtime doesn't seem to be installed\n";

  if (fftw_fail)
  {
#ifndef _WIN32
    if ((sTemp.find("libfftw3f_threads.so.3") != std::string::npos) || (sTemp.find("libfftw3f.so.3") != std::string::npos))
      s_hint += "The single precision FFTW library doesn't seem to be installed\n      (Debian/Ubuntu: libfftw3-single3, Fedora: fftw-libs-single, Arch: fftw)\n";
#endif
    if (sTemp.find("libfftw3f-3.dll") != std::string::npos)
      s_hint += "\'libfftw3f-3.dll\' can be downloaded here:\n      http://www.fftw.org/install/windows.html\n\n      \'libfftw3f-3.dll\' should be placed in \"System32\" and/or \"SysWoW64\"\n";

    if (sTemp.find("fftw3.dll") != std::string::npos)
      s_hint += "\'fftw3.dll\' (aka \'libfftw3f-3.dll\') can be downloaded here:\n      http://www.fftw.org/install/windows.html\n\n      \'fftw3.dll\' should be placed in \"System32\" and/or \"SysWoW64\"\n";
  }

  s_hint.erase(s_hint.find_last_not_of("\n") + 1);

  return;
}


bool CAvisynthInfo::CheckFFTW(std::string s_dll, std::string &s_failed_deps)
{
  int64_t fsize = platform::FileSize(s_dll);
  bool bSuccess = true;

  if ((fsize < (20 * 1024 * 1024)) && (fsize > 1024 ))
  {
    std::ifstream hDLLFile(platform::PathFromUtf8(s_dll), std::ios::binary);
    if (hDLLFile.is_open())
    {
      std::string filedata((std::istreambuf_iterator<char>(hDLLFile)), std::istreambuf_iterator<char>());
      hDLLFile.close();

      std::transform(filedata.begin(), filedata.end(), filedata.begin(), [](unsigned char c) { return (char)tolower(c); });

      //FFTW is often loaded at runtime, so it is not always an import: look for the name
#ifdef _WIN32
      static const char *const pszFFTWNames[] = { "libfftw3f-3.dll", "fftw3.dll" };
#else
      static const char *const pszFFTWNames[] = { "libfftw3f_threads.so.3", "libfftw3f.so.3" }; //fft3dfilter/mvtools, dfttest
#endif
      for (const char *pszName : pszFFTWNames)
      {
        if (filedata.find(pszName) == std::string::npos)
          continue;

        platform::LibHandle hDLL = platform::LoadLib(pszName);
        if (!hDLL)
        {
          std::string sEntry = utils.StrFormat("  %s\n", pszName);
          if (s_failed_deps.find(sEntry) == std::string::npos)
            s_failed_deps += sEntry;
          bSuccess = false;
        }
        else
          platform::FreeLib(hDLL);

        return bSuccess;
      }
    }
  }

  return bSuccess;
}


bool CAvisynthInfo::IsGruntFunc(std::string s_function)
{
  utils.StrToLC(s_function);

  if (s_function == "averagechromau") return true;
  if (s_function == "averagechromav") return true;
  if (s_function == "averageluma") return true;
  if (s_function == "bindstr") return true;
  if (s_function == "chromaudifference") return true;
  if (s_function == "chromavdifference") return true;
  if (s_function == "conditionalfilter") return true;
  if (s_function == "frameevaluate") return true;
  if (s_function == "gconditionalfilter") return true;
  if (s_function == "gframeevaluate") return true;
  if (s_function == "grtconfig") return true;
  if (s_function == "gscriptclip") return true;
  if (s_function == "gwritefile") return true;
  if (s_function == "gwritefileif") return true;
  if (s_function == "lumadifference") return true;
  if (s_function == "rgbdifference") return true;
  if (s_function == "rgbdifferencefromprevious") return true;
  if (s_function == "rgbdifferencetonext") return true;
  if (s_function == "scriptclip") return true;
  if (s_function == "udifferencefromprevious") return true;
  if (s_function == "udifferencetonext") return true;
  if (s_function == "uplanemax") return true;
  if (s_function == "uplanemedian") return true;
  if (s_function == "uplanemin") return true;
  if (s_function == "uplaneminmaxdifference") return true;
  if (s_function == "vdifferencefromprevious") return true;
  if (s_function == "vdifferencetonext") return true;
  if (s_function == "vplanemax") return true;
  if (s_function == "vplanemedian") return true;
  if (s_function == "vplanemin") return true;
  if (s_function == "vplaneminmaxdifference") return true;
  if (s_function == "writefile") return true;
  if (s_function == "writefileif") return true;
  if (s_function == "ydifferencefromprevious") return true;
  if (s_function == "ydifferencetonext") return true;
  if (s_function == "yplanemax") return true;
  if (s_function == "yplanemedian") return true;
  if (s_function == "yplanemin") return true;
  if (s_function == "yplaneminmaxdifference") return true;

  return false;
}
