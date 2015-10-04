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

#ifdef SOLARIS
#include "platform/unix/linux/baselinuxplatform.h"

//platform includes

//platform defines
#define MSG_NOSIGNAL 0
#define MAP_NOCACHE 0
#define MAP_NOEXTEND 0
#define MAP_FILE 0
#define SO_NOSIGPIPE 0

//platform functions
time_t timegm(struct tm *tm);
#endif /* SOLARIS */
