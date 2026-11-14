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

#if !defined(_EXCEPTION_H)
#define _EXCEPTION_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "common.h"
#include "Utility.h"

//test STATUS_FLOAT_DIVIDE_BY_ZERO:
//  int a = 0;
//  int b = 100;
//  volatile float c = b/a;

//test STATUS_ACCESS_VIOLATION
//  volatile int *pInt = 0x00000000;
//  *pInt = 20;


string GetExceptionModule(LPVOID address);
void SE_Translator(unsigned int, EXCEPTION_POINTERS* pExcept);


#endif //_EXCEPTION_H

