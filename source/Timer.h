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


#if !defined(_TIMER_H)
#define _TIMER_H

#include "common.h"

class CTimer
{
public:
  CTimer();
  virtual          ~CTimer();
  double           GetTimer();
  double           GetSTDTimer();
  uint64_t         GetSTDTimerMS();
  std::string      FormatTimeString(int64_t i_milliseconds, bool b_rightaligned);
};


#endif //_TIMER_H

