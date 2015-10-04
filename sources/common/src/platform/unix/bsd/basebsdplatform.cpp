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

#if defined FREEBSD || defined OSX
#include "common.h"

double getFileModificationDate(const string &path) {
	struct stat s;
	if (stat(path.c_str(), &s) != 0) {
		FATAL("Unable to stat file %s", STR(path));
		return 0;
	}
	return (double) s.st_mtimespec.tv_sec + (double) s.st_mtimespec.tv_nsec / 1000000000.0;
}

bool setFdNoSIGPIPE(SOCKET_TYPE fd) {
	int32_t one = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE,
			(const char*) & one, sizeof (one)) != 0) {
		FATAL("Unable to set SO_NOSIGPIPE");
		return false;
	}
	return true;
}

#endif /* defined FREEBSD || defined OSX */
