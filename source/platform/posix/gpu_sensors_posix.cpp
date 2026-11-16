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


#include "platform/gpu_sensors.h"

#include <dirent.h>
#include <dlfcn.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <vector>


// NVIDIA Management Library, loaded at run time: no build dependency, and AVSMeter runs without it.
// Declarations as in nvml.h (stable C ABI). Also found in WSL2 (/usr/lib/wsl/lib).
// GPU index: NVML order, the same as nvidia-smi.
namespace
{
  typedef int nvmlReturn_t; // NVML_SUCCESS = 0
  typedef struct nvmlDevice_st *nvmlDevice_t;
  struct nvmlMemory_t { unsigned long long total, free, used; };
  struct nvmlUtilization_t { unsigned int gpu, memory; };
  struct nvmlPciInfo_t // v3
  {
    char         busIdLegacy[16];
    unsigned int domain, bus, device, pciDeviceId, pciSubSystemId;
    char         busId[32];
  };
  const int NVML_TEMPERATURE_GPU = 0;

  const char *ArchitectureName(unsigned int arch)
  {
    static const char *const names[] = { nullptr, nullptr, "Kepler", "Maxwell", "Pascal", "Volta", "Turing", "Ampere", "Ada", "Hopper", "Blackwell" };
    return (arch < sizeof(names) / sizeof(names[0])) ? names[arch] : nullptr;
  }


  // GPUs seen by the kernel (DRM), any vendor
  const unsigned int kVendorNvidia = 0x10de, kVendorAmd = 0x1002, kVendorIntel = 0x8086;
  const char *const kSupported = "Supported: NVIDIA (proprietary driver), AMD (amdgpu), Intel (i915, xe).";

  struct DrmGpu
  {
    unsigned int vendor = 0, device = 0;
    std::string  card, pci, driver, name, chip;

    std::string Describe() const
    {
      return name + " (PCI " + pci + (driver.empty() ? "" : ", driver " + driver) + ")";
    }

    // Kernel drivers with sensors here; NVIDIA goes through NVML
    bool Supported() const
    {
      return (driver == "amdgpu") || (driver == "i915") || (driver == "xe");
    }
  };

  const char *VendorName(unsigned int vendor)
  {
    switch (vendor)
    {
      case kVendorNvidia: return "NVIDIA";
      case kVendorAmd:    return "AMD";
      case kVendorIntel:  return "Intel";
    }
    return nullptr;
  }

  bool ReadToken(const std::string &s_path, std::string &s)
  {
    std::ifstream f(s_path);
    return static_cast<bool>(f >> s);
  }

  bool ReadHex(const std::string &s_path, unsigned int &value)
  {
    std::string s;
    if (!ReadToken(s_path, s))
      return false;
    char *pEnd = nullptr;
    value = (unsigned int)std::strtoul(s.c_str(), &pEnd, 16);
    return (pEnd != s.c_str()) && (*pEnd == '\0');
  }

  bool ReadUInt64(const std::string &s_path, uint64_t &value)
  {
    std::string s;
    if (!ReadToken(s_path, s))
      return false;
    char *pEnd = nullptr;
    value = std::strtoull(s.c_str(), &pEnd, 10);
    return (pEnd != s.c_str()) && (*pEnd == '\0');
  }

  std::string BaseName(const std::string &s)
  {
    size_t pos = s.find_last_of('/');
    return (pos == std::string::npos) ? s : s.substr(pos + 1);
  }

  bool StartsWith(const std::string &s, const char *psz_prefix)
  {
    return s.compare(0, std::strlen(psz_prefix), psz_prefix) == 0;
  }

  // Device line of the pci.ids database (pciutils/hwdata), e.g. "RocketLake-S GT1 [UHD Graphics 750]"
  std::string PciIdsName(unsigned int vendor, unsigned int device)
  {
    const char *const pszFiles[] = { "/usr/share/hwdata/pci.ids", "/usr/share/misc/pci.ids", "/usr/share/pci.ids" };
    char szVendor[8], szDevice[8];
    std::snprintf(szVendor, sizeof(szVendor), "%04x", vendor);
    std::snprintf(szDevice, sizeof(szDevice), "\t%04x", device);
    for (const char *pszFile : pszFiles)
    {
      std::ifstream f(pszFile);
      if (!f)
        continue;
      std::string sLine;
      bool bVendor = false;
      while (std::getline(f, sLine))
      {
        if (sLine.empty() || (sLine[0] == '#'))
          continue;
        if (sLine[0] != '\t')
        {
          if (bVendor)
            break;
          bVendor = (sLine.compare(0, 4, szVendor) == 0);
        }
        else if (bVendor && (sLine.compare(0, 5, szDevice) == 0) && (sLine.length() > 7))
          return sLine.substr(7);
      }
      return "";
    }
    return "";
  }

