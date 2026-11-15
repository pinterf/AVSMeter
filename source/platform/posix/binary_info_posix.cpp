/*
  This file is part of AVSMeter, (c) 2026 pinterf.

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
#include "platform/posix/last_error_posix.h"

#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>

#include <elf.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>


namespace
{
  class MappedFile
  {
  public:
    ~MappedFile()
    {
      if (pData)
        munmap((void *)pData, nSize);
    }

    bool Open(const std::string &s_path)
    {
      int fd = open(s_path.c_str(), O_RDONLY | O_CLOEXEC);
      if (fd < 0)
        return false;

      struct stat st;
      bool bOk = false;
      if (fstat(fd, &st) == 0)
      {
        if (!S_ISREG(st.st_mode) || st.st_size == 0)
          errno = ENOEXEC;
        else
        {
          void *p = mmap(nullptr, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
          bOk = (p != MAP_FAILED);
          if (bOk)
          {
            pData = (const uint8_t *)p;
            nSize = (size_t)st.st_size;
          }
        }
      }

      int iErrno = errno;
      close(fd);
      errno = iErrno;
      return bOk;
    }

    const uint8_t *pData = nullptr;
    size_t         nSize = 0;
  };


  // What the loader compares before accepting a library: class, byte order, machine.
  struct ElfId
  {
    unsigned char cls = 0;
    unsigned char data = 0;
    uint16_t      machine = 0;

    bool operator==(const ElfId &o) const { return cls == o.cls && data == o.data && machine == o.machine; }
  };


  bool ReadElfId(const uint8_t *p, size_t n, ElfId &id)
  {
    if (n < sizeof(Elf32_Ehdr) || memcmp(p, ELFMAG, SELFMAG) != 0)
      return false;

    id.cls = p[EI_CLASS];
    id.data = p[EI_DATA];
    if ((id.cls != ELFCLASS32 && id.cls != ELFCLASS64) || (id.data != ELFDATA2LSB && id.data != ELFDATA2MSB))
      return false;
    if (id.cls == ELFCLASS64 && n < sizeof(Elf64_Ehdr))
      return false;

    // e_machine has the same offset in both classes
    memcpy(&id.machine, p + offsetof(Elf32_Ehdr, e_machine), sizeof(id.machine));
    return true;
  }


  bool ReadElfIdFromFile(const std::string &s_path, ElfId &id)
  {
    uint8_t header[sizeof(Elf64_Ehdr)] = {};
    std::ifstream file(s_path, std::ios::binary);
    if (!file)
      return false;

    file.read((char *)header, sizeof(header));
    return ReadElfId(header, (size_t)file.gcount(), id);
  }


  const ElfId &OwnElfId()
  {
    static const ElfId id = []
    {
      ElfId own;
      ReadElfIdFromFile("/proc/self/exe", own);
      return own;
    }();
    return id;
  }


  bool IsNativeByteOrder(const ElfId &id)
  {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return id.data == ELFDATA2LSB;
#else
    return id.data == ELFDATA2MSB;
#endif
  }


  template <class Ehdr, class Shdr, class Sym, class Dyn>
  bool ParseElf(const uint8_t *p, size_t n, platform::BinaryInfo &info)
  {
    Ehdr eh;
    memcpy(&eh, p, sizeof(eh));
    if (eh.e_shoff == 0 || eh.e_shnum == 0) // no section headers: nothing to list
      return true;
    if (eh.e_shentsize != sizeof(Shdr) || eh.e_shoff > n || eh.e_shnum > (n - eh.e_shoff) / sizeof(Shdr))
      return false;

    auto Section = [&](size_t i)
    {
      Shdr s;
      memcpy(&s, p + eh.e_shoff + i * sizeof(Shdr), sizeof(s));
      return s;
    };
    auto InFile = [&](const Shdr &s)
    {
      return s.sh_type != SHT_NOBITS && s.sh_offset <= n && s.sh_size <= n - s.sh_offset;
    };
    auto String = [&](const Shdr &strtab, uint64_t off)
    {
      if (off >= strtab.sh_size)
        return std::string();
      const char *psz = (const char *)p + strtab.sh_offset + off;
      return std::string(psz, strnlen(psz, (size_t)(strtab.sh_size - off)));
    };

    for (size_t i = 0; i < eh.e_shnum; i++)
    {
      Shdr sec = Section(i);
      if ((sec.sh_type != SHT_DYNSYM && sec.sh_type != SHT_DYNAMIC) || !InFile(sec) || sec.sh_link >= eh.e_shnum)
        continue;

      Shdr strtab = Section(sec.sh_link);
      if (!InFile(strtab))
        continue;

      const uint8_t *pEntries = p + sec.sh_offset;
      if (sec.sh_type == SHT_DYNSYM)
      {
        size_t nSyms = (size_t)(sec.sh_size / sizeof(Sym));
        for (size_t j = 1; j < nSyms; j++) // entry 0 is the undefined symbol
        {
          Sym sym;
          memcpy(&sym, pEntries + j * sizeof(Sym), sizeof(sym));
          unsigned bind = sym.st_info >> 4;
          if (sym.st_shndx == SHN_UNDEF || (bind != STB_GLOBAL && bind != STB_WEAK && bind != STB_GNU_UNIQUE))
            continue;

          std::string sName = String(strtab, sym.st_name);
          if (!sName.empty())
            info.exports.push_back(sName);
        }
      }
      else
      {
        size_t nDyns = (size_t)(sec.sh_size / sizeof(Dyn));
        for (size_t j = 0; j < nDyns; j++)
        {
          Dyn dyn;
          memcpy(&dyn, pEntries + j * sizeof(Dyn), sizeof(dyn));
          if (dyn.d_tag == DT_NULL)
            break;
          if (dyn.d_tag == DT_NEEDED)
          {
            std::string sName = String(strtab, dyn.d_un.d_val);
            if (!sName.empty())
              info.imports.push_back(sName);
          }
        }
      }
    }

    return true;
  }


  // Paths for s_name in /etc/ld.so.cache (glibc "1.1" format, optionally after the old one), in cache order.
  std::vector<std::string> LdCacheLookup(const std::string &s_name)
  {
    std::vector<std::string> vPaths;
    MappedFile cache;
    if (!cache.Open("/etc/ld.so.cache"))
      return vPaths;

    static const char szMagic[] = "glibc-ld.so.cache1.1";
    const size_t nMagic = sizeof(szMagic) - 1;
    const size_t nHeader = 48;   // magic+version, nlibs, len_strings, flags, padding, extension_offset, unused[3]
    const size_t nEntry = 24;    // flags, key, value, osversion, hwcap
    const uint8_t *p = cache.pData;
    size_t n = cache.nSize;

    size_t nStart = 0;
    static const char szOldMagic[] = "ld.so-1.7.0";
    if (n >= 16 && memcmp(p, szOldMagic, sizeof(szOldMagic) - 1) == 0)
    {
      uint32_t nOldLibs;
      memcpy(&nOldLibs, p + 12, sizeof(nOldLibs));
      nStart = (16 + (size_t)nOldLibs * 12 + 7) & ~(size_t)7;
    }
    if (nStart > n || n - nStart < nHeader || memcmp(p + nStart, szMagic, nMagic) != 0)
      return vPaths;

    const uint8_t *pCache = p + nStart;
    size_t nCache = n - nStart;
    uint32_t nLibs;
    memcpy(&nLibs, pCache + 20, sizeof(nLibs));
    if (nLibs > (nCache - nHeader) / nEntry)
      return vPaths;

    auto String = [&](uint32_t off)
    {
      if (off >= nCache)
        return std::string();
      const char *psz = (const char *)pCache + off;
      return std::string(psz, strnlen(psz, nCache - off));
    };

    for (uint32_t i = 0; i < nLibs; i++)
    {
      uint32_t key, value;
      memcpy(&key, pCache + nHeader + (size_t)i * nEntry + 4, sizeof(key));
      memcpy(&value, pCache + nHeader + (size_t)i * nEntry + 8, sizeof(value));
      if (String(key) == s_name)
        vPaths.push_back(String(value));
    }

    return vPaths;
  }


  // Like dlopen() for a name without '/': LD_LIBRARY_PATH, ld.so.cache, default directories.
  // The executable's own RPATH/RUNPATH is not considered (AVSMeter has none).
  // Incompatible files are skipped like the loader does; if only such exist, the first one is
  // returned so the caller can report the bitness mismatch.
  std::string FindLibrary(const std::string &s_name)
  {
    if (s_name.find('/') != std::string::npos)
      return (access(s_name.c_str(), F_OK) == 0) ? s_name : std::string();

    std::vector<std::string> vCandidates;
    if (const char *pszEnv = getenv("LD_LIBRARY_PATH"))
    {
      std::string sDirs = pszEnv;
      size_t nPos = 0;
      for (;;)
      {
        size_t nEnd = sDirs.find_first_of(":;", nPos);
        std::string sDir = sDirs.substr(nPos, nEnd - nPos);
        vCandidates.push_back((sDir.empty() ? std::string(".") : sDir) + "/" + s_name);
        if (nEnd == std::string::npos)
          break;
        nPos = nEnd + 1;
      }
    }

    for (const std::string &sPath : LdCacheLookup(s_name))
      vCandidates.push_back(sPath);

    for (const char *pszDir : { "/lib64", "/usr/lib64", "/lib", "/usr/lib" })
      vCandidates.push_back(std::string(pszDir) + "/" + s_name);

    std::string sFallback;
    for (const std::string &sPath : vCandidates)
    {
      ElfId id;
      if (!ReadElfIdFromFile(sPath, id))
        continue;
      if (id == OwnElfId())
        return sPath;
      if (sFallback.empty())
        sFallback = sPath;
    }

    return sFallback;
  }
}


bool platform::InspectBinary(const std::string &s_name, BinaryInfo &info)
{
  info = BinaryInfo();
  posix::SetLastDlError("");

  std::string sPath = FindLibrary(s_name);
  if (sPath.empty())
  {
    errno = ENOENT;
    return false;
  }

  char szResolved[PATH_MAX];
  if (realpath(sPath.c_str(), szResolved))
    sPath = szResolved;

  MappedFile file;
  if (!file.Open(sPath))
    return false;

  ElfId id;
  if (!ReadElfId(file.pData, file.nSize, id) || !IsNativeByteOrder(id))
  {
    errno = ENOEXEC;
    return false;
  }

  bool bParsed = (id.cls == ELFCLASS64)
    ? ParseElf<Elf64_Ehdr, Elf64_Shdr, Elf64_Sym, Elf64_Dyn>(file.pData, file.nSize, info)
    : ParseElf<Elf32_Ehdr, Elf32_Shdr, Elf32_Sym, Elf32_Dyn>(file.pData, file.nSize, info);
  if (!bParsed)
  {
    info = BinaryInfo();
    errno = ENOEXEC;
    return false;
  }

  info.path = sPath;
  info.is64Bit = (id.cls == ELFCLASS64);
  return true;
}


// Windows only: the Visual C++ 2005/2008 runtimes are never imported by ELF binaries.
bool platform::IsVCRuntimeInstalled(const std::string &s_year, bool b_64bit)
{
  (void)s_year;
  (void)b_64bit;
  return false;
}
