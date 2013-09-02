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

#ifdef HAS_MEDIA_MP4
#include "mediaformats/readers/mp4/atomavc1.h"
#include "mediaformats/readers/mp4/mp4document.h"
#include "mediaformats/readers/mp4/atomavcc.h"

AtomAVC1::AtomAVC1(MP4Document *pDocument, uint32_t type, uint64_t size, uint64_t start)
: BoxAtom(pDocument, type, size, start) {
	_pAVCC = NULL;
}

AtomAVC1::~AtomAVC1() {
}

bool AtomAVC1::Read() {
	//aligned(8) abstract class SampleEntry (unsigned int(32) format) extends Box(format){
	//	const unsigned int(8)[6] reserved = 0;
	//	unsigned int(16) data_reference_index;
	//}
	//class VisualSampleEntry(codingname) extends SampleEntry (codingname){
	//  unsigned int(16) pre_defined = 0;
	//	const unsigned int(16) reserved = 0;
	//	unsigned int(32)[3] pre_defined = 0;
	//	unsigned int(16) width;
	//	unsigned int(16) height;
	//	template unsigned int(32) horizresolution = 0x00480000; // 72 dpi template
	//  unsigned int(32) vertresolution = 0x00480000; // 72 dpi
	//  const unsigned int(32) reserved = 0;
	//	template unsigned int(16) frame_count = 1;
	//	string[32] compressorname;
	//	template unsigned int(16) depth = 0x0018;
	//	int(16) pre_defined = -1;
	//}

	if (!SkipBytes(78)) {
		FATAL("Unable to skip 78 bytes");
		return false;
	}

	return BoxAtom::Read();
}

bool AtomAVC1::AtomCreated(BaseAtom *pAtom) {
	switch (pAtom->GetTypeNumeric()) {
			//TODO: What is the deal with all this order stuff!?
		case A_AVCC:
			_pAVCC = (AtomAVCC *) pAtom;
			return true;
		default:
		{
			FATAL("Invalid atom type: %s", STR(pAtom->GetTypeString()));
			return false;
		}
	}
}

#endif /* HAS_MEDIA_MP4 */