  std::vector<DrmGpu> DrmGpus()
  {
    std::vector<DrmGpu> gpus;
    const std::string sRoot = "/sys/class/drm/";
    DIR *pDir = opendir(sRoot.c_str());
    if (!pDir)
      return gpus;
    while (dirent *pEntry = readdir(pDir))
    {
      std::string sCard = pEntry->d_name; // "card0"; skip connectors ("card0-HDMI-A-1") and render nodes
      if (!StartsWith(sCard, "card") || (sCard.length() == 4) || (sCard.find_first_not_of("0123456789", 4) != std::string::npos))
        continue;

      std::string sDevice = sRoot + sCard + "/device";
      DrmGpu gpu;
      gpu.card = sCard;
      if (!ReadHex(sDevice + "/vendor", gpu.vendor) || !ReadHex(sDevice + "/device", gpu.device))
        continue; // not a PCI device (e.g. vgem in WSL2)

      char szPath[PATH_MAX];
      if (!realpath(sDevice.c_str(), szPath))
        continue;
      gpu.pci = BaseName(szPath);
      ssize_t len = readlink((sDevice + "/driver").c_str(), szPath, sizeof(szPath) - 1);
      if (len > 0)
        gpu.driver = BaseName(std::string(szPath, (size_t)len));

      // "Chip [Marketing name]": the bracketed part names the card, the rest is the GPU
      const char *pszVendor = VendorName(gpu.vendor);
      std::string sName = PciIdsName(gpu.vendor, gpu.device);
      size_t posOpen = sName.find('['), posClose = sName.rfind(']');
      if ((posOpen != std::string::npos) && (posClose != std::string::npos) && (posClose > posOpen + 1))
      {
        gpu.chip = sName.substr(0, posOpen);
        gpu.chip.erase(gpu.chip.find_last_not_of(' ') + 1);
        sName = sName.substr(posOpen + 1, posClose - posOpen - 1);
      }
      char sz[64];
      std::snprintf(sz, sizeof(sz), "%s GPU [%04x:%04x]", pszVendor ? pszVendor : "Unknown", gpu.vendor, gpu.device);
      gpu.name = sName.empty() ? sz : (pszVendor ? std::string(pszVendor) + " " : "") + sName;

      if (std::none_of(gpus.begin(), gpus.end(), [&](const DrmGpu &g) { return g.pci == gpu.pci; }))
        gpus.push_back(gpu);
    }
    closedir(pDir);
    std::sort(gpus.begin(), gpus.end(), [](const DrmGpu &a, const DrmGpu &b) { return a.pci < b.pci; });
    return gpus;
  }

  // "\n  Intel UHD Graphics 750 (PCI 0000:00:02.0, driver i915)" for each GPU of the vendor (0: non-NVIDIA)
  std::string DrmGpuList(const std::vector<DrmGpu> &gpus, unsigned int vendor)
  {
    std::string s;
    for (const DrmGpu &gpu : gpus)
      if (vendor ? (gpu.vendor == vendor) : (gpu.vendor != kVendorNvidia))
        s += "\n  " + gpu.Describe() + (((gpu.vendor != kVendorNvidia) && !gpu.Supported()) ? ": driver not supported" : "");
    return s;
  }


  // DRM client usage stats (/proc/self/fdinfo, kernel doc gpu/drm-usage-stats): busy time of the
  // GPU engines used by this process. i915/amdgpu: drm-engine-<e> in ns; xe: drm-cycles-<e> and
  // drm-total-cycles-<e> (GPU timestamp). Engine groups: drm-engine-capacity-<e>.
  struct EngineCounter
  {
    uint64_t     busy = 0, total = 0;
    unsigned int capacity = 1;
    bool         bCycles = false;
  };

  struct FdinfoSample
  {
    std::map<std::string, EngineCounter> engines;
    uint64_t residentBytes = 0;
  };

