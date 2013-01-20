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

#include "common.h"

BasePlatform::BasePlatform() {

}

BasePlatform::~BasePlatform() {
}

#if defined DFREEBSD || defined FREEBSD || defined LINUX || defined OPENBSD || defined OSX || defined SOLARIS

bool setFdCloseOnExec(int fd) {
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {
		int err = errno;
		FATAL("fcntl failed %d %s", err, strerror(err));
		return false;
	}
	return true;
}

#endif /* defined DFREEBSD || defined FREEBSD || defined LINUX || defined OPENBSD || defined OSX || defined SOLARIS */
