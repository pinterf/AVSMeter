#ifndef _AVISYNTHINFO_H
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

#define BUFSIZE 2048

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
	map     <string, string, StrCaseInsensitiveCompare> mPluginDLLs;
	vector  <string> vPluginErrors;
	BYTE    NumPlugDirRefs;
	string  sDLLPath;
	string  sLinkerVersion;
	string  sFileVersion;
	string  sVersionString;
	string  sVersionNumber;
	string  sTimeStamp;
	BOOL    bIs64BitVersion;
	BOOL    bLoadError;
	BOOL    bIsAVSPlus;
	BOOL    bIsMTVersion;	
	virtual ~CAvisynthInfo();

private:
	void           StrTokenize(string s_string, vector<string> &Tokens);
	BOOL           GetFileNameFromHandle(HANDLE hFile, string &sFileName);
	string         GetFileVersion(string s_file);
	string         GetTimeStamp(string s_file);
	int            GetInterfaceVersion(string &s_ErrorMsg);
  void           EnumPluginDirs();
	void           EnumPluginDLLs();
	BOOL           SYSCheckPlugin(string s_dll, string &s_Msg);
	void           AVSCheckPlugins();
	void           GetDLLDependencies(string s_dll, string &s_failed_dependencies, string &s_hint);
	void           FindStrings(string s_dll, string &s_failed_dependencies, string &s_hint);
	__int64        FileSize(string s_file);
	inline string  StrFormat(char const *fmt, ...);
	void           StrTrim(string &s_string);
	void           StrToLC(string &s_string);
};