  uint64_t ParseSize(const std::string &s) // "123 KiB"
  {
    char *pEnd = nullptr;
    uint64_t value = std::strtoull(s.c_str(), &pEnd, 10);
    while (*pEnd == ' ')
      pEnd++;
    if (std::strncmp(pEnd, "KiB", 3) == 0)
      value *= 1024;
    else if (std::strncmp(pEnd, "MiB", 3) == 0)
      value *= 1024 * 1024;
    return value;
  }

  FdinfoSample ReadFdinfo(const std::string &s_pci)
  {
    FdinfoSample sample;
    std::set<std::string> clients;
    DIR *pDir = opendir("/proc/self/fd");
    if (!pDir)
      return sample;
    while (dirent *pEntry = readdir(pDir))
    {
      if (pEntry->d_name[0] == '.')
        continue;
      std::string sFd = pEntry->d_name;
      char szPath[PATH_MAX];
      ssize_t len = readlink(("/proc/self/fd/" + sFd).c_str(), szPath, sizeof(szPath) - 1);
      if ((len <= 0) || !StartsWith(std::string(szPath, (size_t)len), "/dev/dri/"))
        continue;

      std::ifstream f("/proc/self/fdinfo/" + sFd);
      std::map<std::string, std::string> keys;
      std::string sLine;
      while (std::getline(f, sLine))
      {
        size_t posColon = sLine.find(':');
        if (posColon == std::string::npos)
          continue;
        size_t posValue = sLine.find_first_not_of(" \t", posColon + 1);
        keys[sLine.substr(0, posColon)] = (posValue == std::string::npos) ? "" : sLine.substr(posValue);
      }

      // Same client through dup()-ed descriptors: count once
      auto itPdev = keys.find("drm-pdev");
      if ((itPdev == keys.end()) || (itPdev->second != s_pci))
        continue;
      auto itClient = keys.find("drm-client-id");
      if (!clients.insert((itClient != keys.end()) ? itClient->second : "fd" + sFd).second)
        continue;

      for (const auto &kv : keys)
      {
        const std::string &sKey = kv.first;
        uint64_t value = std::strtoull(kv.second.c_str(), nullptr, 10);
        if (StartsWith(sKey, "drm-engine-capacity-"))
          sample.engines[sKey.substr(20)].capacity = (std::max)((unsigned int)value, 1u);
        else if (StartsWith(sKey, "drm-engine-"))
          sample.engines[sKey.substr(11)].busy += value;
        else if (StartsWith(sKey, "drm-total-cycles-"))
          sample.engines[sKey.substr(17)].total = (std::max)(sample.engines[sKey.substr(17)].total, value);
        else if (StartsWith(sKey, "drm-cycles-"))
        {
          EngineCounter &engine = sample.engines[sKey.substr(11)];
          engine.busy += value;
          engine.bCycles = true;
        }
        else if (StartsWith(sKey, "drm-resident-"))
          sample.residentBytes += ParseSize(kv.second);
      }
    }
    closedir(pDir);
    return sample;
  }

  enum class EngineKind { Gpu, Vpu, Other };

  // i915: render, compute, video, video-enhance; xe: rcs, ccs, vcs, vecs; amdgpu: gfx, compute, dec, enc, jpeg, ...
  EngineKind KindOf(const std::string &s_engine)
  {
    if ((s_engine == "render") || (s_engine == "compute") || (s_engine == "gfx") || (s_engine == "rcs") || (s_engine == "ccs"))
      return EngineKind::Gpu;
    if (StartsWith(s_engine, "video") || (s_engine == "vcs") || (s_engine == "vecs") || StartsWith(s_engine, "dec") ||
      StartsWith(s_engine, "enc") || StartsWith(s_engine, "jpeg") || StartsWith(s_engine, "vcn"))
      return EngineKind::Vpu;
    return EngineKind::Other;
  }
}


struct platform::GpuSensors::Nvml
{
  void         *hLib = nullptr;
  nvmlDevice_t device = nullptr;
  bool         bInitialized = false;

