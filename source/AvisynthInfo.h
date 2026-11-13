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

#pragma once

#include "common.h"

#define BUFSIZE 2048
#define NUM_PLUG_DIR_REFS 8

struct StrCaseInsensitiveCompare
{ 
	bool operator() (const string& a, const string& b) const
	{
		return _stricmp(a.c_str(), b.c_str()) < 0;
	}
};


class CAvisynthInfo
{
public:
	CAvisynthInfo();
	string  sAVSDLL;
	BOOL    GetInfo(string &s_ErrorMsg);
	void    EnumPluginDLLs();
	void    StrTokenize(string s_string, set<string, StrCaseInsensitiveCompare> &Tokens);
	BOOL    bIsMTVersion;
	int     iInterfaceVersion;
	string  saPluginDirRefs[NUM_PLUG_DIR_REFS];
	set     <string, StrCaseInsensitiveCompare> mPluginDirs;
	set     <string, StrCaseInsensitiveCompare> mPluginFunctions;
	set     <string, StrCaseInsensitiveCompare> mPluginDLLsC25;
	set     <string, StrCaseInsensitiveCompare> mPluginDLLsC20;
	set     <string, StrCaseInsensitiveCompare> mPluginDLLsCPP26;
	set     <string, StrCaseInsensitiveCompare> mPluginDLLsCPP25;
	set     <string, StrCaseInsensitiveCompare> mPluginDLLsCPP20;
	set     <string> mPluginErrors;
	BYTE    NumPlugDirRefs;
	string  sDLLPath;
	string  sLinkerVersion;
	string  sVersionNumber;
	string  sVersionString;
	string  sTimeStamp;
	BOOL    bIs64BitVersion;
	BOOL    bLoadError;
	virtual ~CAvisynthInfo();

private:
	BOOL    GetFileNameFromHandle(HANDLE hFile, string &sFileName);
	string  GetFileVersion(string s_file);
	string  GetTimeStamp(string s_file);
	int     GetInterfaceVersion(string &s_ErrorMsg);
  void    EnumPluginDirs();
	BOOL    SYSCheckPlugin(string s_dll, string &s_Msg);
	void    AVSCheckPlugins();
	void    GetDLLDependencies(string s_dll, string &s_failed_dependencies, string &s_hint);
	void    FindStrings(string s_dll, string &s_failed_deps);
	__int64 FileSize(string s_file);
	string  StrFormat(const char *fmt, ...);
	void    StrTrim(string &s_string);
	void    StrToLC(string &s_string);
};


CAvisynthInfo::CAvisynthInfo()
{
	NumPlugDirRefs = NUM_PLUG_DIR_REFS;
	sAVSDLL = "";
}

CAvisynthInfo::~CAvisynthInfo()
{
}


int CAvisynthInfo::GetInterfaceVersion(string &s_ErrorMsg)
{
	int iInterfaceVersion = 0;
	s_ErrorMsg = "";
	string sSysError = "";

	HINSTANCE hDLL;
	if (sAVSDLL == "")
		hDLL = ::LoadLibrary("avisynth");
	else
		hDLL = ::LoadLibrary(sAVSDLL.c_str());

	if (!hDLL)
	{
		sSysError = SysErrorMessage();
		if (sSysError != "")
			s_ErrorMsg = StrFormat("Failed to load avisynth.dll. OS returned:\n%s", sSysError.c_str());
		else
			s_ErrorMsg = "Failed to load avisynth.dll.";

		string sFailedDependencies = "";
		string sHint = "";
		GetDLLDependencies(sDLLPath, sFailedDependencies, sHint);
		if (sFailedDependencies != "")
		{
			s_ErrorMsg += StrFormat("\n\nPotentially missing dependencies:\n%s", sFailedDependencies.c_str());
			if (sHint != "")
				s_ErrorMsg += StrFormat("\n\nNote: \"%s\"\n      is probably required and not installed.", sHint.c_str());
		}

		return 0;
	}

	try
	{
		_set_se_translator(SE_Translator);
		IScriptEnvironment *(__stdcall *CreateEnvironment)(int) = (IScriptEnvironment *(__stdcall *)(int))::GetProcAddress(hDLL, "CreateScriptEnvironment");
		if (!CreateEnvironment)
		{
			s_ErrorMsg = "Failed to load CreateScriptEnvironment()";
			::FreeLibrary(hDLL);
			return 0;
		}

		iInterfaceVersion = 5; //try the lowest version we want to support
		IScriptEnvironment *AVS_env = CreateEnvironment(iInterfaceVersion);
		if (!AVS_env)
		{
			s_ErrorMsg = "Could not create IScriptenvironment (AVISYNTH_INTERFACE_VERSION = 5)";
			::FreeLibrary(hDLL);
			return 0;
		}

		try
		{
			AVS_env->CheckVersion(6);
			iInterfaceVersion = 6;
			AVS_env->CheckVersion(7); //who knows...
			iInterfaceVersion = 7;
		}
		catch (AvisynthError)
		{
		}

		Sleep(50);
		AVS_env->DeleteScriptEnvironment();
		AVS_env = 0;
	}
	catch (AvisynthError err)
	{
		iInterfaceVersion = 0;
		s_ErrorMsg = StrFormat("%s", (PCSTR)err.msg);
	}
	catch (SE_Exception ep)
	{
		iInterfaceVersion = 0;
		s_ErrorMsg = SE_ErrorMessage(ep.GetCode());
	}
	catch (...)
	{
		iInterfaceVersion = 0;
		s_ErrorMsg = SysErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot load avisynth.dll\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot load avisynth.dll";
	}

	if (!::FreeLibrary(hDLL))
	{
		iInterfaceVersion = 0;
		s_ErrorMsg = SysErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot unload avisynth.dll\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot unload avisynth.dll";
	}
	Sleep(50);

	return iInterfaceVersion;
}


