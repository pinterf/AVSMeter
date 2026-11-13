#if !defined(_COMMON_H)
#define _COMMON_H

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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <process.h>
#include <string>
#include <algorithm>
#include <psapi.h>
#include <tlhelp32.h>
#include <wintrust.h>
#include <imagehlp.h>
#include <conio.h>
#include <time.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <set>
#include <map>
#include <eh.h>

using std::string;
using std::ifstream;
using std::ofstream;
using std::vector;
using std::map;
using std::sort;
using std::set;
using std::transform;
using std::getline;
using std::remove;
using std::exception;


#endif //_COMMON_H
