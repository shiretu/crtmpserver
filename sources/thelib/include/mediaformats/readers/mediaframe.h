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

#ifndef _MEDIAFRAME_H
#define	_MEDIAFRAME_H

#include "common.h"

#define MEDIAFRAME_TYPE_AUDIO 0
#define MEDIAFRAME_TYPE_VIDEO 1
#define MEDIAFRAME_TYPE_DATA 2

typedef struct _MediaFrame {
	uint64_t start;
	uint64_t length;
	uint8_t type;
	bool isKeyFrame;
	double pts;
	double dts;
	double cts;
	bool isBinaryHeader;

	operator string() {
		return format("s: %16"PRIx64"; l: %6"PRIx64"; t: %"PRIu8"; kf: %"PRIu8"; pts: %8.2f; dts: %8.2f; cts: %6.2f; bh: %"PRIu8,
				start, length, type, isKeyFrame, pts, dts, cts, isBinaryHeader);
	};
} MediaFrame;

#endif	/* _MEDIAFRAME_H */

