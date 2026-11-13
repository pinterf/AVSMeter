#if !defined(_TIMER_H)
#define _TIMER_H

/*
	This file is part of AVSMeter, Copyright(C) Groucho2004.

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

#include "common.h"

class CTimer
{
public:
	CTimer();
	virtual       ~CTimer();
	double        GetTimer();
	double        GetSTDTimer();
	BOOL          TestTimer();
	string        FormatTimeString(__int64 i_milliseconds, BOOL b_rightaligned);
private:
};

#endif //_TIMER_H
