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

#ifdef HAS_MEDIA_TS
#ifndef _TSSTREAMINFO_H
#define	_TSSTREAMINFO_H

#include "common.h"
#include "mediaformats/readers/ts/streamdescriptors.h"

struct TSStreamInfo {
	uint8_t streamType; //Table 2-29 : Stream type assignments
	uint16_t elementaryPID;
	uint16_t esInfoLength;
	vector<StreamDescriptor> esDescriptors;

	TSStreamInfo() {
		streamType = 0;
		elementaryPID = 0;
		esInfoLength = 0;
	}

	operator string() {
		return toString(0);
	}

	string toString(int32_t indent) {
		string result = format("%sstreamType: 0x%02"PRIx8"; elementaryPID: %hu; esInfoLength: %hu; descriptors count: %"PRIz"u\n",
				STR(string(indent, '\t')),
				streamType, elementaryPID, esInfoLength, esDescriptors.size());
		for (uint32_t i = 0; i < esDescriptors.size(); i++) {
			result += format("%s%s", STR(string(indent + 1, '\t')), STR(esDescriptors[i]));
			if (i != esDescriptors.size() - 1)
				result += "\n";
		}
		return result;
	}
};

#endif	/* _TSSTREAMINFO_H */
#endif	/* HAS_MEDIA_TS */
