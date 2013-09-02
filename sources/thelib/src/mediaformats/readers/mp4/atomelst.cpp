/*
 * Copyright (c) 2009, Gavriloaie Eugen-Andrei (shiretu@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *		*Redistributions of source code must retain the above copyright notice,
 *		 this list of conditions and the following disclaimer.
 *		*Redistributions in binary form must reproduce the above copyright
 *		 notice, this list of conditions and the following disclaimer in the
 *		 documentation and/or other materials provided with the distribution.
 *		*Neither the name of the DEVSS nor the names of its
 *		 contributors may be used to endorse or promote products derived from
 *		 this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "mediaformats/readers/mp4/atomelst.h"

AtomELST::AtomELST(MP4Document *pDocument, uint32_t type, uint64_t size, uint64_t start)
: VersionedAtom(pDocument, type, size, start) {

}

AtomELST::~AtomELST() {
}

bool AtomELST::ReadData() {
	uint32_t count = 0;
	if (!ReadUInt32(count)) {
		FATAL("Unable to read elst entries count");
		return false;
	}
	//FINEST("-------");
	ELSTEntry entry;
	for (uint32_t i = 0; i < count; i++) {
		if (_version == 1) {
			if (!ReadUInt64(entry.value._64.segmentDuration)) {
				FATAL("Unable to read elst atom");
				return false;
			}
			if (!ReadUInt64(entry.value._64.mediaTime)) {
				FATAL("Unable to read elst atom");
				return false;
			}
		} else {
			if (!ReadUInt32(entry.value._32.segmentDuration)) {
				FATAL("Unable to read elst atom");
				return false;
			}
			if (!ReadUInt32(entry.value._32.mediaTime)) {
				FATAL("Unable to read elst atom");
				return false;
			}
		}
		if (!ReadUInt16(entry.mediaRateInteger)) {
			FATAL("Unable to read elst atom");
			return false;
		}
		if (!ReadUInt16(entry.mediaRateFraction)) {
			FATAL("Unable to read elst atom");
			return false;
		}
		//		FINEST("version: %"PRIu8"; segmentDuration: %"PRIu32"; mediaTime: %"PRIu32"; mediaRateInteger: %"PRIu16"; mediaRateFraction: %"PRIu16,
		//				_version,
		//				entry.value._32.segmentDuration,
		//				entry.value._32.mediaTime,
		//				entry.mediaRateInteger,
		//				entry.mediaRateFraction);
		_entries.push_back(entry);
	}
	return SkipRead(false);
}
