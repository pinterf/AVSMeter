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

#include "AvisynthInfo.h"
const AVS_Linkage *AVS_linkage = 0;

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

	string sDependencies = "";
	string sFailedDependencies = "";
	string sHint = "";
	GetDLLDependencies(sDLLPath, sDependencies, sFailedDependencies, sHint);

	if (sDependencies != "")
		vPluginDependencies.push_back(sDependencies);

	if (!hDLL)
	{
		sSysError = SysErrorMessage();
		if (sSysError != "")
			s_ErrorMsg = StrFormat("Failed to load avisynth.dll:\n%s", sSysError.c_str());
		else
			s_ErrorMsg = "Failed to load avisynth.dll.";

		if (sFailedDependencies != "")
		{
			s_ErrorMsg += StrFormat("\n\nDependencies that couldn't be loaded:\n%s", sFailedDependencies.c_str());
			if (sHint != "")
				s_ErrorMsg += StrFormat("\n\nNote: \"%s\"\n      is required but doesn't seem to be installed.", sHint.c_str());
		}

		return 0;
	}

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

	AVS_linkage = AVS_env->GetAVSLinkage();

	try
	{
		_set_se_translator(SE_Translator);
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
	}
	catch (AvisynthError err)
	{
		iInterfaceVersion = 0;
		s_ErrorMsg = StrFormat("%s", (PCSTR)err.msg);
	}
	catch (exception& ex)
	{
		iInterfaceVersion = 0;
		s_ErrorMsg = ex.what();
	}
	catch (...)
	{
		iInterfaceVersion = 0;
		s_ErrorMsg = SysErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot load avisynth.dll:\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot load avisynth.dll";
	}

	Sleep(100);
	AVS_env->DeleteScriptEnvironment();
	AVS_env = 0;
	AVS_linkage = 0;

	if (!::FreeLibrary(hDLL))
	{
		iInterfaceVersion = 0;
		s_ErrorMsg = SysErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot unload avisynth.dll:\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot unload avisynth.dll";
	}

	Sleep(50);

	return iInterfaceVersion;
}


