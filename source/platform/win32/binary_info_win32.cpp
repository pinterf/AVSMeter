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


#include "platform/binary_info.h"
#include "platform/os_info.h"
#include "platform/win32/utf_win32.h"

#include <algorithm>
#include <cctype>
#include <cstring>


namespace
{
  // Read-only view of a whole file
  class MappedFile
  {
  public:
    ~MappedFile()
    {
      DWORD dwLastError = ::GetLastError(); // keep the reason of a failure for the caller
      if (pData)
        ::UnmapViewOfFile(pData);
      if (hMapping)
        ::CloseHandle(hMapping);
      if (hFile != INVALID_HANDLE_VALUE)
        ::CloseHandle(hFile);
      ::SetLastError(dwLastError);
    }

    bool Open(const std::wstring &ws_path)
    {
      hFile = ::CreateFileW(ws_path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
      if (hFile == INVALID_HANDLE_VALUE)
        return false;

      LARGE_INTEGER liSize;
      if (!::GetFileSizeEx(hFile, &liSize))
        return false;
      if ((uint64_t)liSize.QuadPart > (uint64_t)SIZE_MAX)
      {
        ::SetLastError(ERROR_FILE_TOO_LARGE);
        return false;
      }

      hMapping = ::CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
      if (!hMapping)
        return false;

      pData = (const BYTE *)::MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
      if (!pData)
        return false;

      nSize = (size_t)liSize.QuadPart;
      return true;
    }

    const BYTE *pData = nullptr;
    size_t      nSize = 0;

  private:
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMapping = NULL;
  };


  // Bounds checked access to a PE32 or PE32+ image file
  class PeFile
  {
  public:
    PeFile(const BYTE *p_data, size_t n_size) : pData(p_data), nSize(n_size) {}

    bool Parse()
    {
      const IMAGE_DOS_HEADER *pDos = At<IMAGE_DOS_HEADER>(0);
      if (!pDos || (pDos->e_magic != IMAGE_DOS_SIGNATURE) || (pDos->e_lfanew < 0))
        return false;

      size_t nNtOffset = (size_t)pDos->e_lfanew;
      const DWORD *pSignature = At<DWORD>(nNtOffset);
      const IMAGE_FILE_HEADER *pFileHeader = At<IMAGE_FILE_HEADER>(nNtOffset + sizeof(DWORD));
      if (!pSignature || (*pSignature != IMAGE_NT_SIGNATURE) || !pFileHeader)
        return false;

      size_t nOptOffset = nNtOffset + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER);
      const WORD *pMagic = At<WORD>(nOptOffset);
      if (!pMagic)
        return false;

      DWORD dwDirCount;
      size_t nDirOffset;
      if ((*pMagic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) && At<IMAGE_OPTIONAL_HEADER64>(nOptOffset))
      {
        b64Bit = true;
        dwDirCount = At<IMAGE_OPTIONAL_HEADER64>(nOptOffset)->NumberOfRvaAndSizes;
        nDirOffset = nOptOffset + offsetof(IMAGE_OPTIONAL_HEADER64, DataDirectory);
      }
      else if ((*pMagic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) && At<IMAGE_OPTIONAL_HEADER32>(nOptOffset))
      {
        b64Bit = false;
        dwDirCount = At<IMAGE_OPTIONAL_HEADER32>(nOptOffset)->NumberOfRvaAndSizes;
        nDirOffset = nOptOffset + offsetof(IMAGE_OPTIONAL_HEADER32, DataDirectory);
      }
      else
        return false;

      pDirs = At<IMAGE_DATA_DIRECTORY>(nDirOffset, (std::min<DWORD>)(dwDirCount, IMAGE_NUMBEROF_DIRECTORY_ENTRIES));
      nDirs = pDirs ? (std::min<DWORD>)(dwDirCount, IMAGE_NUMBEROF_DIRECTORY_ENTRIES) : 0;

      pSections = At<IMAGE_SECTION_HEADER>(nOptOffset + pFileHeader->SizeOfOptionalHeader, pFileHeader->NumberOfSections);
      nSections = pSections ? pFileHeader->NumberOfSections : 0;

      return true;
    }

    bool Is64Bit() const { return b64Bit; }

    DWORD DirectoryRva(size_t n_dir) const
    {
      return (n_dir < nDirs) ? pDirs[n_dir].VirtualAddress : 0;
    }

    template <class T>
    const T *AtRva(DWORD dw_rva, size_t n_count = 1) const
    {
      size_t nOffset;
      return RvaToOffset(dw_rva, nOffset) ? At<T>(nOffset, n_count) : nullptr;
    }

    // Zero terminated string, empty if outside the file
    std::string StringAtRva(DWORD dw_rva) const
    {
      size_t nOffset;
      if (!RvaToOffset(dw_rva, nOffset) || (nOffset >= nSize))
        return std::string();

      const char *psz = (const char *)pData + nOffset;
      const void *pEnd = memchr(psz, 0, nSize - nOffset);
      return pEnd ? std::string(psz, (const char *)pEnd) : std::string();
    }

  private:
    template <class T>
    const T *At(size_t n_offset, size_t n_count = 1) const
    {
      if ((n_offset > nSize) || (n_count > (nSize - n_offset) / sizeof(T)))
        return nullptr;

      return (const T *)(pData + n_offset);
    }

    bool RvaToOffset(DWORD dw_rva, size_t &n_offset) const
    {
      for (size_t i = 0; i < nSections; i++)
      {
        const IMAGE_SECTION_HEADER &sec = pSections[i];
        DWORD dwSize = (std::max)(sec.Misc.VirtualSize, sec.SizeOfRawData);
        if ((dw_rva >= sec.VirtualAddress) && (dw_rva - sec.VirtualAddress < dwSize))
        {
          n_offset = (size_t)sec.PointerToRawData + (dw_rva - sec.VirtualAddress);
          return true;
        }
      }

      return false;
    }

