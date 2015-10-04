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

#ifdef ANDROID

#include "common.h"

time_t timegm(struct tm * const t) {
	// time_t is signed on Android.
	static const time_t kTimeMax = ~(1 << (sizeof (time_t) * CHAR_BIT - 1));
	static const time_t kTimeMin = (1 << (sizeof (time_t) * CHAR_BIT - 1));
	time64_t result = timegm64(t);
	if (result < kTimeMin || result > kTimeMax)
		return -1;
	return result;
}

#endif /* ANDROID */
