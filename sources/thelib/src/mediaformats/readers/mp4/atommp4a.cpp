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
#include "mediaformats/readers/mp4/atommp4a.h"
#include "mediaformats/readers/mp4/mp4document.h"

AtomMP4A::AtomMP4A(MP4Document *pDocument, uint32_t type, uint64_t size, uint64_t start)
: BoxAtom(pDocument, type, size, start) {
	_pESDS = NULL;
	_pWAVE = NULL;
	_pCHAN = NULL;
}

AtomMP4A::~AtomMP4A() {
}

bool AtomMP4A::Read() {
	//aligned(8) abstract class SampleEntry (unsigned int(32) format) extends Box(format){
	//	const unsigned int(8)[6] reserved = 0;
	//	unsigned int(16) data_reference_index;
	//}
	//class AudioSampleEntry(codingname) extends SampleEntry (codingname){
	//	const unsigned int(16) version;
	//	const unsigned int(16) revision_level;
	//	const unsigned int(32) vendor;
	//	template unsigned int(16) channelcount = 2;
	//	template unsigned int(16) samplesize = 16;
	//	unsigned int(16) audio_cid = 0;
	//	const unsigned int(16) packet_size = 0 ;
	//	template unsigned int(32) samplerate = {timescale of media}<<16;
	//}

	if (GetSize() == 0x0c)
		return true;

	if (!SkipBytes(8)) {
		FATAL("Unable to skip 8 bytes");
		return false;
	}

	uint16_t version = 0;
	if (!ReadUInt16(version)) {
		FATAL("Unable to read version");
		return false;
	}

	if (!SkipBytes(18)) {
		FATAL("Unable to skip 18 bytes");
		return false;
	}

	switch (version) {
		case 0:
		{
			//standard isom
			break;
		}
		case 1:
		{
			//QT ver1
			//	const unsigned int(32) samples_per_frame;
			//	const unsigned int(32) bytes_per_packet;
			//	const unsigned int(32) bytes_per_frame;
			//	const unsigned int(32) bytes_per_sample;
			if (!SkipBytes(16)) {
				FATAL("Unable to skip 16 bytes");
				return false;
			}
			break;
		}
		case 2:
		{
			//QT ver2
			FATAL("QT version 2 not supported");
			return false;
			//			avio_rb32(pb); /* sizeof struct only */
			//			st->codec->sample_rate = av_int2double(avio_rb64(pb)); /* float 64 */
			//			st->codec->channels = avio_rb32(pb);
			//			avio_rb32(pb); /* always 0x7F000000 */
			//			st->codec->bits_per_coded_sample = avio_rb32(pb); /* bits per channel if sound is uncompressed */
			//			flags = avio_rb32(pb); /* lpcm format specific flag */
			//			sc->bytes_per_frame = avio_rb32(pb); /* bytes per audio packet if constant */
			//			sc->samples_per_frame = avio_rb32(pb); /* lpcm frames per audio packet if constant */
			//			if (format == MKTAG('l', 'p', 'c', 'm'))
			//				st->codec->codec_id = ff_mov_get_lpcm_codec_id(st->codec->bits_per_coded_sample, flags);
		}
		default:
		{
			FATAL("MP4A version not supported");
			return false;
		}
	}

	return BoxAtom::Read();
}

bool AtomMP4A::AtomCreated(BaseAtom *pAtom) {
	switch (pAtom->GetTypeNumeric()) {
		case A_ESDS:
			_pESDS = (AtomESDS *) pAtom;
			return true;
		case A_WAVE:
			_pWAVE = (AtomWAVE *) pAtom;
			return true;
		case A_CHAN:
			_pCHAN = (AtomCHAN *) pAtom;
			return true;
		default:
		{
			FATAL("Invalid atom type: %s", STR(pAtom->GetTypeString()));
			return false;
		}
	}
}

#endif /* HAS_MEDIA_MP4 */
