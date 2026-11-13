#if !defined(_AVISYNTHINFO_H)
#define _AVISYNTHINFO_H

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
#include "avs_headers\avisynth.h"

class CAvisynthInfo
{
public:
	CAvisynthInfo();
	string  sAVSDLL;
	BOOL    GetInfo(string &s_ErrorMsg);
	int     iInterfaceVersion;
	vector  <string> vPluginDirs;
	vector  <string> vInternalFunctions;
	vector  <string> vPluginFunctions;
	vector  <string> vPlugins;
	vector  <string> vPluginDependencies;
	vector  <string> vPluginErrors;
	string  sDLLPath;
	string  sLinkerVersion;
	string  sFileVersion;
	string  sVersionString;
	string  sVersionNumber;
	string  sTimeStamp;
	string  sTimeStampUTC;
	BOOL    bIs64BitAVS;
	BOOL    bLoadError;
	BOOL    bIsAVSPlus;
	BOOL    bIsMTVersion;	
	virtual ~CAvisynthInfo();

private:
	void           StrTokenize(string s_string, vector<string> &Tokens, string s_separator, BOOL b_removedups);
	BOOL           GetFileNameFromHandle(HANDLE hFile, string &sFileName);
	string         GetFileVersion(string s_file);
	string         GetTimeStamp(string s_file, BOOL bUTC);
	int            GetInterfaceVersion(string &s_ErrorMsg);
  void           EnumPluginDirs();
	void           EnumPluginDLLs();
	BOOL           SYSCheckPlugin(string s_dll, string &s_Msg, BOOL &b_Is64BitDLL);
	void           AVSCheckPlugins();
	void           GetDLLDependencies(string s_dll, string &s_dependencies, string &s_failed_dependencies, string &s_hint);
	BOOL           IsRuntimeInstalled(string s_version);
	__int64        FileSize(string s_file);
	inline string  StrFormat(char const *fmt, ...);
	void           StrTrim(string &s_string);
	void           StrToLC(string &s_string);
};


static BOOL CompareNoCase(string first, string second)
{
  unsigned int ui = 0;
  while ((ui < first.length()) && (ui < second.length()))
  {
    if (tolower (first[ui]) < tolower (second[ui]))
			return true;
    else if (tolower (first[ui]) > tolower (second[ui]))
			return false;
    ui++;
  }

  if (first.length() < second.length())
		return true;
  else
		return false;
}


#endif //_AVISYNTHINFO_H

