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

#include <dlfcn.h>

#include <algorithm>
#include <cstdio>


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

  unsigned int uiIndex = 0;
  if ((s_select == "intel") || (s_select == "amd"))
  {
    sError = "\"-gpu=" + s_select + "\": only NVIDIA GPUs are supported on this platform yet";
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
    sError = "No NVIDIA GPU found";
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