  nvmlReturn_t (*Init)();
  nvmlReturn_t (*Shutdown)();
  const char  *(*ErrorString)(nvmlReturn_t);
  nvmlReturn_t (*GetCount)(unsigned int *);
  nvmlReturn_t (*GetHandleByIndex)(unsigned int, nvmlDevice_t *);
  nvmlReturn_t (*GetName)(nvmlDevice_t, char *, unsigned int);
  nvmlReturn_t (*GetDriverVersion)(char *, unsigned int);
  nvmlReturn_t (*GetCudaDriverVersion)(int *);
  nvmlReturn_t (*GetMemoryInfo)(nvmlDevice_t, nvmlMemory_t *);
  nvmlReturn_t (*GetUtilizationRates)(nvmlDevice_t, nvmlUtilization_t *);
  nvmlReturn_t (*GetEncoderUtilization)(nvmlDevice_t, unsigned int *, unsigned int *);
  nvmlReturn_t (*GetDecoderUtilization)(nvmlDevice_t, unsigned int *, unsigned int *);
  nvmlReturn_t (*GetPowerUsage)(nvmlDevice_t, unsigned int *);
  nvmlReturn_t (*GetTemperature)(nvmlDevice_t, int, unsigned int *);
  nvmlReturn_t (*GetCudaComputeCapability)(nvmlDevice_t, int *, int *);
  nvmlReturn_t (*GetArchitecture)(nvmlDevice_t, unsigned int *); // driver 450+
  nvmlReturn_t (*GetPciInfo)(nvmlDevice_t, nvmlPciInfo_t *);
  unsigned int index = 0;

  // "0000:01:00.0" (NVML: 8-digit domain)
  std::string PciBusId(nvmlDevice_t dev) const
  {
    nvmlPciInfo_t pci = {};
    if (!GetPciInfo || (GetPciInfo(dev, &pci) != 0))
      return "";
    pci.busId[sizeof(pci.busId) - 1] = '\0';
    std::string s = pci.busId;
    if ((s.length() == 16) && (s.compare(0, 4, "0000") == 0))
      s.erase(0, 4);
    return s;
  }

  std::string DeviceName(nvmlDevice_t dev) const
  {
    char sz[96];
    return (GetName(dev, sz, sizeof(sz)) == 0) ? sz : "n/a";
  }

  template <class F>
  bool Load(F &f, const char *psz_name, bool b_required = true)
  {
    f = reinterpret_cast<F>(dlsym(hLib, psz_name));
    return (f != nullptr) || !b_required;
  }

  std::string Error(const char *psz_what, nvmlReturn_t ret) const
  {
    const char *psz = ErrorString ? ErrorString(ret) : nullptr;
    char sz[32];
    std::snprintf(sz, sizeof(sz), "error %d", ret);
    return std::string(psz_what) + ": " + (psz ? psz : sz);
  }
};


// AMD (amdgpu) and Intel (i915, xe) through sysfs, hwmon and DRM fdinfo
struct platform::GpuSensors::Drm
{
  DrmGpu      gpu;
  std::string sDevice;      // /sys/class/drm/cardN/device
  std::string sPowerFile;   // hwmon power (uW) or energy counter (uJ)
  bool        bEnergy = false;
  bool        bAmd = false;

  bool        bPrev = false;
  std::chrono::steady_clock::time_point tPrev;
  std::map<std::string, EngineCounter> enginesPrev;
  uint64_t    uEnergyPrev = 0;
  double      dPowerPrev = 0.0;

  void FindPowerFile()
  {
    uint64_t value = 0;
    std::string sHwmon = sDevice + "/hwmon/";
    if (DIR *pDir = opendir(sHwmon.c_str()))
    {
      while (dirent *pEntry = readdir(pDir))
      {
        if (!StartsWith(pEntry->d_name, "hwmon"))
          continue;
        std::string sDir = sHwmon + pEntry->d_name + "/";
        // amdgpu: power1_average (or power1_input, newer GPUs); i915/xe dGPU: energy1_input
        for (const char *pszFile : { "power1_average", "power1_input", "energy1_input" })
          if (sPowerFile.empty() && ReadUInt64(sDir + pszFile, value))
          {
            sPowerFile = sDir + pszFile;
            bEnergy = StartsWith(pszFile, "energy");
          }
      }
      closedir(pDir);
    }

    // Intel iGPU: RAPL "uncore" (graphics) domain; usually readable only by root
    if (sPowerFile.empty() && (gpu.vendor == kVendorIntel))
    {
      const std::string sRapl = "/sys/class/powercap/";
      if (DIR *pDir = opendir(sRapl.c_str()))
      {
        while (dirent *pEntry = readdir(pDir))
        {
          std::string sDir = sRapl + pEntry->d_name + "/", sName;
          if (sPowerFile.empty() && StartsWith(pEntry->d_name, "intel-rapl:") && ReadToken(sDir + "name", sName) &&
            (sName == "uncore") && ReadUInt64(sDir + "energy_uj", value))
          {
            sPowerFile = sDir + "energy_uj";
            bEnergy = true;
          }
        }
        closedir(pDir);
      }
    }
  }
};


