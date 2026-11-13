/*
	This file is part of AVSMeter,
	Copyright(C) Groucho2004.

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

#ifndef _AVISYNTHINFO_H
#define _AVISYNTHINFO_H

#include "stdafx.h"

#define BUFSIZE 2048
#define NUM_PLUG_DIRS 8

class CAvisynthInfo
{
public:
	CAvisynthInfo();
	BOOL    GetInfo(string &s_ErrorMsg);
	string  StrFormat(const char *fmt, ...);
	void    StrTrim(string &s_string);
	string  OSErrorMessage();
	BOOL    GetFileNameFromHandle(HANDLE hFile, string &sFileName);
	string  GetFileVersion(string s_file);

	int     iInterfaceVersion;
	string  saPluginDirs[NUM_PLUG_DIRS];
	BYTE    NumPlugDirs;
	string  sDLLPath;
	string  sVersionNumber;
	string  sVersionString;
	string  sLinkerVersion;
	BOOL    bLoadError;
	BOOL    bIs64BitVersion;
	virtual ~CAvisynthInfo();
	BOOL    bIsMTVersion;
private:
	int     GetInterfaceVersion(string &s_ErrorMsg);
  void    EnumPluginDirs();
	BOOL    bAVSLinkage;
};

CAvisynthInfo::CAvisynthInfo()
{
	NumPlugDirs = NUM_PLUG_DIRS;
}

CAvisynthInfo::~CAvisynthInfo()
{
}


int CAvisynthInfo::GetInterfaceVersion(string &s_ErrorMsg)
{
	int iInterfaceVersion = 0;
	s_ErrorMsg = "";

	HINSTANCE hDLL = ::LoadLibrary("avisynth");
	if (!hDLL)
	{
		s_ErrorMsg = OSErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot load avisynth.dll\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot load avisynth.dll";

		return 0;
	}

	try
	{
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

		AVS_env->DeleteScriptEnvironment();
		AVS_env = 0;
	}
	catch (AvisynthError err)
	{
		iInterfaceVersion = 0;
		s_ErrorMsg = StrFormat("%s", (PCSTR)err.msg);
	}
	catch (...)
	{
		iInterfaceVersion = 0;
		s_ErrorMsg = OSErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot load avisynth.dll\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot load avisynth.dll";
	}

	::FreeLibrary(hDLL);

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
	bAVSLinkage = FALSE;
	bLoadError = FALSE;
	BOOL bRet = TRUE;
	s_ErrorMsg = "";
	string sTempDLLPath = "";

	LOADED_IMAGE li;
	BOOL bLoaded = MapAndLoad("avisynth", NULL, &li,	TRUE,	TRUE);
	if (!bLoaded)
	{
		bLoadError = TRUE;
		s_ErrorMsg = OSErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot load avisynth.dll\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot load avisynth.dll";

		return FALSE;
	}

	try
	{
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
		if (sLinkerVersion.substr(0, 2) == "6.")
			sLinkerVersion += " (VC 6)";
		if (sLinkerVersion.substr(0, 3) == "7.0")
			sLinkerVersion += " (VC 2002)";
		if (sLinkerVersion.substr(0, 3) == "7.1")
			sLinkerVersion += " (VC 2003)";
		if (sLinkerVersion.substr(0, 2) == "8.")
			sLinkerVersion += " (VC 2005)";
		if (sLinkerVersion.substr(0, 2) == "9.")
			sLinkerVersion += " (VC 2008)";
		if (sLinkerVersion.substr(0, 3) == "10.")
			sLinkerVersion += " (VC 2010)";
		if (sLinkerVersion.substr(0, 3) == "11.")
			sLinkerVersion += " (VC 2012)";
		if (sLinkerVersion.substr(0, 3) == "12.")
			sLinkerVersion += " (VC 2013)";
		if (sLinkerVersion.substr(0, 3) == "14.")
			sLinkerVersion += " (VC 2015)";

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
	catch (...)
	{
		bRet = FALSE;
		s_ErrorMsg = OSErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot load avisynth.dll\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot load avisynth.dll";
	}

	UnMapAndLoad(&li);

	if (!bRet)
		return FALSE;

	if (!bAVSLinkage)
	{
		s_ErrorMsg = "Avisynth version is too old. Please update to at least\nAvisynth 2.6 Alpha4 or Avisynth+ r1576.";
		return FALSE;
	}


	//This seems to be the only reliable way to determine the real path of avisynth.dll (System32/SysWoW64) across OS versions
	try
	{
		HANDLE hFile = CreateFile(sTempDLLPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
		if (hFile != INVALID_HANDLE_VALUE)
		{
			if (!GetFileNameFromHandle(hFile, sDLLPath))
				sDLLPath = sTempDLLPath;
		}

		CloseHandle(hFile);
	}
	catch (...)
	{
		bRet = FALSE;
		s_ErrorMsg = OSErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot open avisynth.dll\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot open avisynth.dll";
	}

	if (!bRet)
		return FALSE;

	sVersionNumber = GetFileVersion(sDLLPath);

	iInterfaceVersion = GetInterfaceVersion(s_ErrorMsg);
	if (iInterfaceVersion == 0)
		return FALSE;


	HINSTANCE hDLL = ::LoadLibrary("avisynth");
	if (!hDLL)
	{
		s_ErrorMsg = OSErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot load avisynth.dll\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot load avisynth.dll";

		return FALSE;
	}

	try
	{
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
			size_t spos = sVersionString.find("(ICL10)");
			if ((spos > 1) && (spos < 0xFFFF))
				sLinkerVersion = "6.0 (ICL 10.1)";
		}
		catch (IScriptEnvironment::NotFound)
		{
			sVersionString = "Unknown Avisynth Version";
		}

		if ((AVS_env->FunctionExists("SetMTMode")) || (AVS_env->FunctionExists("Prefetch")))
			bIsMTVersion = TRUE;

		foo = 0;
		AVS_env->DeleteScriptEnvironment();
		AVS_env = 0;
	}
	catch (AvisynthError err)
	{
		bRet = FALSE;
		s_ErrorMsg = StrFormat("%s", (PCSTR)err.msg);
	}
	catch (...)
	{
		bRet = FALSE;
		s_ErrorMsg = OSErrorMessage();
		if (s_ErrorMsg != "")
			s_ErrorMsg = "Cannot load avisynth.dll\n" + s_ErrorMsg;
		else
			s_ErrorMsg = "Cannot load avisynth.dll";
	}

	AVS_linkage = 0;
	::FreeLibrary(hDLL);

	EnumPluginDirs();

	return bRet;
}


void CAvisynthInfo::EnumPluginDirs()
{
	for (unsigned int n = 0; n < NUM_PLUG_DIRS; n++)
		saPluginDirs[n] = "";

	//- PluginDir+ in Software/Avisynth in HKEY_CURRENT_USER
	//- PluginDir+ in Software/Avisynth in HKEY_LOCAL_MACHINE
	//- PluginDir2_5 in Software/Avisynth in HKEY_CURRENT_USER
	//- PluginDir2_5 in Software/Avisynth in HKEY_LOCAL_MACHINE

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
			RegQueryValueEx(hKeyResult, "PluginDir+", NULL, &dwType, (LPBYTE)szPlugDir, &dwBytes);
			string sTemp(szPlugDir);
			StrTrim(sTemp);
			if (sTemp != "")
				saPluginDirs[0] = "PluginDir+    (HKCU, x86):             " + sTemp;
	
			RegCloseKey(hKeyResult);
		}
	
		hKeyResult = 0;
		lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Avisynth", 0, KEY_READ | KEY_WOW64_32KEY, &hKeyResult);
		if (lRes == ERROR_SUCCESS)
		{
			char szPlugDir[2050] = "";
			DWORD dwType = REG_SZ;
			DWORD dwBytes = 2048;
			RegQueryValueEx(hKeyResult, "PluginDir+", NULL, &dwType, (LPBYTE)szPlugDir, &dwBytes);
			string sTemp(szPlugDir);
			StrTrim(sTemp);
			if (sTemp != "")
				saPluginDirs[1] = "PluginDir+    (HKLM, x86):             " + sTemp;
	
			RegCloseKey(hKeyResult);
		}
	
		hKeyResult = 0;
		lRes = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Avisynth", 0, KEY_READ | KEY_WOW64_32KEY, &hKeyResult);
		if (lRes == ERROR_SUCCESS)
		{
			char szPlugDir[2050] = "";
			DWORD dwType = REG_SZ;
			DWORD dwBytes = 2048;
			RegQueryValueEx(hKeyResult, "PluginDir2_5", NULL, &dwType, (LPBYTE)szPlugDir, &dwBytes);
			string sTemp(szPlugDir);
			StrTrim(sTemp);
			if (sTemp != "")
				saPluginDirs[2] = "PluginDir2_5  (HKCU, x86):             " + sTemp;
	
			RegCloseKey(hKeyResult);
		}
	
		hKeyResult = 0;
		lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Avisynth", 0, KEY_READ | KEY_WOW64_32KEY, &hKeyResult);
		if (lRes == ERROR_SUCCESS)
		{
			char szPlugDir[2050] = "";
			DWORD dwType = REG_SZ;
			DWORD dwBytes = 2048;
			RegQueryValueEx(hKeyResult, "PluginDir2_5", NULL, &dwType, (LPBYTE)szPlugDir, &dwBytes);
			string sTemp(szPlugDir);
			StrTrim(sTemp);
			if (sTemp != "")
				saPluginDirs[3] = "PluginDir2_5  (HKLM, x86):             " + sTemp;
	
			RegCloseKey(hKeyResult);
		}
	}	
	else
	{
		hKeyResult = 0;
		lRes = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Avisynth", 0, KEY_READ | KEY_WOW64_64KEY, &hKeyResult);
		if (lRes == ERROR_SUCCESS)
		{
			char szPlugDir[2050] = "";
			DWORD dwType = REG_SZ;
			DWORD dwBytes = 2048;
			RegQueryValueEx(hKeyResult, "PluginDir+", NULL, &dwType, (LPBYTE)szPlugDir, &dwBytes);
			string sTemp(szPlugDir);
			StrTrim(sTemp);
			if (sTemp != "")
				saPluginDirs[4] = "PluginDir+    (HKCU, x64):             " + sTemp;
	
			RegCloseKey(hKeyResult);
		}
	
		hKeyResult = 0;
		lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Avisynth", 0, KEY_READ | KEY_WOW64_64KEY, &hKeyResult);
		if (lRes == ERROR_SUCCESS)
		{
			char szPlugDir[2050] = "";
			DWORD dwType = REG_SZ;
			DWORD dwBytes = 2048;
			RegQueryValueEx(hKeyResult, "PluginDir+", NULL, &dwType, (LPBYTE)szPlugDir, &dwBytes);
			string sTemp(szPlugDir);
			StrTrim(sTemp);
			if (sTemp != "")
				saPluginDirs[5] = "PluginDir+    (HKLM, x64):             " + sTemp;
	
			RegCloseKey(hKeyResult);
		}
	
	
		hKeyResult = 0;
		lRes = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Avisynth", 0, KEY_READ | KEY_WOW64_64KEY, &hKeyResult);
		if (lRes == ERROR_SUCCESS)
		{
			char szPlugDir[2050] = "";
			DWORD dwType = REG_SZ;
			DWORD dwBytes = 2048;
			RegQueryValueEx(hKeyResult, "PluginDir2_5", NULL, &dwType, (LPBYTE)szPlugDir, &dwBytes);
			string sTemp(szPlugDir);
			StrTrim(sTemp);
			if (sTemp != "")
				saPluginDirs[6] = "PluginDir2_5  (HKCU, x64):             " + sTemp;
	
			RegCloseKey(hKeyResult);
		}
	
		hKeyResult = 0;
		lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Avisynth", 0, KEY_READ | KEY_WOW64_64KEY, &hKeyResult);
		if (lRes == ERROR_SUCCESS)
		{
			char szPlugDir[2050] = "";
			DWORD dwType = REG_SZ;
			DWORD dwBytes = 2048;
			RegQueryValueEx(hKeyResult, "PluginDir2_5", NULL, &dwType, (LPBYTE)szPlugDir, &dwBytes);
			string sTemp(szPlugDir);
			StrTrim(sTemp);
			if (sTemp != "")
				saPluginDirs[7] = "PluginDir2_5  (HKLM, x64):             " + sTemp;
	
			RegCloseKey(hKeyResult);
		}
	}

	return;
}


string CAvisynthInfo::OSErrorMessage()
{
	DWORD dwLastError = GetLastError();
	char *szMsg;

	if (dwLastError == ERROR_SUCCESS)
		return "";
		
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
								NULL,
								dwLastError,
								MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
								(LPTSTR)&szMsg,
								0,
								NULL);

	string sMsg(szMsg);

	return sMsg;
}


string CAvisynthInfo::StrFormat(const char *fmt, ...)
{
	if (!fmt)	return "";

	unsigned int bufsize = 16384;
	unsigned int length = 256;
	char *buffer = NULL;
	int result = -1;

	va_list args;
	va_start(args, fmt);
	while (result == -1)
	{
		if (buffer)	delete [] buffer;
		buffer = new char[length];
		memset(buffer, ' ', sizeof(buffer));
		result = _vsnprintf(buffer, length, fmt, args);
		length *= 2;
		if (length >= bufsize)
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


#endif //_AVISYNTHINFO_H