BOOL CAvisynthInfo::GetInfo(string &s_ErrorMsg)
{
	sDLLPath = "";
	bIs64BitVersion = FALSE;
	bIsMTVersion = FALSE;
	iInterfaceVersion = 0;
	sVersionNumber = "";
	sVersionString = "Unknown Avisynth Version";
	BOOL bAVSLinkage = FALSE;
	bLoadError = FALSE;
	BOOL bRet = TRUE;
	s_ErrorMsg = "";
	string sTempDLLPath = "";

	LOADED_IMAGE li;
	BOOL bLoaded;

	if (sAVSDLL == "")
		bLoaded = MapAndLoad("avisynth", NULL, &li,	TRUE,	TRUE);
	else
		bLoaded = MapAndLoad((LPSTR)sAVSDLL.c_str(), NULL, &li,	TRUE,	TRUE);
	
	if (!bLoaded)
	{
		bLoadError = TRUE;
		s_ErrorMsg = SysErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot load avisynth.dll\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot load avisynth.dll";

		return FALSE;
	}

	try
	{
		_set_se_translator(SE_Translator);
		sTempDLLPath = StrFormat("%s", li.ModuleName);
		if ((li.FileHeader->FileHeader.Machine == IMAGE_FILE_MACHINE_IA64) || (li.FileHeader->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64))
		{
			bIs64BitVersion = TRUE;
			#ifndef AVS64
				UnMapAndLoad(&li);
				s_ErrorMsg = "Cannot load avisynth.dll";
				return FALSE;
			#endif
		}
		else //32 Bit Avisynth DLL
		{
			#ifdef AVS64
				UnMapAndLoad(&li);
				s_ErrorMsg = "Cannot load avisynth.dll";
				return FALSE;
			#endif
		}

		sLinkerVersion = StrFormat("%u.%u", li.FileHeader->OptionalHeader.MajorLinkerVersion, li.FileHeader->OptionalHeader.MinorLinkerVersion);

		DWORD expVA = li.FileHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
		PIMAGE_EXPORT_DIRECTORY pExp = (PIMAGE_EXPORT_DIRECTORY)ImageRvaToVa(li.FileHeader, li.MappedAddress, expVA, NULL);
		DWORD rvaNames = pExp->AddressOfNames;
		DWORD *prvaNames = (DWORD*)ImageRvaToVa(li.FileHeader, li.MappedAddress, rvaNames, NULL);
		string sName = "";
		size_t pos = 0;

		for (DWORD dwName = 0; dwName < pExp->NumberOfNames; ++dwName)
		{
			DWORD rvaName = prvaNames[dwName];
			sName = (char *)ImageRvaToVa(li.FileHeader, li.MappedAddress, rvaName, NULL);

			for (unsigned int i = 0; i < sName.length(); i++)
				sName[i] = tolower(sName[i]);

			pos = sName.find("avs_linkage");
			if (pos != std::string::npos)
			{
				bAVSLinkage = TRUE;
				break;
			}
		}
	}
	catch (SE_Exception ep)
	{
		bRet = FALSE;
		s_ErrorMsg = SE_ErrorMessage(ep.GetCode());
	}
	catch (...)
	{
		bRet = FALSE;
		s_ErrorMsg = SysErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot load avisynth.dll\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot load avisynth.dll";
	}

	if (!UnMapAndLoad(&li))
	{
		bRet = FALSE;
		s_ErrorMsg = SysErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot unload avisynth.dll\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot unload avisynth.dll";
	}

	if (!bRet)
		return FALSE;


	//This seems to be the only reliable way to determine the real path of avisynth.dll (System32/SysWoW64)
	//across OS versions without having the file system redirector interfere
	try
	{
		_set_se_translator(SE_Translator);
		HANDLE hFile = CreateFile(sTempDLLPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
		if (hFile != INVALID_HANDLE_VALUE)
		{
			if (!GetFileNameFromHandle(hFile, sDLLPath))
				sDLLPath = sTempDLLPath;
		}

		CloseHandle(hFile);
	}
	catch (SE_Exception ep)
	{
		bRet = FALSE;
		s_ErrorMsg = SE_ErrorMessage(ep.GetCode());
	}
	catch (...)
	{
		bRet = FALSE;
		s_ErrorMsg = SysErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot open avisynth.dll\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot open avisynth.dll";
	}

	if (!bRet)
		return FALSE;


	sVersionNumber = GetFileVersion(sDLLPath);

	if (!bAVSLinkage)
	{
		s_ErrorMsg = StrFormat("Avisynth version is too old (%s).\nPlease update to Avisynth 2.6 or Avisynth+.", sVersionNumber.c_str());
		return FALSE;
	}


	sTimeStamp = GetTimeStamp(sDLLPath);

	iInterfaceVersion = GetInterfaceVersion(s_ErrorMsg);
	if (iInterfaceVersion == 0)
		return FALSE;

	HINSTANCE hDLL;
	if (sAVSDLL == "")
		hDLL = ::LoadLibrary("avisynth");
	else
		hDLL = ::LoadLibrary(sAVSDLL.c_str());

	if (!hDLL)
	{
		s_ErrorMsg = SysErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot load avisynth.dll\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot load avisynth.dll";

		return FALSE;
	}

	try
	{
		_set_se_translator(SE_Translator);
		IScriptEnvironment *(__stdcall *CreateEnvironment)(int) = (IScriptEnvironment *(__stdcall *)(int))::GetProcAddress(hDLL, "CreateScriptEnvironment");
		if (!CreateEnvironment)
		{
			s_ErrorMsg = "Failed to load CreateScriptEnvironment()";
			::FreeLibrary(hDLL);
			return FALSE;
		}

		IScriptEnvironment *AVS_env = CreateEnvironment(iInterfaceVersion);
		if (!AVS_env)
		{
			s_ErrorMsg = "Could not create IScriptenvironment";
			::FreeLibrary(hDLL);
			return FALSE;
		}

		AVS_linkage = AVS_env->GetAVSLinkage();
		AVSValue foo;

		try
		{
			sVersionString = StrFormat("%s", AVS_env->Invoke("VersionString", AVSValue(&foo, 0)).AsString());
		}
		catch (IScriptEnvironment::NotFound)
		{
			sVersionString = "Unknown Avisynth Version";
		}

		if ((AVS_env->FunctionExists("SetMTMode")) || (AVS_env->FunctionExists("Prefetch")))
			bIsMTVersion = TRUE;

		string sPluginFunctions = "";
		try
		{
			foo = AVS_env->GetVar("$PluginFunctions$");
			sPluginFunctions = foo.AsString();
			StrTokenize(sPluginFunctions, mPluginFunctions);
		}
		catch(...)
		{
		}

		foo = 0;
		Sleep(50);
		AVS_env->DeleteScriptEnvironment();
		AVS_env = 0;
	}
	catch (AvisynthError err)
	{
		bRet = FALSE;
		s_ErrorMsg = StrFormat("%s", (PCSTR)err.msg);
	}
	catch (SE_Exception ep)
	{
		bRet = FALSE;
		s_ErrorMsg = SE_ErrorMessage(ep.GetCode());
	}
	catch (...)
	{
		bRet = FALSE;
		s_ErrorMsg = SysErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot load avisynth.dll\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot load avisynth.dll";
	}

	AVS_linkage = 0;
	if (!::FreeLibrary(hDLL))
	{
		bRet = FALSE;
		s_ErrorMsg = SysErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot unload avisynth.dll\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot unload avisynth.dll";
	}
	Sleep(50);

	EnumPluginDirs();

	return bRet;
}


void CAvisynthInfo::EnumPluginDirs()
{
	for (unsigned int n = 0; n < NUM_PLUG_DIR_REFS; n++)
		saPluginDirRefs[n] = "";

	//- PluginDir+ in Software/Avisynth in HKEY_CURRENT_USER
	//- PluginDir+ in Software/Avisynth in HKEY_LOCAL_MACHINE
	//- PluginDir2_5 in Software/Avisynth in HKEY_CURRENT_USER
	//- PluginDir2_5 in Software/Avisynth in HKEY_LOCAL_MACHINE

	set <string> mDuplicates;
	LONG lRes;
	HKEY hKeyResult = 0;

	if (!bIs64BitVersion)
	{
		hKeyResult = 0;
		lRes = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Avisynth", 0, KEY_READ | KEY_WOW64_32KEY, &hKeyResult);
		if (lRes == ERROR_SUCCESS)
		{
			char szPlugDir[2050] = "";
			DWORD dwType = REG_SZ;
			DWORD dwBytes = 2048;
			if (RegQueryValueEx(hKeyResult, "PluginDir+", NULL, &dwType, (LPBYTE)szPlugDir, &dwBytes) == 0)
			{
				string sTemp(szPlugDir);
				StrTrim(sTemp);
				string sTempLower = sTemp;
				StrToLC(sTempLower);

				if (sTemp != "")
				{
					saPluginDirRefs[0] = "PluginDir+    (HKCU, x86):\t" + sTemp;

					if (mDuplicates.find(sTempLower) == mDuplicates.end())
					{
						mDuplicates.insert(sTempLower);
						mPluginDirs.insert(sTemp);
					}
				}
			}

			RegCloseKey(hKeyResult);
		}
	
		hKeyResult = 0;
		lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Avisynth", 0, KEY_READ | KEY_WOW64_32KEY, &hKeyResult);
		if (lRes == ERROR_SUCCESS)
		{
			char szPlugDir[2050] = "";
			DWORD dwType = REG_SZ;
			DWORD dwBytes = 2048;
			if (RegQueryValueEx(hKeyResult, "PluginDir+", NULL, &dwType, (LPBYTE)szPlugDir, &dwBytes) == 0)
			{
				string sTemp(szPlugDir);
				StrTrim(sTemp);
				string sTempLower = sTemp;
				StrToLC(sTempLower);

				if (sTemp != "")
				{
					saPluginDirRefs[1] = "PluginDir+    (HKLM, x86):\t" + sTemp;

					if (mDuplicates.find(sTempLower) == mDuplicates.end())
					{
						mDuplicates.insert(sTempLower);
						mPluginDirs.insert(sTemp);
					}
				}
			}

			RegCloseKey(hKeyResult);
		}
	
		hKeyResult = 0;
		lRes = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Avisynth", 0, KEY_READ | KEY_WOW64_32KEY, &hKeyResult);
		if (lRes == ERROR_SUCCESS)
		{
			char szPlugDir[2050] = "";
			DWORD dwType = REG_SZ;
			DWORD dwBytes = 2048;
			if (RegQueryValueEx(hKeyResult, "PluginDir2_5", NULL, &dwType, (LPBYTE)szPlugDir, &dwBytes) == 0)
			{
				string sTemp(szPlugDir);
				StrTrim(sTemp);
				string sTempLower = sTemp;
				StrToLC(sTempLower);

				if (sTemp != "")
				{
					saPluginDirRefs[2] = "PluginDir2_5  (HKCU, x86):\t" + sTemp;

					if (mDuplicates.find(sTempLower) == mDuplicates.end())
					{
						mDuplicates.insert(sTempLower);
						mPluginDirs.insert(sTemp);
					}
				}
			}

			RegCloseKey(hKeyResult);
		}
	
		hKeyResult = 0;
		lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Avisynth", 0, KEY_READ | KEY_WOW64_32KEY, &hKeyResult);
		if (lRes == ERROR_SUCCESS)
		{
			char szPlugDir[2050] = "";
			DWORD dwType = REG_SZ;
			DWORD dwBytes = 2048;
			if (RegQueryValueEx(hKeyResult, "PluginDir2_5", NULL, &dwType, (LPBYTE)szPlugDir, &dwBytes) == 0)
			{
				string sTemp(szPlugDir);
				StrTrim(sTemp);
				string sTempLower = sTemp;
				StrToLC(sTempLower);

				if (sTemp != "")
				{
					saPluginDirRefs[3] = "PluginDir2_5  (HKLM, x86):\t" + sTemp;

					if (mDuplicates.find(sTempLower) == mDuplicates.end())
					{
						mDuplicates.insert(sTempLower);
						mPluginDirs.insert(sTemp);
					}
				}
			}

			RegCloseKey(hKeyResult);
		}
	}	
	else //64 Bit
	{
		hKeyResult = 0;
		lRes = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Avisynth", 0, KEY_READ | KEY_WOW64_64KEY, &hKeyResult);
		if (lRes == ERROR_SUCCESS)
		{
			char szPlugDir[2050] = "";
			DWORD dwType = REG_SZ;
			DWORD dwBytes = 2048;
			if (RegQueryValueEx(hKeyResult, "PluginDir+", NULL, &dwType, (LPBYTE)szPlugDir, &dwBytes) == 0)
			{
				string sTemp(szPlugDir);
				StrTrim(sTemp);
				string sTempLower = sTemp;
				StrToLC(sTempLower);

				if (sTemp != "")
				{
					saPluginDirRefs[4] = "PluginDir+    (HKCU, x64):\t" + sTemp;

					if (mDuplicates.find(sTempLower) == mDuplicates.end())
					{
						mDuplicates.insert(sTempLower);
						mPluginDirs.insert(sTemp);
					}
				}
			}

			RegCloseKey(hKeyResult);
		}
	
		hKeyResult = 0;
		lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Avisynth", 0, KEY_READ | KEY_WOW64_64KEY, &hKeyResult);
		if (lRes == ERROR_SUCCESS)
		{
			char szPlugDir[2050] = "";
			DWORD dwType = REG_SZ;
			DWORD dwBytes = 2048;
			if (RegQueryValueEx(hKeyResult, "PluginDir+", NULL, &dwType, (LPBYTE)szPlugDir, &dwBytes) == 0)
			{
				string sTemp(szPlugDir);
				StrTrim(sTemp);
				string sTempLower = sTemp;
				StrToLC(sTempLower);

				if (sTemp != "")
				{
					saPluginDirRefs[5] = "PluginDir+    (HKLM, x64):\t" + sTemp;

					if (mDuplicates.find(sTempLower) == mDuplicates.end())
					{
						mDuplicates.insert(sTempLower);
						mPluginDirs.insert(sTemp);
					}
				}
			}

			RegCloseKey(hKeyResult);
		}

		hKeyResult = 0;
		lRes = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Avisynth", 0, KEY_READ | KEY_WOW64_64KEY, &hKeyResult);
		if (lRes == ERROR_SUCCESS)
		{
			char szPlugDir[2050] = "";
			DWORD dwType = REG_SZ;
			DWORD dwBytes = 2048;
			if (RegQueryValueEx(hKeyResult, "PluginDir2_5", NULL, &dwType, (LPBYTE)szPlugDir, &dwBytes) == 0)
			{
				string sTemp(szPlugDir);
				StrTrim(sTemp);
				string sTempLower = sTemp;
				StrToLC(sTempLower);

				if (sTemp != "")
				{
					saPluginDirRefs[6] = "PluginDir2_5  (HKCU, x64):\t" + sTemp;

					if (mDuplicates.find(sTempLower) == mDuplicates.end())
					{
						mDuplicates.insert(sTempLower);
						mPluginDirs.insert(sTemp);
					}
				}
			}

			RegCloseKey(hKeyResult);
		}
	
		hKeyResult = 0;
		lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Avisynth", 0, KEY_READ | KEY_WOW64_64KEY, &hKeyResult);
		if (lRes == ERROR_SUCCESS)
		{
			char szPlugDir[2050] = "";
			DWORD dwType = REG_SZ;
			DWORD dwBytes = 2048;
			if (RegQueryValueEx(hKeyResult, "PluginDir2_5", NULL, &dwType, (LPBYTE)szPlugDir, &dwBytes) == 0)
			{
				string sTemp(szPlugDir);
				StrTrim(sTemp);
				string sTempLower = sTemp;
				StrToLC(sTempLower);

				if (sTemp != "")
				{
					saPluginDirRefs[7] = "PluginDir2_5  (HKLM, x64):\t" + sTemp;

					if (mDuplicates.find(sTempLower) == mDuplicates.end())
					{
						mDuplicates.insert(sTempLower);
						mPluginDirs.insert(sTemp);
					}
				}
			}

			RegCloseKey(hKeyResult);
		}
	}

	return;
}


BOOL CAvisynthInfo::GetFileNameFromHandle(HANDLE hFile, string &sFileName)
{
	BOOL bSuccess = FALSE;
	char pszFilename[MAX_PATH + 1];
	HANDLE hFileMap;

	hFileMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 1, NULL);

	if (hFileMap) 
	{
		void* pMem = MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 1);
		if (pMem) 
		{
			if (GetMappedFileName (GetCurrentProcess(), pMem, pszFilename, MAX_PATH))
			{
				char szTemp[BUFSIZE];
				szTemp[0] = '\0';
				if (GetLogicalDriveStrings(BUFSIZE - 1, szTemp))
				{
					char szName[MAX_PATH];
					char szDrive[3] = TEXT(" :");
					BOOL bFound = FALSE;
					char* p = szTemp;

					do 
					{
						*szDrive = *p;
						if (QueryDosDevice(szDrive, szName, MAX_PATH))
						{
							size_t uNameLen = strlen(szName);
							if (uNameLen < MAX_PATH)
							{
								bFound = _strnicmp(pszFilename, szName, uNameLen) == 0 && *(pszFilename + uNameLen) == '\\';
								if (bFound)
								{
									sFileName = StrFormat("%s%s", szDrive, pszFilename+uNameLen);
								}
							}
						}

						while (*p++);
					} while (!bFound && *p); // end of string
				}
			}
			bSuccess = TRUE;
			UnmapViewOfFile(pMem);
		} 
		CloseHandle(hFileMap);
	}
	else
	  return FALSE;

  return (bSuccess);
}


