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
#ifndef _TSPACKETPMT_H
#define	_TSPACKETPMT_H

#include "common.h"
#include "mediaformats/readers/ts/streamdescriptors.h"
#include "mediaformats/readers/ts/tsstreaminfo.h"


//Table 2-29 : Stream type assignments
/*
0x00		ITU-T | ISO/IEC Reserved
0x01		ISO/IEC 11172 Video
0x02		ITU-T Rec. H.262 | ISO/IEC 13818-2 Video or ISO/IEC 11172-2 constrained parameter video stream
0x03		ISO/IEC 11172 Audio
0x04		ISO/IEC 13818-3 Audio
0x05		ITU-T Rec. H.222.0 | ISO/IEC 13818-1 private_sections
0x06		ITU-T Rec. H.222.0 | ISO/IEC 13818-1 PES packets containing private data
0x07		ISO/IEC 13522 MHEG
0x08		ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Annex A DSM-CC
0x09		ITU-T Rec. H.222.1
0x0a		ISO/IEC 13818-6 type A
0x0b		ISO/IEC 13818-6 type B
0x0c		ISO/IEC 13818-6 type C
0x0d		ISO/IEC 13818-6 type D
0x0e		ITU-T Rec. H.222.0 | ISO/IEC 13818-1 auxiliary
0x0f		ISO/IEC 13818-7 Audio with ADTS transport syntax
0x10		ISO/IEC 14496-2 Visual
0x11		ISO/IEC 14496-3 Audio with the LATM transport syntax as defined in ISO/IEC 14496-3 / AMD 1
0x12		ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in PES packets
0x13		ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in ISO/IEC14496_sections.
0x14		ISO/IEC 13818-6 Synchronized Download Protocol
0x15-0x7f	ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Reserved
 */

#define TS_STREAMTYPE_VIDEO_MPEG1     0x01
#define TS_STREAMTYPE_VIDEO_MPEG2     0x02
#define TS_STREAMTYPE_VIDEO_MPEG4     0x10
#define TS_STREAMTYPE_VIDEO_H264      0x1b
#define TS_STREAMTYPE_VIDEO_VC1       0xea
#define TS_STREAMTYPE_VIDEO_DIRAC     0xd1

#define TS_STREAMTYPE_AUDIO_MPEG1     0x03
#define TS_STREAMTYPE_AUDIO_MPEG2     0x04
#define TS_STREAMTYPE_AUDIO_AAC       0x0f
#define TS_STREAMTYPE_AUDIO_AC3       0x81
#define TS_STREAMTYPE_AUDIO_DTS       0x8a
#define TS_STREAMTYPE_UNDEFINED       0x8a

//2.4.4.8

//iso13818-1.pdf page 64/174
//Table 2-28‚ Transport Stream program map section

class TSPacketPMT {
private:
	//fields
	uint8_t _tableId;
	bool _sectionSyntaxIndicator;
	bool _reserved1;
	uint8_t _reserved2;
	uint16_t _sectionLength;
	uint16_t _programNumber;
	uint8_t _reserved3;
	uint8_t _versionNumber;
	bool _currentNextIndicator;
	uint8_t _sectionNumber;
	uint8_t _lastSectionNumber;
	uint8_t _reserved4;
	uint16_t _pcrPid;
	uint8_t _reserved5;
	uint16_t _programInfoLength;
	uint32_t _crc;

	//internal variables
	vector<StreamDescriptor> _programInfoDescriptors;
	map<uint16_t, TSStreamInfo> _streams;
public:
	TSPacketPMT();
	virtual ~TSPacketPMT();

	operator string();

	uint16_t GetProgramNumber();
	uint32_t GetCRC();
	map<uint16_t, TSStreamInfo> & GetStreamsInfo();

	bool Read(uint8_t *pBuffer, uint32_t &cursor, uint32_t maxCursor);
	static uint32_t PeekCRC(uint8_t *pBuffer, uint32_t cursor, uint32_t maxCursor);
	uint32_t GetBandwidth();
};


#endif	/* _TSPACKETPMT_H */
#endif	/* HAS_MEDIA_TS */

