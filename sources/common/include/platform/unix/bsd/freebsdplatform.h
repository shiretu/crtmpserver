/*
 *  Copyright (c) 2010,
 *  Gavriloaie Eugen-Andrei (shiretu@gmail.com)
 *
 *  This file is part of crtmpserver.
 *  crtmpserver is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  crtmpserver is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with crtmpserver.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#ifdef FREEBSD

#define FILE_OFFSET_BITS 64
#define HAS_clock_gettime
#include "platform/unix/bsd/basebsdplatform.h"


//platform includes


//platform defines
#define LIBRARY_NAME_PATTERN "lib%s.so"
#define KQUEUE_TIMER_MULTIPLIER 1000
#ifndef OPEN_MAX
#define OPEN_MAX 100000
#endif /* OPEN_MAX */
#define MAP_NOCACHE 0
#define MAP_NOEXTEND 0
#define NOTE_USECONDS 0

#endif /* FREEBSD */