platform::GpuSensors::GpuSensors()
{
  sensors = Sensors();
  data = Data();
  sError = "";
  bInitialized = false;
  pMapAddr = nullptr;
  hMapFile = nullptr;
}


platform::GpuSensors::~GpuSensors()
{
  Release();
}


void platform::GpuSensors::Init(const std::string &s_select)
{
  Release();
  sError = "";
  sNote = "";

  if (s_select == "amd")
  {
    InitDrm(kVendorAmd);
    return;
  }
  if (s_select == "intel")
  {
    InitDrm(kVendorIntel);
    return;
  }

  unsigned int uiIndex = 0;
  if (!s_select.empty() && (s_select != "nvidia"))
  {
    if ((s_select.length() > 2) || (s_select.find_first_not_of("0123456789") != std::string::npos))
    {
      sError = "\"-gpu=" + s_select + "\": unknown GPU, use \"nvidia\", \"amd\", \"intel\" or an NVIDIA GPU index (0, 1, ...)";
      return;
    }
    uiIndex = (unsigned int)std::stoul(s_select);
  }

  if (InitNvml(uiIndex, s_select) || !s_select.empty())
    return;

  // Default: without a usable NVIDIA GPU, the first AMD or Intel GPU
  const std::vector<DrmGpu> drmGpus = DrmGpus();
  for (unsigned int uiVendor : { kVendorAmd, kVendorIntel })
    if (std::any_of(drmGpus.begin(), drmGpus.end(), [&](const DrmGpu &g) { return (g.vendor == uiVendor) && g.Supported(); }))
    {
      sError = "";
      if (InitDrm(uiVendor))
      {
        // Supported GPUs of the same vendor are in the "Several" note already
        std::string sOthers;
        for (const DrmGpu &gpu : drmGpus)
          if ((gpu.vendor != kVendorNvidia) && !((gpu.vendor == uiVendor) && gpu.Supported()))
            sOthers += DrmGpuList({ gpu }, gpu.vendor);
        if (!sOthers.empty())
          sNote += (sNote.empty() ? "" : "\n") + std::string("Other GPUs, not measured:") + sOthers;
      }
      return;
    }
}


