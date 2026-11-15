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


// GPU-Z is Windows only: Init() reports that there is no sensor source.
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


void platform::GpuSensors::Init()
{
  sError = "GPU sensors are not available on this platform";
}


void platform::GpuSensors::Release()
{
  bInitialized = false;
}


void platform::GpuSensors::ReadSensors()
{
  sensors.ReadError = true;
}


void platform::GpuSensors::ReadData()
{
}