string CAvisynthInfo::GetFileVersion(string s_file)
{
	string sFileVersion = "0.0.0.0";
	char szFilename[MAX_PATH + 1] = {0};

	if (s_file == "")
	{
		if (GetModuleFileName(NULL, szFilename, MAX_PATH) == 0)
			return sFileVersion;
	}
	else
		sprintf(szFilename, "%s", s_file.c_str());

	DWORD dummy;
	DWORD dwSize = GetFileVersionInfoSize(szFilename, &dummy);
	if (dwSize == 0)
		return sFileVersion;

	BYTE* VersionInfoBuffer = new BYTE[dwSize];
	if (!GetFileVersionInfo(szFilename, 0, dwSize, (VOID*)VersionInfoBuffer))
	{
		delete [] VersionInfoBuffer;
		return sFileVersion;
	}

	UINT uiLen;
	VS_FIXEDFILEINFO* pFixedInfo = 0;
	if (!VerQueryValue(VersionInfoBuffer, "\\", (LPVOID*)(&pFixedInfo), &uiLen))
	{
		delete [] VersionInfoBuffer;
		return sFileVersion;
	}

	sFileVersion = StrFormat("%u.%u.%u.%u", HIWORD(pFixedInfo->dwFileVersionMS), LOWORD(pFixedInfo->dwFileVersionMS), HIWORD(pFixedInfo->dwFileVersionLS), LOWORD(pFixedInfo->dwFileVersionLS));
	delete [] VersionInfoBuffer;

	return sFileVersion;
}


