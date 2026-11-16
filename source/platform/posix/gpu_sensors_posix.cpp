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
#include <unistd.h>

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <fstream>
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


  // GPUs seen by the kernel (DRM), any vendor: names the GPUs without sensor support yet
  const unsigned int kVendorNvidia = 0x10de, kVendorAmd = 0x1002, kVendorIntel = 0x8086;

  struct DrmGpu
  {
    unsigned int vendor = 0, device = 0;
    std::string  pci, driver, name;

    std::string Describe() const
    {
      return name + " (PCI " + pci + (driver.empty() ? "" : ", driver " + driver) + ")";
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

  bool ReadHex(const std::string &s_path, unsigned int &value)
  {
    std::ifstream f(s_path);
    std::string s;
    if (!(f >> s))
      return false;
    char *pEnd = nullptr;
    unsigned long ul = std::strtoul(s.c_str(), &pEnd, 16);
    value = (unsigned int)ul;
    return pEnd && (*pEnd == '\0') && (pEnd != s.c_str());
  }

  std::string BaseName(const std::string &s)
  {
    size_t pos = s.find_last_of('/');
    return (pos == std::string::npos) ? s : s.substr(pos + 1);
  }

  // Device name from the pci.ids database (pciutils/hwdata), "[...]" marketing name preferred
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
        {
          std::string sName = sLine.substr(7);
          size_t posOpen = sName.find('['), posClose = sName.rfind(']');
          if ((posOpen != std::string::npos) && (posClose != std::string::npos) && (posClose > posOpen + 1))
            sName = sName.substr(posOpen + 1, posClose - posOpen - 1);
          return sName;
        }
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
      if ((sCard.compare(0, 4, "card") != 0) || (sCard.length() == 4) || (sCard.find_first_not_of("0123456789", 4) != std::string::npos))
        continue;

      std::string sDevice = sRoot + sCard + "/device";
      DrmGpu gpu;
      if (!ReadHex(sDevice + "/vendor", gpu.vendor) || !ReadHex(sDevice + "/device", gpu.device))
        continue; // not a PCI device (e.g. vgem in WSL2)

      char szPath[PATH_MAX];
      if (!realpath(sDevice.c_str(), szPath))
        continue;
      gpu.pci = BaseName(szPath);
      ssize_t len = readlink((sDevice + "/driver").c_str(), szPath, sizeof(szPath) - 1);
      if (len > 0)
        gpu.driver = BaseName(std::string(szPath, (size_t)len));

      const char *pszVendor = VendorName(gpu.vendor);
      std::string sName = PciIdsName(gpu.vendor, gpu.device);
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
        s += "\n  " + gpu.Describe();
    return s;
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

  const std::vector<DrmGpu> drmGpus = DrmGpus();
  const std::string sOtherGpus = DrmGpuList(drmGpus, 0);
  const std::string sNotSupported = "Sensors of Intel and AMD GPUs are not supported on Linux yet, only NVIDIA.";

  unsigned int uiIndex = 0;
  if ((s_select == "intel") || (s_select == "amd"))
  {
    const char *pszVendor = (s_select == "intel") ? "Intel" : "AMD";
    std::string sList = DrmGpuList(drmGpus, (s_select == "intel") ? kVendorIntel : kVendorAmd);
    sError = sList.empty() ? std::string("No ") + pszVendor + " GPU found.\n" + sNotSupported
      : std::string(pszVendor) + " GPU found:" + sList + "\n" + sNotSupported;
    return;
  }
  else if (!s_select.empty() && (s_select != "nvidia"))
  {
    if ((s_select.length() > 2) || (s_select.find_first_not_of("0123456789") != std::string::npos))
    {
      sError = "\"-gpu=" + s_select + "\": unknown GPU, use \"nvidia\" or a GPU index (0, 1, ...)";
      return;
    }
    uiIndex = (unsigned int)std::stoul(s_select);
  }

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
      sError = "GPU found:" + sOtherGpus + "\n" + sNotSupported;
    else
      sError = "Cannot load the NVIDIA Management Library (libnvidia-ml.so.1).\n"
        "GPU sensors (-gpu) need an NVIDIA GPU and its proprietary driver.";
    return;
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
    return;
  }

  nvmlReturn_t ret = n.Init();
  if (ret != 0)
  {
    sError = n.Error("Cannot initialize the NVIDIA Management Library", ret);
    Release();
    return;
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
    sError = sOtherGpus.empty() ? "No NVIDIA GPU found" : "No NVIDIA GPU found, other GPU:" + sOtherGpus + "\n" + sNotSupported;
  else if ((ret == 0) && (uiIndex >= uiCount))
    sError = "No NVIDIA GPU with index " + std::to_string(uiIndex) + ", available:" + sList;
  else if (ret == 0)
    ret = n.GetHandleByIndex(uiIndex, &n.device);
  if ((ret != 0) || !sError.empty())
  {
    if (sError == "")
      sError = n.Error("Cannot access the NVIDIA GPU", ret);
    Release();
    return;
  }
  n.index = uiIndex;

  if ((uiCount > 1) && (s_select.empty() || (s_select == "nvidia")))
    sNote = "Several NVIDIA GPUs, measuring GPU 0 (select one with -gpu=n):" + sList;
  if (s_select.empty() && !sOtherGpus.empty())
    sNote += (sNote.empty() ? "" : "\n") + std::string("Not measured (Intel/AMD not supported on Linux yet):") + sOtherGpus;

  bInitialized = true;
  ReadData();
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
  bInitialized = false;
}


void platform::GpuSensors::ReadData()
{
  data = Data();
  if (!bInitialized)
    return;

  Nvml &n = *pNvml;
  char sz[256];

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