/*
 Copyright (C) 2004-2008 Grame

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation; either version 2.1 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

 */


#ifndef __JackTypes_WIN32__
#define __JackTypes_WIN32__

#ifdef __MINGW32__
#include <winsock2.h>
#endif
#include <windows.h>

typedef ULONGLONG UInt64;
typedef UInt64 uint64_t;
typedef unsigned short uint16_t;
typedef DWORD jack_tls_key;

#endif