string CAvisynthInfo::GetTimeStamp(string s_file)
{
	HANDLE hFile = CreateFile(s_file.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return "Cannot determine timestamp";

	FILETIME ftCreate, ftAccess, ftWrite, ftLocal;
	SYSTEMTIME fTime;

	if (!GetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite))
	{
		CloseHandle(hFile);
		return "Cannot determine timestamp";
	}

	if (!FileTimeToLocalFileTime(&ftWrite, &ftLocal))
	{
		CloseHandle(hFile);
		return "Cannot determine timestamp";
	}

	if (!FileTimeToSystemTime(&ftLocal, &fTime))
	{
		CloseHandle(hFile);
		return "Cannot determine timestamp";
	}

	CloseHandle(hFile);
	
	return StrFormat("%04u-%02u-%02u, %02u:%02u:%02u", fTime.wYear, fTime.wMonth, fTime.wDay, fTime.wHour, fTime.wMinute, fTime.wSecond);
}


string CAvisynthInfo::StrFormat(const char *fmt, ...)
{
	if (!fmt)	return "";

	unsigned int maxbufsize = 16384;
	unsigned int length = 256;
	char *buffer = NULL;
	int result = -1;

	va_list args;
	va_start(args, fmt);
	while (result < 0)
	{
		if (buffer)	delete [] buffer;
		buffer = new char[length];
		memset(buffer, ' ', sizeof(buffer));
		result = _vsnprintf(buffer, length, fmt, args);
		length *= 2;
		if (length >= maxbufsize)
		{
			delete [] buffer;
			va_end(args);
			return "";
		}
	}
	va_end(args);

	string str(buffer);
	delete [] buffer;

	return str;
}


