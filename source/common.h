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

#define b0000  0
#define b0001  1
#define b0010  2
#define b0011  3
#define b0100  4
#define b0101  5
#define b0110  6
#define b0111  7
#define b1000  8
#define b1001  9
#define b1010 10
#define b1011 11
#define b1100 12
#define b1101 13
#define b1110 14
#define b1111 15

#endif //_COMMON_H
