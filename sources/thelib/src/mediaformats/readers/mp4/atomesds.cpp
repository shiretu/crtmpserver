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
#include "mediaformats/readers/mp4/atomesds.h"


#define ESTAG_Forbidden 0x00
#define ESTAG_ObjectDescrTag 0x01
#define ESTAG_InitialObjectDescrTag 0x02
#define ESTAG_ES_DescrTag 0x03
#define ESTAG_DecoderConfigDescrTag 0x04
#define ESTAG_DecSpecificInfoTag 0x05
#define ESTAG_SLConfigDescrTag 0x06
#define ESTAG_ContentIdentDescrTag 0x07
#define ESTAG_SupplContentIdentDescrTag 0x08
#define ESTAG_IPI_DescrPointerTag 0x09
#define ESTAG_IPMP_DescrPointerTag 0x0A
#define ESTAG_IPMP_DescrTag 0x0B
#define ESTAG_QoS_DescrTag 0x0C
#define ESTAG_RegistrationDescrTag 0x0D
#define ESTAG_ES_ID_IncTag 0x0E
#define ESTAG_ES_ID_RefTag 0x0F
#define ESTAG_MP4_IOD_Tag 0x10
#define ESTAG_MP4_OD_Tag 0x11
#define ESTAG_IPL_DescrPointerRefTag 0x12
#define ESTAG_ExtensionProfileLevelDescrTag 0x13
#define ESTAG_profileLevelIndicationIndexDescrTag 0x14
#define ESTAG_ContentClassificationDescrTag 0x40
#define ESTAG_KeyWordDescrTag 0x41
#define ESTAG_RatingDescrTag 0x42
#define ESTAG_LanguageDescrTag 0x43
#define ESTAG_ShortTextualDescrTag 0x44
#define ESTAG_ExpandedTextualDescrTag 0x45
#define ESTAG_ContentCreatorNameDescrTag 0x46
#define ESTAG_ContentCreationDateDescrTag 0x47
#define ESTAG_OCICreatorNameDescrTag 0x48
#define ESTAG_OCICreationDateDescrTag 0x49
#define ESTAG_SmpteCameraPositionDescrTag 0x4A
#define ESTAG_SegmentDescrTag 0x4B
#define ESTAG_MediaTimeDescrTag 0x4C
#define ESTAG_IPMP_ToolsListDescrTag 0x60
#define ESTAG_IPMP_ToolTag 0x61
#define ESTAG_M4MuxTimingDescrTag 0x62
#define ESTAG_M4MuxCodeTableDescrTag 0x63
#define ESTAG_ExtSLConfigDescrTag 0x64
#define ESTAG_M4MuxBufferSizeDescrTag 0x65
#define ESTAG_M4MuxIdentDescrTag 0x66
#define ESTAG_DependencyPointerTag 0x67
#define ESTAG_DependencyMarkerTag 0x68
#define ESTAG_M4MuxChannelDescrTag 0x69

AtomESDS::AtomESDS(MP4Document *pDocument, uint32_t type, uint64_t size, uint64_t start)
: VersionedAtom(pDocument, type, size, start) {
	_MP4ESDescrTag_ID = 0;
	_MP4ESDescrTag_Priority = 0;
	_MP4DecConfigDescrTag_ObjectTypeID = 0;
	_MP4DecConfigDescrTag_StreamType = 0;
	_MP4DecConfigDescrTag_BufferSizeDB = 0;
	_MP4DecConfigDescrTag_MaxBitRate = 0;
	_MP4DecConfigDescrTag_AvgBitRate = 0;
	_extraDataStart = 0;
	_extraDataLength = 0;
	_isMP3 = false;
#ifdef DEBUG_ESDS_ATOM
	_objectType = 0;
	_sampleRate = 0;
	_channels = 0;
	_extObjectType = 0;
	_sbr = 0;
	_extSampleRate = 0;
#endif /* DEBUG_ESDS_ATOM */
}

AtomESDS::~AtomESDS() {
}

bool AtomESDS::IsMP3() {
	return _isMP3;
}

uint64_t AtomESDS::GetExtraDataStart() {
	return _extraDataStart;
}

uint64_t AtomESDS::GetExtraDataLength() {
	return _extraDataLength;
}

bool AtomESDS::ReadData() {
	//14496-1.pdf 7.2.2.2.1 Syntax page 32/158
	while (CurrentPosition() != _start + _size) {
		uint8_t tag;
		uint32_t length;

		if (!ReadTagAndLength(tag, length)) {
			FATAL("Unable to read tag type and length");
			return false;
		}
		//FINEST("tag: %02"PRIx8"; length: %"PRIu32, tag, length);

		switch (tag) {
			case ESTAG_ES_DescrTag:
			{
				if (!ReadESDescrTag()) {
					FATAL("Unable to read tag: 0x%02"PRIu8, tag);
					return false;
				}
				break;
			}
			default:
			{
				FATAL("Unknown descriptor tag: 0x%02"PRIu8, tag);
				return false;
			}
		}
	}

	return true;
}

bool AtomESDS::ReadTagLength(uint32_t &length) {
	length = 0;
	uint32_t count = 4;
	while (count--) {
		uint8_t c = 0;
		if (!ReadUInt8(c))
			return false;

		length = (length << 7) | (c & 0x7f);
		if (!(c & 0x80))
			break;
	}
	return true;
}