void CAvisynthInfo::StrTrim(string &s_string)
{
	s_string.erase(s_string.find_last_not_of(" \t\n\r") + 1);
	s_string.erase(0, s_string.find_first_not_of(" \t\n\r"));

	return;
}


void CAvisynthInfo::StrToLC(string &s_string)
{
	for (unsigned int i = 0; i < s_string.length(); i++)
		s_string[i] = tolower(s_string[i]);

	return;
}


void CAvisynthInfo::EnumPluginDLLs()
{
	string sDir = "";
	string sMessage = "";

	for (set<string, StrCaseInsensitiveCompare>::iterator it = mPluginDirs.begin(); it != mPluginDirs.end(); ++it)
	{
		sDir = *it;

		WIN32_FIND_DATA fd;
		HANDLE hFind;
		string sFind =  sDir + "\\*.dll";
		BOOL bRet = TRUE;
		string sCurrentFile = "";
		hFind = FindFirstFile (sFind.c_str(), &fd);
		while (hFind != INVALID_HANDLE_VALUE && bRet)
		{
			if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				sCurrentFile = sDir + "\\" + fd.cFileName;
				if (!SYSCheckPlugin(sCurrentFile, sMessage))
				{
					if (sMessage != "")
						mPluginErrors.insert(sMessage);
				}
				else
				{
					if (sMessage == "AVSC25")
						mPluginDLLsC25.insert(sCurrentFile);
					if (sMessage == "AVSC20")
						mPluginDLLsC20.insert(sCurrentFile);
					if (sMessage == "AVS26CPP")
						mPluginDLLsCPP26.insert(sCurrentFile);
					if (sMessage == "AVS25CPP")
						mPluginDLLsCPP25.insert(sCurrentFile);
					if (sMessage == "AVS20CPP")
						mPluginDLLsCPP20.insert(sCurrentFile);
				}
			}
			bRet = FindNextFile(hFind, &fd);
		}

		FindClose (hFind);
	}

	AVSCheckPlugins();

	return;
}