bool platform::GpuSensors::InitNvml(unsigned int ui_index, const std::string &s_select)
{
  const std::vector<DrmGpu> drmGpus = DrmGpus();
  const std::string sOtherGpus = DrmGpuList(drmGpus, 0);

  const char *const pszLibs[] = { "libnvidia-ml.so.1", "/usr/lib/wsl/lib/libnvidia-ml.so.1" };
  void *hLib = nullptr;
  for (const char *pszLib : pszLibs)
    if ((hLib = dlopen(pszLib, RTLD_NOW | RTLD_LOCAL)) != nullptr)
      break;

  if (!hLib)
  {
    std::string sNvidia = DrmGpuList(drmGpus, kVendorNvidia);
    if (!sNvidia.empty())
      sError = "NVIDIA GPU found:" + sNvidia + "\n"
        "but the NVIDIA Management Library (libnvidia-ml.so.1) is missing, GPU sensors need the proprietary NVIDIA driver.";
    else if (!sOtherGpus.empty())
      sError = "No NVIDIA GPU found, other GPU:" + sOtherGpus + "\n" + kSupported;
    else
      sError = "Cannot load the NVIDIA Management Library (libnvidia-ml.so.1).\n"
        "GPU sensors (-gpu) need an NVIDIA GPU and its proprietary driver, or an AMD or Intel GPU.";
    return false;
  }

  pNvml = new Nvml;
  Nvml &n = *pNvml;
  n.hLib = hLib;

  bool bOk = n.Load(n.Init, "nvmlInit_v2") && n.Load(n.Shutdown, "nvmlShutdown") &&
    n.Load(n.ErrorString, "nvmlErrorString") && n.Load(n.GetCount, "nvmlDeviceGetCount_v2") &&
    n.Load(n.GetHandleByIndex, "nvmlDeviceGetHandleByIndex_v2") && n.Load(n.GetName, "nvmlDeviceGetName") &&
    n.Load(n.GetDriverVersion, "nvmlSystemGetDriverVersion") && n.Load(n.GetMemoryInfo, "nvmlDeviceGetMemoryInfo") &&
    n.Load(n.GetUtilizationRates, "nvmlDeviceGetUtilizationRates") &&
    n.Load(n.GetCudaDriverVersion, "nvmlSystemGetCudaDriverVersion_v2", false) &&
    n.Load(n.GetEncoderUtilization, "nvmlDeviceGetEncoderUtilization", false) &&
    n.Load(n.GetDecoderUtilization, "nvmlDeviceGetDecoderUtilization", false) &&
    n.Load(n.GetPowerUsage, "nvmlDeviceGetPowerUsage", false) &&
    n.Load(n.GetTemperature, "nvmlDeviceGetTemperature", false) &&
    n.Load(n.GetCudaComputeCapability, "nvmlDeviceGetCudaComputeCapability", false) &&
    n.Load(n.GetArchitecture, "nvmlDeviceGetArchitecture", false) &&
    n.Load(n.GetPciInfo, "nvmlDeviceGetPciInfo_v3", false);
  if (!bOk)
  {
    sError = "The NVIDIA Management Library (libnvidia-ml.so.1) is too old, a function is missing";
    Release();
    return false;
  }

  nvmlReturn_t ret = n.Init();
  if (ret != 0)
  {
    sError = n.Error("Cannot initialize the NVIDIA Management Library", ret);
    Release();
    return false;
  }
  n.bInitialized = true;

  unsigned int uiCount = 0;
  ret = n.GetCount(&uiCount);
  std::string sList;
  for (unsigned int i = 0; (ret == 0) && (i < uiCount); i++)
  {
    nvmlDevice_t device = nullptr;
    std::string sName = "n/a", sPci;
    if (n.GetHandleByIndex(i, &device) == 0)
    {
      sName = n.DeviceName(device);
      sPci = n.PciBusId(device);
    }
    sList += "\n  " + std::to_string(i) + ": " + sName + (sPci.empty() ? "" : " (PCI " + sPci + ")");
  }

  if ((ret == 0) && (uiCount == 0))
    sError = sOtherGpus.empty() ? "No NVIDIA GPU found" : "No NVIDIA GPU found, other GPU:" + sOtherGpus + "\n" + kSupported;
  else if ((ret == 0) && (ui_index >= uiCount))
    sError = "No NVIDIA GPU with index " + std::to_string(ui_index) + ", available:" + sList;
  else if (ret == 0)
    ret = n.GetHandleByIndex(ui_index, &n.device);
  if ((ret != 0) || !sError.empty())
  {
    if (sError == "")
      sError = n.Error("Cannot access the NVIDIA GPU", ret);
    Release();
    return false;
  }
  n.index = ui_index;

  if ((uiCount > 1) && (s_select.empty() || (s_select == "nvidia")))
    sNote = "Several NVIDIA GPUs, measuring GPU 0 (select one with -gpu=n):" + sList;
  if (s_select.empty() && !sOtherGpus.empty())
    sNote += (sNote.empty() ? "" : "\n") + std::string("Other GPUs, not measured (select with -gpu=amd or -gpu=intel):") + sOtherGpus;

  bInitialized = true;
  ReadData();
  return true;
}


bool platform::GpuSensors::InitDrm(unsigned int ui_vendor)
{
  const std::vector<DrmGpu> drmGpus = DrmGpus();
  const char *pszVendor = VendorName(ui_vendor);
  std::vector<DrmGpu> candidates;
  for (const DrmGpu &gpu : drmGpus)
    if ((gpu.vendor == ui_vendor) && gpu.Supported())
      candidates.push_back(gpu);

  std::string sList = DrmGpuList(drmGpus, ui_vendor);
  if (candidates.empty())
  {
    if (sList.empty())
      sError = std::string("No ") + pszVendor + " GPU found";
    else
      sError = std::string(pszVendor) + " GPU found:" + sList + "\nbut its kernel driver is not supported. " + kSupported;
    return false;
  }
  if (candidates.size() > 1)
  {
    sNote = std::string("Several ") + pszVendor + " GPUs, measuring the first:";
    for (const DrmGpu &gpu : candidates)
      sNote += "\n  " + gpu.Describe();
  }

  pDrm = new Drm;
  pDrm->gpu = candidates[0];
  pDrm->sDevice = "/sys/class/drm/" + pDrm->gpu.card + "/device";
  pDrm->bAmd = (ui_vendor == kVendorAmd);

  bInitialized = true;
  ReadData();
  return true;
}


void platform::GpuSensors::Release()
{
  if (pNvml)
  {
    if (pNvml->bInitialized)
      pNvml->Shutdown();
    if (pNvml->hLib)
      dlclose(pNvml->hLib);
    delete pNvml;
    pNvml = nullptr;
  }
  delete pDrm;
  pDrm = nullptr;
  bInitialized = false;
}


