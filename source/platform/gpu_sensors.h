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


#if !defined(_PLATFORM_GPU_SENSORS_H)
#define _PLATFORM_GPU_SENSORS_H

#include <cstdint>
#include <string>

namespace platform
{
  // GPU information and sensor readings (Windows: GPU-Z shared memory interface).
  class GpuSensors
  {
  public:
    GpuSensors();
    ~GpuSensors();
    GpuSensors(const GpuSensors &) = delete;
    GpuSensors &operator=(const GpuSensors &) = delete;

    struct Sensors
    {
      uint8_t  GPULoad;
      uint8_t  VPULoad;
      uint8_t  GPUTemp;
      double   PowerConsumption;
      uint32_t MemoryUsedGeneral;
      uint32_t MemoryUsedDedicated;
      uint32_t MemoryUsedDynamic;
      bool     ReadError;
    } sensors;

    struct Data
    {
      std::string GPUZVersion;
      std::string CardName;
      std::string GPUName;
      std::string MemSize;
      std::string OpenCLVersion;
      std::string DriverVersion;
      bool        NVVPU;
      bool        GeneralMem;
      bool        DynamicMem;
      bool        DedicatedMem;
    } data;

    // Init() sets bInitialized, or sError on failure; it reads data on success.
    void Init();
    void Release();
    void ReadSensors();
    void ReadData();
    bool bInitialized;
    std::string sError;

  private:
    void *hMapFile;
    void *pMapAddr;
  };
}


#endif //_PLATFORM_GPU_SENSORS_H