BOOL CAvisynthInfo::SYSCheckPlugin(string s_dll, string &s_Msg)
{
	BOOL bRet = TRUE;
	s_Msg = "";
	BOOL bIs64BitDLL = FALSE;
	BOOL bIsPlugin = FALSE;

	LOADED_IMAGE li;
	BOOL bLoaded = MapAndLoad((LPSTR)s_dll.c_str(), NULL, &li,	TRUE,	TRUE);

	if (!bLoaded)
	{
		s_Msg = SysErrorMessage();
		if (s_Msg != "")
			s_Msg = StrFormat("Error loading \"%s\"\n%s", s_dll.c_str(), s_Msg.c_str());
		else
			s_Msg = StrFormat("Error loading \"%s\"", s_dll.c_str());

		return FALSE;
	}

	try
	{
		_set_se_translator(SE_Translator);

		if ((li.FileHeader->FileHeader.Machine == IMAGE_FILE_MACHINE_IA64) || (li.FileHeader->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64))
		{
			bIs64BitDLL = TRUE;
			#ifndef AVS64
				UnMapAndLoad(&li);
				s_Msg = StrFormat("Error loading \"%s\"\nCannot load 64 bit DLL with 32 bit Avisynth", s_dll.c_str());
				return FALSE;
			#endif
		}
		else
		{
			#ifdef AVS64
				UnMapAndLoad(&li);
				s_Msg = StrFormat("Error loading \"%s\"\nCannot load 32 bit DLL with 64 bit Avisynth", s_dll.c_str());
				return FALSE;
			#endif
		}

		DWORD expVA = li.FileHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
		PIMAGE_EXPORT_DIRECTORY pExp = (PIMAGE_EXPORT_DIRECTORY)ImageRvaToVa(li.FileHeader, li.MappedAddress, expVA, NULL);
		DWORD rvaNames = pExp->AddressOfNames;
		DWORD *prvaNames = (DWORD*)ImageRvaToVa(li.FileHeader, li.MappedAddress, rvaNames, NULL);
		string sName = "";
		size_t pos = 0;

		for (DWORD dwName = 0; dwName < pExp->NumberOfNames; ++dwName)
		{
			DWORD rvaName = prvaNames[dwName];
			sName = (char *)ImageRvaToVa(li.FileHeader, li.MappedAddress, rvaName, NULL);

			for (unsigned int i = 0; i < sName.length(); i++)
				sName[i] = tolower(sName[i]);

			pos = sName.find("avisynthplugininit3");
			if (pos != std::string::npos)
			{
				bIsPlugin = TRUE;
				s_Msg = "AVS26CPP";
				break;
			}

			pos = sName.find("avisynthplugininit2");
			if (pos != std::string::npos)
			{
				bIsPlugin = TRUE;
				s_Msg = "AVS25CPP";
				break;
			}

			pos = sName.find("avisynthplugininit");
			if (pos != std::string::npos)
			{
				bIsPlugin = TRUE;
				s_Msg = "AVS20CPP";
				break;
			}

			pos = sName.find("avisynth_c_plugin_init@4");
			if (pos != std::string::npos)
			{
				bIsPlugin = TRUE;
				s_Msg = "AVSC25";
				break;
			}
			pos = sName.find("avisynth_c_plugin_init");
			if (pos != std::string::npos)
			{
				bIsPlugin = TRUE;
				s_Msg = "AVSC20";
				break;
			}
		}

		if (!bIsPlugin)
		{
			UnMapAndLoad(&li);
			s_Msg = "";
			return FALSE;
		}
	}
	catch (SE_Exception ep)
	{
		bRet = FALSE;
		s_Msg = StrFormat("\"%s\"\n%s", s_dll.c_str(), SE_ErrorMessage(ep.GetCode()).c_str());
	}
	catch (...)
	{
		bRet = FALSE;

		s_Msg = SysErrorMessage();
		if (s_Msg != "")
			s_Msg = StrFormat("\"%s\"\n%s", s_dll.c_str(), s_Msg.c_str());
		else
			s_Msg = StrFormat("Unknown exception:\n\"%s\"", s_dll.c_str());
	}

	UnMapAndLoad(&li);

	return bRet;
}