BOOL CAvisynthInfo::GetInfo(string &s_ErrorMsg)
{
	sDLLPath = "";
	bIs64BitAVS = FALSE;
	bIsMTVersion = FALSE;
	bIsAVSPlus = FALSE;
	iInterfaceVersion = 0;
	sFileVersion = "";
	sVersionString = "Unknown Avisynth Version";
	BOOL bAVSLinkage = FALSE;
	bLoadError = FALSE;
	BOOL bRet = TRUE;
	s_ErrorMsg = "";

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
			s_ErrorMsg = "Cannot load avisynth.dll:\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot load avisynth.dll";

		return FALSE;
	}

	try
	{
		_set_se_translator(SE_Translator);
		sDLLPath = StrFormat("%s", li.ModuleName);
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
			bIs64BitAVS = TRUE;
			#ifndef AVS64
				UnMapAndLoad(&li);
				s_ErrorMsg = "Cannot load avisynth.dll";
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
			StrToLC(sName);

			pos = sName.find("avs_linkage");
			if (pos != std::string::npos)
			{
				bAVSLinkage = TRUE;
				break;
			}
		}
	}
	catch (exception& ex)
	{
		bRet = FALSE;
		s_ErrorMsg = ex.what();
	}
	catch (...)
	{
		bRet = FALSE;
		s_ErrorMsg = SysErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot load avisynth.dll:\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot load avisynth.dll";
	}

	if (!UnMapAndLoad(&li))
	{
		bRet = FALSE;
		s_ErrorMsg = SysErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot unload avisynth.dll:\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot unload avisynth.dll";
	}

	if (!bRet)
		return FALSE;


	#ifndef AVS64
	SYSTEM_INFO stInfo;
	GetNativeSystemInfo(&stInfo);
	if ((stInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) || (stInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64))
	{
		HANDLE hFile = CreateFile(sDLLPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		if (hFile != INVALID_HANDLE_VALUE)
		{
			char pszFilename[2048];
			HANDLE hFileMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 1, NULL);;
			if (hFileMap)
			{
				void* pMem = MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 1);
				if (pMem)
				{
					if (GetMappedFileName(GetCurrentProcess(), pMem, pszFilename, 2046))
					{
						char pszWinDir[2048];
						GetWindowsDirectory(pszWinDir, 2046);
						string sWinDir(pszWinDir);
						sWinDir = sWinDir.substr(2);
						string sTestSystem32 = sWinDir + "\\system32\\";
						string sTestSysWOW64 = sWinDir + "\\syswow64\\";
						StrToLC(sTestSystem32);
						StrToLC(sTestSysWOW64);
						string sTempDLLPath = sDLLPath;
						string sRealDLLPath(pszFilename);
						StrToLC(sRealDLLPath);
						StrToLC(sTempDLLPath);
	
						if ((sRealDLLPath.find(sTestSysWOW64) != std::string::npos) && (sTempDLLPath.find(sTestSystem32) != std::string::npos))
						{
							size_t spos = sTempDLLPath.find("\\system32\\");
							if (spos > 2)
								sDLLPath = sDLLPath.substr(0, spos) + "\\SysWOW64\\" + sDLLPath.substr(spos + 10);
						}
					}
				}
				UnmapViewOfFile(pMem);
			}
			CloseHandle(hFileMap);
		}
		CloseHandle(hFile);
	}
	#endif //AVS64

	sFileVersion = GetFileVersion(sDLLPath);

	if (!bAVSLinkage)
	{
		s_ErrorMsg = "Avisynth version is too old.\nUpdate to Avisynth 2.6 or Avisynth+";
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
			s_ErrorMsg = "Cannot load avisynth.dll:\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot load avisynth.dll";

		return FALSE;
	}

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
		s_ErrorMsg = "Cannot create IScriptenvironment";
		::FreeLibrary(hDLL);
		return FALSE;
	}

	AVS_linkage = AVS_env->GetAVSLinkage();
	AVSValue foo;

	try
	{
		_set_se_translator(SE_Translator);

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
			sVersionNumber = "Cannot invoke \"VersionNumber\"";
		}


		try
		{
			sVersionString = StrFormat("%s", AVS_env->Invoke("VersionString", AVSValue(&foo, 0)).AsString());
		}
		catch (IScriptEnvironment::NotFound)
		{
			sVersionString = "Cannot invoke \"VersionString\"";
		}

		string sInternalFunctions = "";
		try
		{
			foo = AVS_env->GetVar("$InternalFunctions$");
			sInternalFunctions = foo.AsString();
			StrTokenize(sInternalFunctions, vInternalFunctions, " ", TRUE);
			sort(vInternalFunctions.begin(), vInternalFunctions.end());
		}
		catch(...)
		{
			sInternalFunctions = "";
		}

		string sPluginFunctions = "";
		try
		{
			foo = AVS_env->GetVar("$PluginFunctions$");
			sPluginFunctions = foo.AsString();
			StrTokenize(sPluginFunctions, vPluginFunctions, " ", TRUE);
			sort(vPluginFunctions.begin(), vPluginFunctions.end());
		}
		catch(...)
		{
			sPluginFunctions = "";
		}
	}
	catch (AvisynthError err)
	{
		bRet = FALSE;
		s_ErrorMsg = StrFormat("%s", (PCSTR)err.msg);
	}
	catch (exception& ex)
	{
		bRet = FALSE;
		s_ErrorMsg = ex.what();
	}
	catch (...)
	{
		bRet = FALSE;
		s_ErrorMsg = SysErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot load avisynth.dll:\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot load avisynth.dll";
	}

	foo = 0;
	Sleep(100);
	AVS_env->DeleteScriptEnvironment();
	AVS_env = 0;
	AVS_linkage = 0;

	if (!::FreeLibrary(hDLL))
	{
		bRet = FALSE;
		s_ErrorMsg = SysErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot unload avisynth.dll:\n" + s_ErrorMsg;
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

	if (!bIs64BitAVS)
	{
		hKeyResult = 0;
		lRes = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Avisynth", 0, KEY_READ | KEY_WOW64_32KEY, &hKeyResult);
		if (lRes == ERROR_SUCCESS)
		{
			char szPlugDir[MAX_PATH + 1] = "";
			DWORD dwType = REG_SZ;
			DWORD dwBytes = MAX_PATH;
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
						vPluginDirs.push_back("PluginDir+   (HKCU, x86):\t" + sTemp);
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
			char szPlugDir[MAX_PATH + 1] = "";
			DWORD dwType = REG_SZ;
			DWORD dwBytes = MAX_PATH;
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
						vPluginDirs.push_back("PluginDir+   (HKLM, x86):\t" + sTemp);
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
			char szPlugDir[MAX_PATH + 1] = "";
			DWORD dwType = REG_SZ;
			DWORD dwBytes = MAX_PATH;
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
						vPluginDirs.push_back("PluginDir2_5 (HKCU, x86):\t" + sTemp);
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
			char szPlugDir[MAX_PATH + 1] = "";
			DWORD dwType = REG_SZ;
			DWORD dwBytes = MAX_PATH;
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
						vPluginDirs.push_back("PluginDir2_5 (HKLM, x86):\t" + sTemp);
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
			char szPlugDir[MAX_PATH + 1] = "";
			DWORD dwType = REG_SZ;
			DWORD dwBytes = MAX_PATH;
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
						vPluginDirs.push_back("PluginDir+   (HKCU, x64):\t" + sTemp);
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
			char szPlugDir[MAX_PATH + 1] = "";
			DWORD dwType = REG_SZ;
			DWORD dwBytes = MAX_PATH;
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
						vPluginDirs.push_back("PluginDir+   (HKLM, x64):\t" + sTemp);
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
			char szPlugDir[MAX_PATH + 1] = "";
			DWORD dwType = REG_SZ;
			DWORD dwBytes = MAX_PATH;
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
						vPluginDirs.push_back("PluginDir2_5 (HKCU, x64):\t" + sTemp);
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
			char szPlugDir[MAX_PATH + 1] = "";
			DWORD dwType = REG_SZ;
			DWORD dwBytes = MAX_PATH;
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
						vPluginDirs.push_back("PluginDir2_5 (HKLM, x64):\t" + sTemp);
						mDuplicates.insert(sTempLower);
					}
				}
			}

			RegCloseKey(hKeyResult);
		}
	}

	if (vPluginDirs.size() < 1)
		vPluginErrors.push_back("No plugin directory references found in the registry.\nPlugin auto-loading disabled.");

	return;
}


