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


#include "Timer.h"

#include <chrono>


static uint64_t SteadyNowMS()
{
  using namespace std::chrono;
  return (uint64_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}


CTimer::CTimer()
{
}

CTimer::~CTimer()
{
}



double CTimer::GetTimer()
{
  using namespace std::chrono;
  return duration<double>(steady_clock::now().time_since_epoch()).count();
}


double CTimer::GetSTDTimer()
{
  return ((double)GetSTDTimerMS() / 1000.0);
}


uint64_t CTimer::GetSTDTimerMS()
{
  return SteadyNowMS();
}


std::string CTimer::FormatTimeString(int64_t i_milliseconds, bool b_rightaligned)
{
  char buffer[128];
  unsigned int hours = (unsigned int)(i_milliseconds / 3600000);
  unsigned int minutes = (unsigned int)(i_milliseconds / 60000) % 60;
  unsigned int seconds = (unsigned int)(i_milliseconds / 1000) % 60;
  unsigned int milliseconds = (unsigned int)i_milliseconds % 1000;

  int iRet = -1;
  if (hours < 100)
    iRet = sprintf(buffer, "   %02u:%02u:%02u.%03u", hours, minutes, seconds, milliseconds);
  else
    iRet = sprintf(buffer, "%5u:%02u:%02u.%03u", hours, minutes, seconds, milliseconds);

  if (iRet < 0)
    return "buffer overflow";

  std::string sTime = &buffer[0];

  if (!b_rightaligned)
    sTime.erase(0, sTime.find_first_not_of(" "));

  return sTime;
}
