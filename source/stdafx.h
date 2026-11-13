/*
	This file is part of AVSMeter,
	Copyright(C) Groucho2004.

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

#if _MSC_VER > 1000
#pragma once
#endif

//#pragma warning (disable : 4101)

#include <windows.h>
#include <process.h>
#include <string>
#include <algorithm>
#include <psapi.h>
#include <tlhelp32.h>
#include <wintrust.h>
#include <imagehlp.h>
#include <conio.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <map>

using namespace std;

#include "avsinc\avisynth.h"
const AVS_Linkage *AVS_linkage = 0;
