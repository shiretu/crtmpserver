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
#ifndef _TSPARSER_H
#define	_TSPARSER_H

#include "common.h"
#include "mediaformats/readers/ts/piddescriptor.h"

class TSParserEventsSink;

#define TS_CHUNK_188 188
#define TS_CHUNK_204 204
#define TS_CHUNK_208 208

class TSParser {
private:
	TSParserEventsSink *_pEventSink;
	uint32_t _chunkSize;
	map<uint16_t, PIDDescriptor *> _pidMapping;
	map<uint16_t, uint16_t> _unknownPids;
	uint64_t _totalParsedBytes;
public:
	TSParser(TSParserEventsSink *pEventSink);
	virtual ~TSParser();

	void SetChunkSize(uint32_t chunkSize);
	bool ProcessPacket(uint32_t packetHeader, IOBuffer &buffer,
			uint32_t maxCursor);
	bool ProcessBuffer(IOBuffer &buffer, bool chunkByChunk);
	uint64_t GetTotalParsedBytes();
private:
	void FreePidDescriptor(PIDDescriptor *pPIDDescriptor);
	bool ProcessPidTypePAT(uint32_t packetHeader, PIDDescriptor &pidDescriptor,
			uint8_t *pBuffer, uint32_t &cursor, uint32_t maxCursor);
	bool ProcessPidTypePMT(uint32_t packetHeader, PIDDescriptor &pidDescriptor,
			uint8_t *pBuffer, uint32_t &cursor, uint32_t maxCursor);
	bool ProcessPidTypeAV(PIDDescriptor *pPIDDescriptor, uint8_t *pData,
			uint32_t length, bool packetStart, int8_t sequenceNumber);
};


#endif	/* _TSPARSER_H */
#endif	/* HAS_MEDIA_TS */