string CAvisynthInfo::GetFileVersion(string s_file)
{
	string sFileVersion = "0.0.0.0";
	vector<char> szFileName(MAX_PATH + 1);

	if (_snprintf(&szFileName[0], szFileName.size() - 1, "%s", s_file.c_str()) < 0)
		return sFileVersion;

	DWORD dummy;
	DWORD dwSize = GetFileVersionInfoSize(&szFileName[0], &dummy);
	if (dwSize == 0)
		return sFileVersion;

	BYTE* VersionInfoBuffer = new BYTE[dwSize];
	if (!GetFileVersionInfo(&szFileName[0], 0, dwSize, (VOID*)VersionInfoBuffer))
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
	string sTimeStamp = "";
	WIN32_FIND_DATA fd;
	HANDLE hFind = FindFirstFile(s_file.c_str(), &fd); 

	if (hFind == INVALID_HANDLE_VALUE)
		return "Cannot determine timestamp";

	SYSTEMTIME fTime;

	if (!FileTimeToSystemTime(&fd.ftLastWriteTime, &fTime))
	{
		FindClose(hFind);
		return "Cannot determine timestamp";
	}

	FindClose(hFind);

	sTimeStamp = StrFormat("%04u-%02u-%02u, %02u:%02u:%02u (UTC)", fTime.wYear, fTime.wMonth, fTime.wDay, fTime.wHour, fTime.wMinute, fTime.wSecond);

	return sTimeStamp;
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
	transform(s_string.begin(), s_string.end(), s_string.begin(), ::tolower);
	return;
}