void CAvisynthInfo::AVSCheckPlugins()
{
	string sPlugin = "";
	string sFailedDependencies = "";
	string sHint = "";
	string sMsg = "";

	HINSTANCE hDLL;
	if (sAVSDLL == "")
		hDLL = ::LoadLibrary("avisynth");
	else
		hDLL = ::LoadLibrary(sAVSDLL.c_str());

	if (!hDLL)
		return;

	IScriptEnvironment *(__stdcall *CreateEnvironment)(int) = (IScriptEnvironment *(__stdcall *)(int))::GetProcAddress(hDLL, "CreateScriptEnvironment");
	if (!CreateEnvironment)
	{
		::FreeLibrary(hDLL);
		return;
	}

	IScriptEnvironment *AVS_env = CreateEnvironment(iInterfaceVersion);
	if (!AVS_env)
	{
		::FreeLibrary(hDLL);
		return;
	}

	AVS_linkage = AVS_env->GetAVSLinkage();
	AVSValue AVS_main;
	BOOL bPlugError = FALSE;
	string sAVSError = "";

	for (set<string, StrCaseInsensitiveCompare>::iterator itCPP26 = mPluginDLLsCPP26.begin(); itCPP26 != mPluginDLLsCPP26.end(); ++itCPP26)
	{
		sPlugin = *itCPP26;

		bPlugError = FALSE;
		string s_Msg = "";
		try
		{
			AVS_main = AVS_env->Invoke("LoadPlugin", sPlugin.c_str());
		}
		catch (AvisynthError err)
		{
			sAVSError = StrFormat("%s", (PCSTR)err.msg);
			StrTrim(sAVSError);
			bPlugError = TRUE;
			s_Msg = StrFormat("%s", sPlugin.c_str(), sAVSError.c_str());
			StrTrim(s_Msg);
		}

		if (bPlugError)
		{
			sFailedDependencies = "";
			sHint = "";
			GetDLLDependencies(sPlugin, sFailedDependencies, sHint);
			if (sFailedDependencies != "")
			{
				s_Msg += StrFormat("\n\nPotentially missing dependencies:\n%s", sFailedDependencies.c_str());
				if (sHint != "")
					s_Msg += StrFormat("\n\nNote: \"%s\"\n      is probably required and not installed.", sHint.c_str());
			}
		}
		else
		{
			//Look for delay-load dependencies
			sFailedDependencies = "";
			FindStrings(sPlugin, sFailedDependencies);
			if (sFailedDependencies != "")
				s_Msg += StrFormat("\"%s\"\n\nPotentially missing dependencies:\n%s", sPlugin.c_str(), sFailedDependencies.c_str());
		}

		if (s_Msg != "")
			mPluginErrors.insert(s_Msg);
	}

	for (set<string, StrCaseInsensitiveCompare>::iterator itCPP25 = mPluginDLLsCPP25.begin(); itCPP25 != mPluginDLLsCPP25.end(); ++itCPP25)
	{
		sPlugin = *itCPP25;

		bPlugError = FALSE;
		string s_Msg = "";
		try
		{
			AVS_main = AVS_env->Invoke("LoadPlugin", sPlugin.c_str());
		}
		catch (AvisynthError err)
		{
			sAVSError = StrFormat("%s", (PCSTR)err.msg);
			StrTrim(sAVSError);
			bPlugError = TRUE;
			s_Msg = StrFormat("%s", sAVSError.c_str());
			StrTrim(s_Msg);
		}

		if (bPlugError)
		{
			sFailedDependencies = "";
			sHint = "";
			GetDLLDependencies(sPlugin, sFailedDependencies, sHint);
			if (sFailedDependencies != "")
			{
				s_Msg += StrFormat("\n\nPotentially missing dependencies:\n%s", sFailedDependencies.c_str());
				if (sHint != "")
					s_Msg += StrFormat("\n\nNote: \"%s\"\n      is probably required and not installed.", sHint.c_str());
			}
		}
		else
		{
			//Look for delay-load dependencies
			sFailedDependencies = "";
			FindStrings(sPlugin, sFailedDependencies);
			if (sFailedDependencies != "")
				s_Msg += StrFormat("\"%s\"\n\nPotentially missing dependencies:\n%s", sPlugin.c_str(), sFailedDependencies.c_str());
		}

		if (s_Msg != "")
			mPluginErrors.insert(s_Msg);
	}

	for (set<string, StrCaseInsensitiveCompare>::iterator itCPP20 = mPluginDLLsCPP20.begin(); itCPP20 != mPluginDLLsCPP20.end(); ++itCPP20)
	{
		sPlugin = *itCPP20;

		bPlugError = FALSE;
		string s_Msg = "";
		try
		{
			AVS_main = AVS_env->Invoke("LoadPlugin", sPlugin.c_str());
		}
		catch (AvisynthError err)
		{
			sAVSError = StrFormat("%s", (PCSTR)err.msg);
			StrTrim(sAVSError);
			bPlugError = TRUE;
			s_Msg = StrFormat("%s", sAVSError.c_str());
			StrTrim(s_Msg);
		}

		if (bPlugError)
		{
			sFailedDependencies = "";
			sHint = "";
			GetDLLDependencies(sPlugin, sFailedDependencies, sHint);
			if (sFailedDependencies != "")
			{
				s_Msg += StrFormat("\n\nPotentially missing dependencies:\n%s", sFailedDependencies.c_str());
				if (sHint != "")
					s_Msg += StrFormat("\n\nNote: \"%s\"\n      is probably required and not installed.", sHint.c_str());
			}
		}
		else
		{
			//Look for delay-load dependencies
			sFailedDependencies = "";
			FindStrings(sPlugin, sFailedDependencies);
			if (sFailedDependencies != "")
				s_Msg += StrFormat("\"%s\"\n\nPotentially missing dependencies:\n%s", sPlugin.c_str(), sFailedDependencies.c_str());
		}

		if (s_Msg != "")
			mPluginErrors.insert(s_Msg);
	}

	for (set<string, StrCaseInsensitiveCompare>::iterator itC25 = mPluginDLLsC25.begin(); itC25 != mPluginDLLsC25.end(); ++itC25)
	{
		sPlugin = *itC25;

		bPlugError = FALSE;
		string s_Msg = "";
		try
		{
			AVS_main = AVS_env->Invoke("LoadPlugin", sPlugin.c_str());
		}
		catch (AvisynthError err)
		{
			sAVSError = StrFormat("%s", (PCSTR)err.msg);
			StrTrim(sAVSError);
			bPlugError = TRUE;
			s_Msg = StrFormat("%s\n\nNote: C-Plugins must be loaded explicitly with \"LoadCPlugin()\"", sAVSError.c_str());
		}

		if (bPlugError)
		{
			sFailedDependencies = "";
			sHint = "";
			GetDLLDependencies(sPlugin, sFailedDependencies, sHint);
			if (sFailedDependencies != "")
			{
				s_Msg += StrFormat("\n\nPotentially missing dependencies:\n%s", sFailedDependencies.c_str());
				if (sHint != "")
					s_Msg += StrFormat("\n\nNote: \"%s\"\n      is probably required and not installed.", sHint.c_str());
			}
		}
		else
		{
			//Look for delay-load dependencies
			sFailedDependencies = "";
			FindStrings(sPlugin, sFailedDependencies);
			if (sFailedDependencies != "")
				s_Msg += StrFormat("\"%s\"\n\nPotentially missing dependencies:\n%s", sPlugin.c_str(), sFailedDependencies.c_str());
		}

		if (s_Msg != "")
			mPluginErrors.insert(s_Msg);
	}

	for (set<string, StrCaseInsensitiveCompare>::iterator itC20 = mPluginDLLsC20.begin(); itC20 != mPluginDLLsC20.end(); ++itC20)
	{
		sPlugin = *itC20;

		bPlugError = FALSE;
		string s_Msg = "";
		try
		{
			AVS_main = AVS_env->Invoke("LoadPlugin", sPlugin.c_str());
		}
		catch (AvisynthError err)
		{
			sAVSError = StrFormat("%s", (PCSTR)err.msg);
			StrTrim(sAVSError);
			bPlugError = TRUE;
			s_Msg = StrFormat("%s\n\nNote 1: C 2.0 Plugins are not supported by Avisynth+\nNote 2: For other Avisynth versions, C 2.0 Plugins must be loaded explicitly\n        with \"LoadCPlugin()\" and need \"avisynth_c.dll\" support DLL", sAVSError.c_str());
		}

		if (bPlugError)
		{
			sFailedDependencies = "";
			sHint = "";
			GetDLLDependencies(sPlugin, sFailedDependencies, sHint);
			if (sFailedDependencies != "")
			{
				s_Msg += StrFormat("\n\nPotentially missing dependencies:\n%s", sFailedDependencies.c_str());
				if (sHint != "")
					s_Msg += StrFormat("\n\nNote: \"%s\"\n      is probably required and not installed.", sHint.c_str());
			}
		}
		else
		{
			//Look for delay-load dependencies
			sFailedDependencies = "";
			FindStrings(sPlugin, sFailedDependencies);
			if (sFailedDependencies != "")
				s_Msg += StrFormat("\"%s\"\n\nPotentially missing dependencies:\n%s", sPlugin.c_str(), sFailedDependencies.c_str());
		}

		if (s_Msg != "")
			mPluginErrors.insert(s_Msg);
	}

	AVS_main = 0;
	Sleep(50);
	AVS_env->DeleteScriptEnvironment();
	AVS_env = 0;

	AVS_linkage = 0;

	FreeLibrary(hDLL);
	Sleep(50);

	return;
}


