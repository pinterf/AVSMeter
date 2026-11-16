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


#if !defined(_VERSION_H)
#define _VERSION_H

#define PRODUCTNAME_STR "AVSMeter\0"
#define VERSION_NUM      4,0,0,0
#define VERSION_STR     "4.0"
#define COPYRIGHT_STR   "(c) 2012-2021 Groucho2004, 2026 pinterf\0"
// Version resource only: ASCII source with an escaped é, as llvm-rc rejects UTF-8 .rc files
#define RC_COPYRIGHT_STR L"(c) 2012-2021 Groucho2004, 2026 Ferenc Pint\x00E9r (pinterf)"
#define COMMENTS_STR    "AVSMeter - Avisynth Script Testing\0"

#define IDI_MAINFRAME 101

#endif //_VERSION_H