    const BYTE                 *pData;
    size_t                      nSize;
    bool                        b64Bit = false;
    const IMAGE_DATA_DIRECTORY *pDirs = nullptr;
    size_t                      nDirs = 0;
    const IMAGE_SECTION_HEADER *pSections = nullptr;
    size_t                      nSections = 0;
  };


  void ReadExports(const PeFile &pe, std::vector<std::string> &v_exports)
  {
    const IMAGE_EXPORT_DIRECTORY *pExp = pe.AtRva<IMAGE_EXPORT_DIRECTORY>(pe.DirectoryRva(IMAGE_DIRECTORY_ENTRY_EXPORT));
    if (!pExp)
      return;

    const DWORD *pNames = pe.AtRva<DWORD>(pExp->AddressOfNames, pExp->NumberOfNames);
    if (!pNames)
      return;

    for (DWORD i = 0; i < pExp->NumberOfNames; i++)
    {
      std::string sName = pe.StringAtRva(pNames[i]);
      if (!sName.empty())
        v_exports.push_back(sName);
    }
  }


  void ReadImports(const PeFile &pe, std::vector<std::string> &v_imports)
  {
    DWORD dwRva = pe.DirectoryRva(IMAGE_DIRECTORY_ENTRY_IMPORT);
    if (dwRva == 0)
      return;

    const IMAGE_IMPORT_DESCRIPTOR nullDescriptor = {};
    for (;; dwRva += sizeof(IMAGE_IMPORT_DESCRIPTOR))
    {
      const IMAGE_IMPORT_DESCRIPTOR *pDesc = pe.AtRva<IMAGE_IMPORT_DESCRIPTOR>(dwRva);
      if (!pDesc || (memcmp(pDesc, &nullDescriptor, sizeof(nullDescriptor)) == 0))
        break;

      std::string sName = pe.StringAtRva(pDesc->Name);
      if (!sName.empty())
        v_imports.push_back(sName);
    }
  }


  std::string AsciiLower(std::string s_string)
  {
    std::transform(s_string.begin(), s_string.end(), s_string.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s_string;
  }
}


bool platform::InspectBinary(const std::string &s_name, BinaryInfo &info)
{
  info = BinaryInfo();

  std::wstring wsName = win32::ToWide(s_name.c_str());
  std::replace(wsName.begin(), wsName.end(), L'/', L'\\');

  // Same search as the former MapAndLoad(): SearchPath() with ".dll" as default extension
  std::wstring wsPath(MAX_PATH, L'\0');
  DWORD dwLen;
  while ((dwLen = ::SearchPathW(NULL, wsName.c_str(), L".dll", (DWORD)wsPath.size(), &wsPath[0], NULL)) >= wsPath.size())
    wsPath.resize(dwLen);
  if (dwLen == 0)
    return false;
  wsPath.resize(dwLen);

  MappedFile file;
  if (!file.Open(wsPath))
    return false;

  PeFile pe(file.pData, file.nSize);
  if (!pe.Parse())
  {
    ::SetLastError(ERROR_BAD_FORMAT);
    return false;
  }

  info.path = win32::FromWideAs(wsPath.c_str(), s_name.c_str());
  info.is64Bit = pe.Is64Bit();
  ReadExports(pe, info.exports);
  ReadImports(pe, info.imports);

  return true;
}


bool platform::IsVCRuntimeInstalled(const std::string &s_year, bool b_64bit)
{
  const char szProducts[] = "Software\\Classes\\Installer\\Products";
  REGSAM samView = Is64BitOS() ? KEY_WOW64_64KEY : 0;

  std::vector<std::string> vSubKeys;
  HKEY hKey = NULL;
  if (::RegOpenKeyExA(HKEY_LOCAL_MACHINE, szProducts, 0, KEY_READ | samView, &hKey) == ERROR_SUCCESS)
  {
    char szSubKey[256];
    DWORD dwChars = sizeof(szSubKey);
    for (DWORD dwIndex = 0; ::RegEnumKeyExA(hKey, dwIndex, szSubKey, &dwChars, NULL, NULL, NULL, NULL) == ERROR_SUCCESS; dwIndex++)
    {
      vSubKeys.push_back(szSubKey);
      dwChars = sizeof(szSubKey);
    }
    ::RegCloseKey(hKey);
  }

  bool bInstalled = false;
  for (const std::string &sSubKey : vSubKeys)
  {
    std::string sKey = std::string(szProducts) + "\\" + sSubKey;
    if (::RegOpenKeyExA(HKEY_LOCAL_MACHINE, sKey.c_str(), 0, KEY_READ | samView, &hKey) != ERROR_SUCCESS)
      continue;

    char szValue[2048] = "";
    DWORD dwBytes = sizeof(szValue) - 1;
    if (::RegQueryValueExA(hKey, "ProductName", NULL, NULL, (LPBYTE)szValue, &dwBytes) == ERROR_SUCCESS)
    {
      std::string sValue = AsciiLower(szValue);

      bool bVersion = (sValue.find(s_year) != std::string::npos);
      bool bVC = (sValue.find("visual c++") != std::string::npos);
      bool bRedist = (sValue.find("redistributable") != std::string::npos) || (sValue.find("runtime") != std::string::npos);
      bool b64BitRuntime = (sValue.find("x64") != std::string::npos);
      if (bVersion && bVC && bRedist && (b64BitRuntime == b_64bit))
        bInstalled = true;
    }
    ::RegCloseKey(hKey);
  }

  return bInstalled;
}