void CAvisynthInfo::EnumPluginDLLs()
{
	string sDir = "";
	string sMessage = "";
	BOOL bIs64BitDLL = FALSE;

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
					if (!SYSCheckPlugin(sCurrentFile, sMessage, bIs64BitDLL))
					{
						if (sMessage != "")
							vPluginErrors.push_back(sMessage);
					}
					else
					{
						if (bIs64BitDLL)
						{
							if (sMessage == "AVSC25")
								vPlugins.push_back("C 2.5 / 64 Bit|" + sCurrentFile);
							if (sMessage == "AVSC20")
								vPlugins.push_back("C 2.0 / 64 Bit|" + sCurrentFile);
							if (sMessage == "AVS26CPP")
								vPlugins.push_back("CPP 2.6 / 64 Bit|" + sCurrentFile);
							if (sMessage == "AVS25CPP")
								vPlugins.push_back("CPP 2.5 / 64 Bit|" + sCurrentFile);
							if (sMessage == "AVS20CPP")
								vPlugins.push_back("CPP 2.0 / 64 Bit|" + sCurrentFile);
						}
						else
						{
							if (sMessage == "AVSC25")
								vPlugins.push_back("C 2.5 / 32 Bit|" + sCurrentFile);
							if (sMessage == "AVSC20")
								vPlugins.push_back("C 2.0 / 32 Bit|" + sCurrentFile);
							if (sMessage == "AVS26CPP")
								vPlugins.push_back("CPP 2.6 / 32 Bit|" + sCurrentFile);
							if (sMessage == "AVS25CPP")
								vPlugins.push_back("CPP 2.5 / 32 Bit|" + sCurrentFile);
							if (sMessage == "AVS20CPP")
								vPlugins.push_back("CPP 2.0 / 32 Bit|" + sCurrentFile);
						}
					}
				}
				bRet = FindNextFile(hFind, &fd);
			}

			FindClose (hFind);
		}
	}

	sort(vPlugins.begin(), vPlugins.end(), CompareNoCase);

	AVSCheckPlugins();

	return;
}