bool AtomESDS::ReadTagAndLength(uint8_t &tagType, uint32_t &length) {
	if (!ReadUInt8(tagType))
		return false;
	if (!ReadTagLength(length))
		return false;
	return true;
}

bool AtomESDS::ReadESDescrTag() {
	//14496-1.pdf 7.2.6.5 ES_Descriptor page 47/158

	//ES_ID
	if (!SkipBytes(2)) {
		FATAL("Unable to skip bytes");
		return false;
	}

	//streamDependenceFlag, URL_Flags, OCRstreamFlag, streamPriority
	uint8_t flags;
	if (!ReadUInt8(flags)) {
		FATAL("Unable to read flags");
		return false;
	}

	if ((flags & 0x80) != 0) {
		//dependsOn_ES_ID
		if (!SkipBytes(2)) {
			FATAL("Unable to skip bytes");
			return false;
		}
	}

	if ((flags & 0x40) != 0) {
		//URLlength;
		uint8_t URLlength;
		if (!ReadUInt8(URLlength)) {
			FATAL("Unable to read URLlength");
			return false;
		}

		//URLstring[URLlength];
		if (!SkipBytes(URLlength)) {
			FATAL("Unable to skip bytes");
			return false;
		}
	}

	if ((flags & 0x20) != 0) {
		//OCR_ES_Id
		if (!SkipBytes(2)) {
			FATAL("Unable to skip bytes");
			return false;
		}
	}

	uint8_t tag;
	uint32_t length;
	if (!ReadTagAndLength(tag, length)) {
		FATAL("Unable to read tag type and length");
		return false;
	}
	if ((tag != ESTAG_DecoderConfigDescrTag) || (length == 0)) {
		FATAL("Invalid descriptor");
		return false;
	}
	return ReadDecoderConfigDescriptorTag();
}

bool AtomESDS::ReadDecoderConfigDescriptorTag() {
	//14496-1.pdf 7.2.6.6 DecoderConfigDescriptor page 48/158

	//objectTypeIndication
	uint8_t objectTypeIndication;
	if (!ReadUInt8(objectTypeIndication)) {
		FATAL("Unable to read objectTypeIndication");
		return false;
	}

	//streamType, upStream, reserved
	uint8_t streamType;
	if (!ReadUInt8(streamType)) {
		FATAL("Unable to read streamType");
		return false;
	}
	streamType = streamType >> 2;

	//bufferSizeDB, maxBitrate, avgBitrate
	if (!SkipBytes(11)) {
		FATAL("Unable to skip bytes");
		return false;
	}

	//decSpecificInfo[0 .. 1]
	switch (objectTypeIndication) {
		case 0x20: //Visual ISO/IEC 14496-2
		case 0x21: //Visual ITU-T Recommendation H.264 | ISO/IEC 14496-10
		case 0x60: //Visual ISO/IEC 13818-2 Simple Profile
		case 0x61: //Visual ISO/IEC 13818-2 Main Profile
		case 0x62: //Visual ISO/IEC 13818-2 SNR Profile
		case 0x63: //Visual ISO/IEC 13818-2 Spatial Profile
		case 0x64: //Visual ISO/IEC 13818-2 High Profile
		case 0x65: //Visual ISO/IEC 13818-2 422 Profile
		case 0x6a: //Visual ISO/IEC 11172-2
		case 0x6c: //Visual ISO/IEC 10918-1
		case 0x6e: //Visual ISO/IEC 15444-1
		{
			FATAL("Visual objectTypeIndication 0x%02"PRIx8" not implemented yet", objectTypeIndication);
			return false;
		}
		case 0x6b: //Audio ISO/IEC 11172-3
		case 0x69: //Audio ISO/IEC 13818-3 MP3
		{
			if (!SkipRead(false)) {
				FATAL("Unable to skip atom");
				return false;
			}
			_isMP3 = true;
			break;
		}
		case 0x40: //Audio ISO/IEC 14496-3
		{
			uint8_t tag;
			uint32_t length;
			if (!ReadTagAndLength(tag, length)) {
				FATAL("Unable to read tag type and length");
				return false;
			}
			if ((tag != ESTAG_DecSpecificInfoTag) || (length == 0)) {
				FATAL("Invalid ESDS atom for AAC content");
				return false;
			}
			_extraDataStart = CurrentPosition();
			_extraDataLength = length;
			if (!SkipRead(false)) {
				FATAL("Unable to skip atom");
				return false;
			}
			break;
		}
		case 0x66: //Audio ISO/IEC 13818-7 Main Profile
		case 0x67: //Audio ISO/IEC 13818-7 LowComplexity Profile
		case 0x68: //Audio ISO/IEC 13818-7 Scalable Sampling Rate Profile
		{
			FATAL("Audio objectTypeIndication 0x%02"PRIx8" not implemented yet", objectTypeIndication);
			return false;
		}
		default:
		{
			FATAL("objectTypeIndication 0x%02"PRIx8" not supported", objectTypeIndication);
			return false;
		}
	}
	return true;
}
#endif /* HAS_MEDIA_MP4 */
