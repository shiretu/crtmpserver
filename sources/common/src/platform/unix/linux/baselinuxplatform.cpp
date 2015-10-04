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

#if defined ANDROID || defined LINUX || defined SOLARIS

#include "common.h"

double getFileModificationDate(const string &path) {
	struct stat s;
	if (stat(path.c_str(), &s) != 0) {
		FATAL("Unable to stat file %s", STR(path));
		return 0;
	}

	return (double) s.st_mtim.tv_sec + (double) s.st_mtim.tv_nsec / 1000000000.0;
}

bool setFdNoSIGPIPE(SOCKET_TYPE fd) {
	//on linux/android, we use MSG_NOSIGNAL when using send/write functions
	//on solaris, we ignore SIGPIPE using sigaction
	return true;
}

#endif /* defined ANDROID || defined LINUX || defined SOLARIS */