CAvisynthInfo::CAvisynthInfo()
{
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
			s_ErrorMsg += StrFormat("\n\nDependencies that couldn't be loaded:\n%s", sFailedDependencies.c_str());
			if (sHint != "")
				s_ErrorMsg += StrFormat("\n\nNote: \"%s\"\n      is required but doesn't seem to be installed.", sHint.c_str());
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

		Sleep(100);
		AVS_env->DeleteScriptEnvironment();
		AVS_env = 0;
	}
	catch (AvisynthError err)
	{
		iInterfaceVersion = 0;
		s_ErrorMsg = StrFormat("%s", (PCSTR)err.msg);
	}
	catch (std::exception& ex)
	{
		iInterfaceVersion = 0;
		s_ErrorMsg = ex.what();
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
	bIsAVSPlus = FALSE;
	iInterfaceVersion = 0;
	sFileVersion = "";
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
		if (li.FileHeader->FileHeader.Machine == IMAGE_FILE_MACHINE_I386)
		{
			#ifdef AVS64
				UnMapAndLoad(&li);
				s_ErrorMsg = "Cannot load avisynth.dll";
				return FALSE;
			#endif
		}
		else //64 Bit Avisynth DLL
		{
			bIs64BitVersion = TRUE;
			#ifndef AVS64
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
	catch (std::exception& ex)
	{
		bRet = FALSE;
		s_ErrorMsg = ex.what();
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
		HANDLE hFile = CreateFile(sTempDLLPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		if (hFile != INVALID_HANDLE_VALUE)
		{
			if (!GetFileNameFromHandle(hFile, sDLLPath))
				sDLLPath = sTempDLLPath;
		}

		CloseHandle(hFile);
	}
	catch (std::exception& ex)
	{
		bRet = FALSE;
		s_ErrorMsg = ex.what();
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


	sFileVersion = GetFileVersion(sDLLPath);

	if (!bAVSLinkage)
	{
		s_ErrorMsg = "Avisynth version is too old.\nPlease update to Avisynth 2.6 or Avisynth+.";
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

		if (AVS_env->FunctionExists("AutoloadPlugins"))
			bIsAVSPlus = TRUE;

		if ((AVS_env->FunctionExists("SetMTMode")) || (AVS_env->FunctionExists("Prefetch")))
			bIsMTVersion = TRUE;

		try
		{
			sVersionNumber = StrFormat("%f", AVS_env->Invoke("VersionNumber", AVSValue(&foo, 0)).AsFloat());
			sVersionNumber.erase(sVersionNumber.find_last_not_of("0") + 1);
			while (sVersionNumber.length() < 4)
				sVersionNumber += "0";
		}
		catch (IScriptEnvironment::NotFound)
		{
			sVersionNumber = "Unknown Avisynth Version";
		}


		try
		{
			sVersionString = StrFormat("%s", AVS_env->Invoke("VersionString", AVSValue(&foo, 0)).AsString());
		}
		catch (IScriptEnvironment::NotFound)
		{
			sVersionString = "Unknown Avisynth Version";
		}


		string sInternalFunctions = "";
		try
		{
			foo = AVS_env->GetVar("$InternalFunctions$");
			sInternalFunctions = foo.AsString();
			StrTokenize(sInternalFunctions, vInternalFunctions);
			sort(vInternalFunctions.begin(), vInternalFunctions.end());
		}
		catch(...)
		{
		}

		string sPluginFunctions = "";
		try
		{
			foo = AVS_env->GetVar("$PluginFunctions$");
			sPluginFunctions = foo.AsString();
			StrTokenize(sPluginFunctions, vPluginFunctions);
			sort(vPluginFunctions.begin(), vPluginFunctions.end());
		}
		catch(...)
		{
		}

		foo = 0;
		Sleep(100);
		AVS_env->DeleteScriptEnvironment();
		AVS_env = 0;
	}
	catch (AvisynthError err)
	{
		bRet = FALSE;
		s_ErrorMsg = StrFormat("%s", (PCSTR)err.msg);
	}
	catch (std::exception& ex)
	{
		bRet = FALSE;
		s_ErrorMsg = ex.what();
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
	EnumPluginDLLs();

	return bRet;
}


void CAvisynthInfo::EnumPluginDirs()
{
	vPluginDirs.empty();

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

				if ((sTemp != "") && (bIsAVSPlus))
				{
					if (mDuplicates.find(sTempLower) == mDuplicates.end())
					{
						vPluginDirs.push_back("PluginDir+    (HKCU, x86):\t" + sTemp);
						mDuplicates.insert(sTempLower);
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

				if ((sTemp != "") && (bIsAVSPlus))
				{
					if (mDuplicates.find(sTempLower) == mDuplicates.end())
					{
						vPluginDirs.push_back("PluginDir+    (HKLM, x86):\t" + sTemp);
						mDuplicates.insert(sTempLower);
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
					if (mDuplicates.find(sTempLower) == mDuplicates.end())
					{
						vPluginDirs.push_back("PluginDir2_5  (HKCU, x86):\t" + sTemp);
						mDuplicates.insert(sTempLower);
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
					if (mDuplicates.find(sTempLower) == mDuplicates.end())
					{
						vPluginDirs.push_back("PluginDir2_5  (HKLM, x86):\t" + sTemp);
						mDuplicates.insert(sTempLower);
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

				if ((sTemp != "") && (bIsAVSPlus))
				{
					if (mDuplicates.find(sTempLower) == mDuplicates.end())
					{
						vPluginDirs.push_back("PluginDir+    (HKCU, x64):\t" + sTemp);
						mDuplicates.insert(sTempLower);
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

				if ((sTemp != "") && (bIsAVSPlus))
				{
					if (mDuplicates.find(sTempLower) == mDuplicates.end())
					{
						vPluginDirs.push_back("PluginDir+    (HKLM, x64):\t" + sTemp);
						mDuplicates.insert(sTempLower);
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
					if (mDuplicates.find(sTempLower) == mDuplicates.end())
					{
						vPluginDirs.push_back("PluginDir2_5  (HKCU, x64):\t" + sTemp);
						mDuplicates.insert(sTempLower);
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
					if (mDuplicates.find(sTempLower) == mDuplicates.end())
					{
						vPluginDirs.push_back("PluginDir2_5  (HKLM, x64):\t" + sTemp);
						mDuplicates.insert(sTempLower);
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
	HANDLE hFile = CreateFile(s_file.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
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


inline string CAvisynthInfo::StrFormat(char const *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	vector<char> buffer(256);
	int iRet = -1;
	while (iRet == -1)
	{
		iRet = _vsnprintf(&buffer[0], buffer.size() - 1, fmt, args);
		if (iRet == -1)
			buffer.resize(buffer.size() * 2);
		if (buffer.size() > 16384)
			return "\nBuffer overflow";
	}
	buffer[buffer.size() - 1] = '\0';
	string sRet = &buffer[0];
	va_end(args);

	return sRet;
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

	for (unsigned int uiPlugDir = 0; uiPlugDir < vPluginDirs.size(); uiPlugDir++)
	{
		sDir = vPluginDirs[uiPlugDir];
		if (sDir != "")
		{
			size_t spos = sDir.find(":\t");
			sDir = sDir.substr(spos + 2);

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
							vPluginErrors.push_back(sMessage);
					}
					else
					{
						if (sMessage == "AVSC25")
							mPluginDLLs[sCurrentFile] = "C25";
						if (sMessage == "AVSC20")
							mPluginDLLs[sCurrentFile] = "C20";
						if (sMessage == "AVS26CPP")
							mPluginDLLs[sCurrentFile] = "CPP26";
						if (sMessage == "AVS25CPP")
							mPluginDLLs[sCurrentFile] = "CPP25";
						if (sMessage == "AVS20CPP")
							mPluginDLLs[sCurrentFile] = "CPP20";
					}
				}
				bRet = FindNextFile(hFind, &fd);
			}

			FindClose (hFind);
		}
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

		if (li.FileHeader->FileHeader.Machine == IMAGE_FILE_MACHINE_I386)
		{
			#ifdef AVS64
				UnMapAndLoad(&li);
				s_Msg = StrFormat("Error loading \"%s\"\nCannot load 32 bit DLL with 64 bit Avisynth", s_dll.c_str());
				return FALSE;
			#endif
		}
		else
		{
			bIs64BitDLL = TRUE;
			#ifndef AVS64
				UnMapAndLoad(&li);
				s_Msg = StrFormat("Error loading \"%s\"\nCannot load 64 bit DLL with 32 bit Avisynth", s_dll.c_str());
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
	catch (std::exception& ex)
	{
		bRet = FALSE;
		s_Msg = StrFormat("\"%s\"\n", s_dll.c_str()) + ex.what();
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
	string sPluginType = "";
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

	for (map<string, string, StrCaseInsensitiveCompare>::iterator it = mPluginDLLs.begin(); it != mPluginDLLs.end(); ++it)
	{
		sPlugin = it->first;
		sPluginType = it->second;

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

			StrToLC(sAVSError);
			if (sAVSError.find("0x36b3") != std::string::npos)
				s_Msg += "\n\nNote: Microsoft Visual Studio 2005 Runtime Distributable\n      is required but doesn't seem to be installed";
			if (sPluginType == "C25")
				s_Msg += "\n\nNote: C-Plugins must be loaded explicitly with \"LoadCPlugin()\"";
			if (sPluginType == "C20")
				s_Msg += "\n\nNote 1: C 2.0 Plugins are not supported by Avisynth+\nNote 2: For other Avisynth versions, C 2.0 Plugins must be loaded explicitly\n        with \"LoadCPlugin()\" and need \"avisynth_c.dll\" support DLL";
		}

		if (bPlugError)
		{
			sFailedDependencies = "";
			sHint = "";
			GetDLLDependencies(sPlugin, sFailedDependencies, sHint);
			if (sFailedDependencies != "")
			{
				s_Msg += StrFormat("\n\nDependencies that couldn't be loaded:\n%s", sFailedDependencies.c_str());
				if (sHint != "")
					s_Msg += StrFormat("\n\nNote: \"%s\"\n      is required but doesn't seem to be installed.", sHint.c_str());
			}
		}
		else
		{
			//Look for delay-load dependencies
			sFailedDependencies = "";
			sHint = "";
			FindStrings(sPlugin, sFailedDependencies, sHint);
			if (sFailedDependencies != "")
			{
				s_Msg += StrFormat("\"%s\"\n\nDependencies that couldn't be loaded:\n  %s", sPlugin.c_str(), sFailedDependencies.c_str());
				if (sHint != "")
					s_Msg += StrFormat("\n\nNote: %s", sHint.c_str());
			}
		}

		if (s_Msg != "")
			vPluginErrors.push_back(s_Msg);
	}

	AVS_main = 0;
	Sleep(100);
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
	string sTemp = "";

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
			sTemp = StrFormat("%s", szDepName);
			StrToLC(sTemp);
			StrTrim(sTemp);
			if ((sTemp == "msvcm80.dll") || (sTemp == "msvcp80.dll") || (sTemp == "msvcr80.dll"))
				continue;

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

	sTemp = s_failed_dependencies;
	StrToLC(sTemp);

	s_hint = "";
	if ((sTemp.find("msvcm71.dll") != std::string::npos) || (sTemp.find("msvcp71.dll") != std::string::npos) || (sTemp.find("msvcr71.dll") != std::string::npos))
		s_hint += "Microsoft Visual Studio 2003 Runtime Distributable\n";
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


void CAvisynthInfo::FindStrings(string s_dll, string &s_failed_dependencies, string &s_hint)
{
	if ((FileSize(s_dll) > (50 * 1024 * 1024)) || (FileSize(s_dll) < 32 ))
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
			{
				s_failed_dependencies += "libfftw3f-3.dll";
				s_hint += "libfftw3f-3.dll can be downloaded here:\n      http://www.fftw.org/install/windows.html\n";
			}
			else
				::FreeLibrary(hDLL);

			return;
		}

		pos = content.find("fftw3.dll");
		if (pos != std::string::npos)
		{
			HINSTANCE hDLL = ::LoadLibrary("fftw3.dll");
			if (!hDLL)
			{
				s_failed_dependencies += "fftw3.dll";
				s_hint += "fftw3.dll (libfftw3f-3.dll) can be downloaded here:\n      http://www.fftw.org/install/windows.html\n";
			}
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


void CAvisynthInfo::StrTokenize(string s_string, vector<string> &Tokens)
{
	set <string> mDuplicates;
	string sToken = "";
	string sTemp = "";
	size_t current;
	size_t next = -1;
	do
	{
		current = next + 1;
		next = s_string.find_first_of(" ", current);
		sToken = s_string.substr(current, next - current);
		StrTrim(sToken);
		sTemp = sToken;
		StrToLC(sTemp);
		if ((sToken != "") && (mDuplicates.find(sTemp) == mDuplicates.end()))
		{
			Tokens.push_back(sToken);
			mDuplicates.insert(sTemp);
		}
	}
	while (next != string::npos);

	return;
}


#endif //_AVISYNTHINFO_H