void platform::GpuSensors::ReadData()
{
  data = Data();
  if (!bInitialized)
    return;

  char sz[256];

  if (pDrm)
  {
    Drm &d = *pDrm;
    data.CardName = d.gpu.Describe();
    data.GPUName = d.gpu.chip.empty() ? "n/a" : d.gpu.chip;

    uint64_t value = 0;
    if (ReadUInt64(d.sDevice + "/mem_info_vram_total", value))
      data.MemSize = std::to_string(value / (1024 * 1024));
    else
      data.MemSize = ((d.gpu.vendor == kVendorIntel) && (d.gpu.pci == "0000:00:02.0")) ? "shared (system memory)" : "n/a";

    // In-tree drivers have no module version: the kernel release tells it
    std::string sVersion;
    struct utsname uts;
    if (ReadToken("/sys/module/" + d.gpu.driver + "/version", sVersion))
      data.DriverVersion = d.gpu.driver + " " + sVersion;
    else
      data.DriverVersion = d.gpu.driver + ((uname(&uts) == 0) ? std::string(" (Linux ") + uts.release + ")" : "");

    data.NVVPU = true;
    data.GeneralMem = d.bAmd ? ReadUInt64(d.sDevice + "/mem_info_vram_used", value) : true;
    d.FindPowerFile();
    data.Power = !d.sPowerFile.empty();
    return;
  }

  Nvml &n = *pNvml;

  // Index and PCI bus ID tell identical cards apart (the index is NVML's, not CUDA's order)
  std::string sPci = n.PciBusId(n.device);
  data.CardName = n.DeviceName(n.device) + " (GPU " + std::to_string(n.index) + (sPci.empty() ? "" : ", PCI " + sPci) + ")";
  data.DriverVersion = (n.GetDriverVersion(sz, sizeof(sz)) == 0) ? sz : "n/a";

  int iMajor = 0, iMinor = 0;
  bool bCompute = n.GetCudaComputeCapability && (n.GetCudaComputeCapability(n.device, &iMajor, &iMinor) == 0);
  unsigned int uiArch = 0;
  const char *pszArch = (n.GetArchitecture && (n.GetArchitecture(n.device, &uiArch) == 0)) ? ArchitectureName(uiArch) : nullptr;
  if (pszArch && bCompute)
    std::snprintf(sz, sizeof(sz), "%s (compute capability %d.%d)", pszArch, iMajor, iMinor);
  else if (bCompute)
    std::snprintf(sz, sizeof(sz), "compute capability %d.%d", iMajor, iMinor);
  else
    std::snprintf(sz, sizeof(sz), "%s", pszArch ? pszArch : "n/a");
  data.GPUName = sz;

  nvmlMemory_t mem = {};
  if (n.GetMemoryInfo(n.device, &mem) == 0)
  {
    data.MemSize = std::to_string(mem.total / (1024 * 1024));
    data.GeneralMem = true;
  }
  else
    data.MemSize = "n/a";

  int iCuda = 0;
  if (n.GetCudaDriverVersion && (n.GetCudaDriverVersion(&iCuda) == 0) && (iCuda > 0))
  {
    std::snprintf(sz, sizeof(sz), "%d.%d", iCuda / 1000, (iCuda % 1000) / 10);
    data.CudaVersion = sz;
  }

  // "Video Engine Load" in GPU-Z: NVENC/NVDEC. NVML reports them separately.
  unsigned int uiUtil = 0, uiPeriod = 0;
  data.NVVPU = (n.GetEncoderUtilization && (n.GetEncoderUtilization(n.device, &uiUtil, &uiPeriod) == 0)) ||
    (n.GetDecoderUtilization && (n.GetDecoderUtilization(n.device, &uiUtil, &uiPeriod) == 0));

  unsigned int uiPower = 0;
  data.Power = n.GetPowerUsage && (n.GetPowerUsage(n.device, &uiPower) == 0);
}