void CAvisynthInfo::GetDLLDependencies(string s_dll, string &s_failed_dependencies, string &s_hint)
{
	BOOL bRet = TRUE;
	HANDLE hFile = NULL;
	HANDLE hFileMapping = NULL;
	LPBYTE lpbaseAddress = NULL;
	s_failed_dependencies = "";

	try
	{
		hFile = CreateFile(s_dll.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
			return;
		
		hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
		if (hFileMapping == NULL || hFileMapping == INVALID_HANDLE_VALUE)
		{
			CloseHandle(hFile);
			return;
		}
		
		lpbaseAddress = (LPBYTE)MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
		if (lpbaseAddress == NULL)
		{
			CloseHandle(hFileMapping);
			CloseHandle(hFile);
			return;
		}
		
		PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)lpbaseAddress;
		PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)(lpbaseAddress + pDosHeader->e_lfanew);
		DWORD rva_import_table = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
		if (rva_import_table == 0)
		{
			UnmapViewOfFile(lpbaseAddress);
			CloseHandle(hFileMapping);
			CloseHandle(hFile);
			return;
		}
		
		string sDepDLL = "";
		PIMAGE_IMPORT_DESCRIPTOR pImageTable = (PIMAGE_IMPORT_DESCRIPTOR)ImageRvaToVa(pNtHeaders, lpbaseAddress, rva_import_table, NULL);
		IMAGE_IMPORT_DESCRIPTOR null_iid;
		IMAGE_THUNK_DATA null_thunk;
		memset(&null_iid, 0, sizeof(null_iid));
		memset(&null_thunk, 0, sizeof(null_thunk));
		for (int i = 0; memcmp(pImageTable + i, &null_iid, sizeof(null_iid)) != 0; i++)
		{

			LPCSTR szDepName = (LPCSTR)ImageRvaToVa(pNtHeaders, lpbaseAddress, pImageTable[i].Name,NULL);
			sDepDLL = StrFormat("  %s\n", szDepName);

			HINSTANCE hDLL = ::LoadLibraryEx(szDepName, NULL, LOAD_LIBRARY_AS_DATAFILE);

			if (!hDLL)
				s_failed_dependencies += sDepDLL;
			else
				::FreeLibrary(hDLL);
		}
	}
	catch (...)
	{
	}
	
	if (lpbaseAddress)
		UnmapViewOfFile(lpbaseAddress);
	if (hFileMapping)
		CloseHandle(hFileMapping);
	if (hFile)
		CloseHandle(hFile);

	string sTemp = s_failed_dependencies;
	StrToLC(sTemp);

	s_hint = "";
	if ((sTemp.find("msvcm71.dll") != std::string::npos) || (sTemp.find("msvcp71.dll") != std::string::npos) || (sTemp.find("msvcr71.dll") != std::string::npos))
		s_hint += "Microsoft Visual Studio 2003 Runtime Distributable\n";
	if ((sTemp.find("msvcm80.dll") != std::string::npos) || (sTemp.find("msvcp80.dll") != std::string::npos) || (sTemp.find("msvcr80.dll") != std::string::npos))
		s_hint += "Microsoft Visual Studio 2005 Runtime Distributable\n";
	if ((sTemp.find("msvcm90.dll") != std::string::npos) || (sTemp.find("msvcp90.dll") != std::string::npos) || (sTemp.find("msvcr90.dll") != std::string::npos))
		s_hint += "Microsoft Visual Studio 2008 Runtime Distributable\n";
	if ((sTemp.find("msvcm100.dll") != std::string::npos) || (sTemp.find("msvcp100.dll") != std::string::npos) || (sTemp.find("msvcr100.dll") != std::string::npos))
		s_hint += "Microsoft Visual Studio 2010 Runtime Distributable\n";
	if ((sTemp.find("msvcm110.dll") != std::string::npos) || (sTemp.find("msvcp110.dll") != std::string::npos) || (sTemp.find("msvcr110.dll") != std::string::npos))
		s_hint += "Microsoft Visual Studio 2012 Runtime Distributable\n";
	if ((sTemp.find("msvcm120.dll") != std::string::npos) || (sTemp.find("msvcp120.dll") != std::string::npos) || (sTemp.find("msvcr120.dll") != std::string::npos))
		s_hint += "Microsoft Visual Studio 2013 Runtime Distributable\n";
	if ((sTemp.find("msvcm140.dll") != std::string::npos) || (sTemp.find("msvcp140.dll") != std::string::npos) || (sTemp.find("msvcr140.dll") != std::string::npos))
		s_hint += "Microsoft Visual Studio 2015 Runtime Distributable\n";

	s_failed_dependencies.erase(s_failed_dependencies.find_last_not_of("\n") + 1);
	s_hint.erase(s_hint.find_last_not_of("\n") + 1);

	return;
}


void CAvisynthInfo::FindStrings(string s_dll, string &s_failed_deps)
{
	if (FileSize(s_dll) > (50 * 1024 * 1024))
		return;

	size_t pos = 0;

	FILE *fp = fopen(s_dll.c_str(), "rb");
	if (fp)
	{
		string content;
		fseek(fp, 0, SEEK_END);
		content.resize(ftell(fp));
		rewind(fp);
		fread(&content[0], 1, content.size(), fp);
		fclose(fp);

		transform(content.begin(), content.end(), content.begin(), ::tolower);

		pos = content.find("libfftw3f-3.dll");
		if (pos != std::string::npos)
		{
			HINSTANCE hDLL = ::LoadLibrary("libfftw3f-3.dll");
			if (!hDLL)
				s_failed_deps += "  libfftw3f-3.dll";
			else
				::FreeLibrary(hDLL);

			return;
		}

		pos = content.find("fftw3.dll");
		if (pos != std::string::npos)
		{
			HINSTANCE hDLL = ::LoadLibrary("fftw3.dll");
			if (!hDLL)
				s_failed_deps += "  fftw3.dll";
			else
				::FreeLibrary(hDLL);

			return;
		}
	}

	return;
}


__int64 CAvisynthInfo::FileSize(string s_file)
{
	WIN32_FIND_DATA fd; 
	__int64 iSize = -1;

	HANDLE hFind = FindFirstFile(s_file.c_str(), &fd); 
	if (hFind != INVALID_HANDLE_VALUE)
		iSize =	(((__int64)fd.nFileSizeHigh) << 32) + fd.nFileSizeLow;

	FindClose(hFind); 

	return iSize;
}


void CAvisynthInfo::StrTokenize(string s_string, set<string, StrCaseInsensitiveCompare> &Tokens)
{
	string sToken = "";
	size_t current;
	size_t next = -1;
	do
	{
		current = next + 1;
		next = s_string.find_first_of(" ", current);
		sToken = s_string.substr(current, next - current);
		StrTrim(sToken);
		if (sToken != "")
			Tokens.insert(sToken);
	}
	while (next != string::npos);


	return;
}


