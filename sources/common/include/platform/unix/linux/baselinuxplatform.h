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

#if defined ANDROID || defined LINUX || defined SOLARIS

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif /* _GNU_SOURCE */
#define _FILE_OFFSET_BITS 64
#define HAS_clock_gettime
#include "platform/unix/baseunixplatform.h"

#define LIBRARY_NAME_PATTERN "lib%s.so"
#define MSGHDR_MSG_IOVLEN_TYPE size_t
#define FD_COPY(src,dst) memcpy(dst,src,sizeof(fd_set));
#ifndef OPEN_MAX
#define OPEN_MAX 100000
#endif /* OPEN_MAX */

#endif /* defined ANDROID || defined LINUX || defined SOLARIS */