void platform::GpuSensors::ReadSensors()
{
  sensors = Sensors();
  if (!bInitialized)
  {
    sensors.ReadError = true;
    return;
  }

  if (pDrm)
  {
    Drm &d = *pDrm;
    auto tNow = std::chrono::steady_clock::now();
    double dSeconds = d.bPrev ? std::chrono::duration<double>(tNow - d.tPrev).count() : 0.0;

    // Engine load of this process; a counter going backwards keeps its previous value (spec)
    unsigned int uiGpu = 0, uiVpu = 0;
    FdinfoSample sample = ReadFdinfo(d.gpu.pci);
    for (const auto &kv : sample.engines)
    {
      const EngineCounter &cur = kv.second;
      auto itPrev = d.enginesPrev.find(kv.first);
      double dLoad = 0.0;
      if (d.bPrev && (itPrev != d.enginesPrev.end()) && (cur.busy >= itPrev->second.busy))
      {
        double dBusy = (double)(cur.busy - itPrev->second.busy);
        if (cur.bCycles)
          dLoad = (cur.total > itPrev->second.total) ? dBusy / (double)(cur.total - itPrev->second.total) : 0.0;
        else if (dSeconds > 0.0)
          dLoad = dBusy / (dSeconds * 1e9);
      }
      unsigned int uiLoad = (unsigned int)(std::min)(std::lround(dLoad * 100.0 / cur.capacity), 100L);
      EngineKind kind = KindOf(kv.first);
      if (kind == EngineKind::Gpu)
        uiGpu = (std::max)(uiGpu, uiLoad);
      else if (kind == EngineKind::Vpu)
        uiVpu = (std::max)(uiVpu, uiLoad);
      if ((itPrev == d.enginesPrev.end()) || (cur.busy >= itPrev->second.busy))
        d.enginesPrev[kv.first] = cur;
    }

    // amdgpu: load of the whole GPU, not only this process
    uint64_t value = 0;
    if (d.bAmd && ReadUInt64(d.sDevice + "/gpu_busy_percent", value))
      uiGpu = (unsigned int)(std::min)(value, (uint64_t)100);
    sensors.GPULoad = (uint8_t)uiGpu;
    sensors.VPULoad = (uint8_t)uiVpu;

    if (data.GeneralMem)
    {
      if (d.bAmd && ReadUInt64(d.sDevice + "/mem_info_vram_used", value))
        sensors.MemoryUsedGeneral = (uint32_t)((value + 512 * 1024) / (1024 * 1024));
      else if (!d.bAmd)
        sensors.MemoryUsedGeneral = (uint32_t)((sample.residentBytes + 512 * 1024) / (1024 * 1024));
    }

    if (data.Power && ReadUInt64(d.sPowerFile, value))
    {
      if (!d.bEnergy)
        sensors.PowerConsumption = value / 1e6;
      else
      {
        // Energy counter: average power since the previous reading (a wrap-around keeps the last value)
        if (d.bPrev && (value >= d.uEnergyPrev) && (dSeconds > 0.0))
          d.dPowerPrev = (value - d.uEnergyPrev) / 1e6 / dSeconds;
        d.uEnergyPrev = value;
        sensors.PowerConsumption = d.dPowerPrev;
      }
    }

    d.tPrev = tNow;
    d.bPrev = true;
    return;
  }

  Nvml &n = *pNvml;

  nvmlUtilization_t util = {};
  if (n.GetUtilizationRates(n.device, &util) != 0)
  {
    sensors.ReadError = true;
    return;
  }
  sensors.GPULoad = (uint8_t)(std::min)(util.gpu, 100u);

  if (data.NVVPU)
  {
    unsigned int uiEnc = 0, uiDec = 0, uiPeriod = 0;
    if (n.GetEncoderUtilization && (n.GetEncoderUtilization(n.device, &uiEnc, &uiPeriod) != 0))
      uiEnc = 0;
    if (n.GetDecoderUtilization && (n.GetDecoderUtilization(n.device, &uiDec, &uiPeriod) != 0))
      uiDec = 0;
    sensors.VPULoad = (uint8_t)(std::min)((std::max)(uiEnc, uiDec), 100u);
  }

  nvmlMemory_t mem = {};
  if (data.GeneralMem && (n.GetMemoryInfo(n.device, &mem) == 0))
    sensors.MemoryUsedGeneral = (uint32_t)((mem.used + 512 * 1024) / (1024 * 1024));

  unsigned int uiValue = 0;
  if (data.Power && (n.GetPowerUsage(n.device, &uiValue) == 0))
    sensors.PowerConsumption = uiValue / 1000.0;

  if (n.GetTemperature && (n.GetTemperature(n.device, NVML_TEMPERATURE_GPU, &uiValue) == 0))
    sensors.GPUTemp = (uint8_t)(std::min)(uiValue, 255u);
}