BOOL CAvisynthInfo::SYSCheckPlugin(string s_dll, string &s_Msg, BOOL &b_Is64BitDLL)
{
	BOOL bRet = TRUE;
	s_Msg = "";
	BOOL bIsPlugin = FALSE;
	b_Is64BitDLL = FALSE;

	LOADED_IMAGE li;
	BOOL bLoaded = MapAndLoad((LPSTR)s_dll.c_str(), NULL, &li,	TRUE,	TRUE);

	if (!bLoaded)
	{
		s_Msg = SysErrorMessage();
		if (s_Msg != "")
			s_Msg = StrFormat("Error loading \"%s\":\n%s", s_dll.c_str(), s_Msg.c_str());
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
			b_Is64BitDLL = TRUE;
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

		for (DWORD dwName = 0; dwName < pExp->NumberOfNames; ++dwName)
		{
			DWORD rvaName = prvaNames[dwName];
			sName = (char *)ImageRvaToVa(li.FileHeader, li.MappedAddress, rvaName, NULL);
			StrToLC(sName);

			if (sName.find("avisynthplugininit3") != std::string::npos)
			{
				bIsPlugin = TRUE;
				s_Msg = "AVS26CPP";
				break;
			}

			if (sName.find("avisynthplugininit2") != std::string::npos)
			{
				bIsPlugin = TRUE;
				s_Msg = "AVS25CPP";
				break;
			}

			if (sName.find("avisynthplugininit") != std::string::npos)
			{
				bIsPlugin = TRUE;
				s_Msg = "AVS20CPP";
				break;
			}

			if (sName.find("avisynth_c_plugin_init@4") != std::string::npos)
			{
				bIsPlugin = TRUE;
				s_Msg = "AVSC25";
				break;
			}

			if (sName.find("avisynth_c_plugin_init") != std::string::npos)
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
	catch (exception& ex)
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
	string sPlugError = "";
	string sPlugErrorLC = "";
	string sNote = "";
	string sPlugin = "";
	string sPluginType = "";	
	unsigned int uiPlugin = 0;
	size_t spos = 0;
	string sDependencies = "";
	string sFailedDependencies = "";
	string sHint = "";
	string sMsg = "";
	BOOL bMaxPlugsExceeded = FALSE;

	for (uiPlugin = 0; uiPlugin < vPlugins.size(); uiPlugin++)
	{
		sPlugin = vPlugins[uiPlugin];
		spos = sPlugin.find("|");
		if (spos < 3)
			continue;

		sPluginType = sPlugin.substr(0, spos);
		sPlugin = sPlugin.substr(spos + 1);

		sDependencies = "";
		sFailedDependencies = "";
		sHint = "";
		GetDLLDependencies(sPlugin, sDependencies, sFailedDependencies, sHint);
		if (sDependencies != "")
			vPluginDependencies.push_back(sDependencies);

		if (bMaxPlugsExceeded)
			continue;

		bPlugError = FALSE;
		sPlugError = "";
		sNote = "";
		try
		{
			AVS_main = AVS_env->Invoke("LoadPlugin", sPlugin.c_str());
		}
		catch (AvisynthError err)
		{
			sPlugError = StrFormat("%s", (PCSTR)err.msg);
			StrTrim(sPlugError);
			bPlugError = TRUE;

			sPlugErrorLC = sPlugError;
			StrToLC(sPlugErrorLC);
			if (sPlugErrorLC.find("too many plugins loaded") != std::string::npos)
				bMaxPlugsExceeded = TRUE;
		}

		if (bPlugError)
		{
			if (sFailedDependencies != "")
			{
				sNote += StrFormat("\n\nDependencies that could not be loaded:\n%s", sFailedDependencies.c_str());
				if (sHint != "")
					sNote += StrFormat("\n\nNote: \"%s\"", sHint.c_str());
			}
			if (sPluginType.substr(0, 5) == "C 2.0")
				sNote += "\n\nNote 1: C 2.0 Plugins are not supported by Avisynth+\nNote 2: For other Avisynth versions, C 2.0 Plugins must be loaded explicitly\n        with \"LoadCPlugin()\" and need the \"avisynth_c.dll\" support DLL";
			if (sPluginType.substr(0, 5) == "C 2.5")
				sNote += "\n\nNote: C-Plugins must be loaded explicitly with \"LoadCPlugin()\"";
		}
		else
		{
			//Delay-load dependencies
			if (sFailedDependencies != "")
			{
				sNote += StrFormat("\"%s\"\n\nDependencies that could not be loaded:\n  %s", sPlugin.c_str(), sFailedDependencies.c_str());
				if (sHint != "")
						sNote += StrFormat("\n\nNote: \"%s\"", sHint.c_str());
			}
		}

		if (sNote != "")
			sPlugError += sNote;

		if (sPlugError != "")
			vPluginErrors.push_back(sPlugError);
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


void CAvisynthInfo::GetDLLDependencies(string s_dll, string &s_dependencies, string &s_failed_dependencies, string &s_hint)
{
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

		//s_dependencies = "\"" + s_dll + "\":\n";
		s_dependencies = s_dll + ":\n";
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

			sDepDLL = StrFormat("  %s\n", szDepName);
			s_dependencies += "  " + sDepDLL;

			if ((sTemp == "msvcm80.dll") || (sTemp == "msvcp80.dll") || (sTemp == "msvcr80.dll"))
			{
				if (!IsRuntimeInstalled("2005"))
					s_failed_dependencies += sDepDLL;

				continue;
			}

			if ((sTemp == "msvcm90.dll") || (sTemp == "msvcp90.dll") || (sTemp == "msvcr90.dll"))
			{
				if (!IsRuntimeInstalled("2008"))
					s_failed_dependencies += sDepDLL;

				continue;
			}

			HINSTANCE hDLL = ::LoadLibrary(szDepName);
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

	s_failed_dependencies.erase(s_failed_dependencies.find_last_not_of("\n") + 1);
	sTemp = s_failed_dependencies;
	StrToLC(sTemp);

	s_hint = "";
	if ((sTemp.find("msvcm80.dll") != std::string::npos) || (sTemp.find("msvcp80.dll") != std::string::npos) || (sTemp.find("msvcr80.dll") != std::string::npos))
		s_hint += "Visual Studio 2005 Runtime doesn't seem to be installed\n";
	if ((sTemp.find("msvcm90.dll") != std::string::npos) || (sTemp.find("msvcp90.dll") != std::string::npos) || (sTemp.find("msvcr90.dll") != std::string::npos))
		s_hint += "Visual Studio 2008 Runtime doesn't seem to be installed\n";
	if ((sTemp.find("msvcm100.dll") != std::string::npos) || (sTemp.find("msvcp100.dll") != std::string::npos) || (sTemp.find("msvcr100.dll") != std::string::npos))
		s_hint += "Visual Studio 2010 Runtime doesn't seem to be installed\n";
	if ((sTemp.find("msvcm110.dll") != std::string::npos) || (sTemp.find("msvcp110.dll") != std::string::npos) || (sTemp.find("msvcr110.dll") != std::string::npos))
		s_hint += "Visual Studio 2012 Runtime doesn't seem to be installed\n";
	if ((sTemp.find("msvcm120.dll") != std::string::npos) || (sTemp.find("msvcp120.dll") != std::string::npos) || (sTemp.find("msvcr120.dll") != std::string::npos))
		s_hint += "Visual Studio 2013 Runtime doesn't seem to be installed\n";
	if ((sTemp.find("msvcm140.dll") != std::string::npos) || (sTemp.find("msvcp140.dll") != std::string::npos) || (sTemp.find("msvcr140.dll") != std::string::npos))
		s_hint += "Visual Studio 2015 Runtime doesn't seem to be installed\n";

	s_hint.erase(s_hint.find_last_not_of("\n") + 1);

	return;
}


BOOL CAvisynthInfo::IsRuntimeInstalled(string s_version)
{
	BOOL bInstalled = FALSE;
	LONG lRes;
	HKEY hKeyResult = 0;
	
	char szSubKey[2048] = "";
	vector<string> vSubKeys;
	DWORD dwBytes = 2050;
	DWORD dwIndex = 0;
	lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Classes\\Installer\\Products", 0, KEY_READ, &hKeyResult);
	if (lRes == ERROR_SUCCESS)
	{
		while (RegEnumKeyEx(hKeyResult, dwIndex, szSubKey, &dwBytes, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
		{
			vSubKeys.push_back(StrFormat("%s", szSubKey));
			dwBytes = 2050;
			++dwIndex;
		}
	}
	RegCloseKey(hKeyResult);

	for (unsigned int uiKey = 0; uiKey < vSubKeys.size(); uiKey++)
	{
		string sKey = "SOFTWARE\\Classes\\Installer\\Products\\" + vSubKeys[uiKey];
		hKeyResult = 0;
		lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, sKey.c_str(), 0, KEY_READ, &hKeyResult);
		if (lRes == ERROR_SUCCESS)
		{
			char szValue[2048] = "";
			DWORD dwType = REG_SZ;
			dwBytes = 2050;
			if (RegQueryValueEx(hKeyResult, "ProductName", NULL, &dwType, (LPBYTE)szValue, &dwBytes) == 0)
			{
				string sValue(szValue);
				StrToLC(sValue);

				BOOL bVersion = FALSE;
				BOOL bVC = FALSE;
				BOOL bRedist = FALSE;
				BOOL b64Bit = FALSE;

				if (sValue.find(s_version) != std::string::npos) bVersion = TRUE;
				if (sValue.find("visual c++") != std::string::npos) bVC = TRUE;
				if ((sValue.find("redistributable") != std::string::npos) || (sValue.find("runtime") != std::string::npos)) bRedist = TRUE;
				if (sValue.find("x64") != std::string::npos) b64Bit = TRUE;

				if (bVersion && bVC && bRedist && !b64Bit && !bIs64BitAVS) bInstalled = TRUE;
				if (bVersion && bVC && bRedist && b64Bit && bIs64BitAVS) bInstalled = TRUE;
			}
		}
		RegCloseKey(hKeyResult);
	}

	return bInstalled;
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


void CAvisynthInfo::StrTokenize(string s_string, vector<string> &Tokens, string s_separator, BOOL b_removedups)
{
	set <string> mDuplicates;
	string sToken = "";
	string sTemp = "";
	size_t current;
	size_t next = -1;
	do
	{
		current = next + 1;
		next = s_string.find_first_of(s_separator, current);
		sToken = s_string.substr(current, next - current);
		StrTrim(sToken);
		sTemp = sToken;
		StrToLC(sTemp);
		if ((sToken != "") && (mDuplicates.find(sTemp) == mDuplicates.end()))
		{
			Tokens.push_back(sToken);
			if (b_removedups)
				mDuplicates.insert(sTemp);
		}
	}
	while (next != string::npos);

	return;
}


