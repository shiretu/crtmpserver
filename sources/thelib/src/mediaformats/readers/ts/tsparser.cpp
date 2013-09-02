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

#include "mediaformats/readers/ts/tsparser.h"
#include "mediaformats/readers/ts/tspacketheader.h"
#include "mediaformats/readers/ts/tsboundscheck.h"
#include "mediaformats/readers/ts/avcontext.h"
#include "mediaformats/readers/ts/tspacketpat.h"
#include "mediaformats/readers/ts/tspacketpmt.h"
#include "mediaformats/readers/ts/tsparsereventsink.h"
#include "streaming/baseinstream.h"

TSParser::TSParser(TSParserEventsSink *pEventSink) {
	_pEventSink = pEventSink;
	_chunkSize = 188;

	//2. Setup the PAT pid
	PIDDescriptor *pPAT = new PIDDescriptor;
	pPAT->type = PID_TYPE_PAT;
	pPAT->pid = 0;
	pPAT->crc = 0;
	pPAT->pAVContext = NULL;
	_pidMapping[0] = pPAT;

	//3. Setup CAT table
	PIDDescriptor *pCAT = new PIDDescriptor;
	pCAT->type = PID_TYPE_CAT;
	pCAT->pid = 1;
	pCAT->crc = 0;
	pCAT->pAVContext = NULL;
	_pidMapping[1] = pCAT;

	//4. Setup TSDT table
	PIDDescriptor *pTSDT = new PIDDescriptor;
	pTSDT->type = PID_TYPE_TSDT;
	pTSDT->pid = 2;
	pTSDT->crc = 0;
	pTSDT->pAVContext = NULL;
	_pidMapping[2] = pTSDT;

	//5. Setup reserved tables
	for (uint16_t i = 3; i < 16; i++) {
		PIDDescriptor *pReserved = new PIDDescriptor;
		pReserved->type = PID_TYPE_RESERVED;
		pReserved->pid = i;
		pReserved->crc = 0;
		pReserved->pAVContext = NULL;
		_pidMapping[i] = pReserved;
	}

	//6. Setup the NULL pid
	PIDDescriptor *pNULL = new PIDDescriptor;
	pNULL->type = PID_TYPE_NULL;
	pNULL->pid = 0x1fff;
	pNULL->crc = 0;
	pNULL->pAVContext = NULL;
	_pidMapping[0x1fff] = pNULL;

	_totalParsedBytes = 0;
}

TSParser::~TSParser() {
	//1. Cleanup the pid mappings

	FOR_MAP(_pidMapping, uint16_t, PIDDescriptor *, i) {
		FreePidDescriptor(MAP_VAL(i));
	}
	_pidMapping.clear();
}

void TSParser::SetChunkSize(uint32_t chunkSize) {
	_chunkSize = chunkSize;
}

bool TSParser::ProcessPacket(uint32_t packetHeader, IOBuffer &buffer,
		uint32_t maxCursor) {
	//1. Get the PID descriptor or create it if absent
	PIDDescriptor *pPIDDescriptor = NULL;
	if (MAP_HAS1(_pidMapping, TS_TRANSPORT_PACKET_PID(packetHeader))) {
		pPIDDescriptor = _pidMapping[TS_TRANSPORT_PACKET_PID(packetHeader)];
	} else {
		pPIDDescriptor = new PIDDescriptor;
		pPIDDescriptor->type = PID_TYPE_UNKNOWN;
		pPIDDescriptor->pAVContext = NULL;
		pPIDDescriptor->pid = TS_TRANSPORT_PACKET_PID(packetHeader);
		_pidMapping[pPIDDescriptor->pid] = pPIDDescriptor;
	}

	//2. Skip the transport packet structure
	uint8_t *pBuffer = GETIBPOINTER(buffer);
	uint32_t cursor = 4;

	if (TS_TRANSPORT_PACKET_HAS_ADAPTATION_FIELD(packetHeader)) {
		TS_CHECK_BOUNDS(1);
		TS_CHECK_BOUNDS(pBuffer[cursor]);
		cursor += pBuffer[cursor] + 1;
	}
	if (!TS_TRANSPORT_PACKET_HAS_PAYLOAD(packetHeader)) {
		return true;
	}

	switch (pPIDDescriptor->type) {
		case PID_TYPE_PAT:
		{
			return ProcessPidTypePAT(packetHeader, *pPIDDescriptor, pBuffer, cursor, maxCursor);
		}
		case PID_TYPE_NIT:
		case PID_TYPE_CAT:
		case PID_TYPE_TSDT:
		{
			//			FINEST("%s", STR(IOBuffer::DumpBuffer(pBuffer + cursor,
			//					_chunkSize - cursor)));
			return true;
		}
		case PID_TYPE_PMT:
		{
			return ProcessPidTypePMT(packetHeader, *pPIDDescriptor, pBuffer,
					cursor, maxCursor);
		}
		case PID_TYPE_AUDIOSTREAM:
		case PID_TYPE_VIDEOSTREAM:
		{
			return ProcessPidTypeAV(pPIDDescriptor,
					pBuffer + cursor, _chunkSize - cursor,
					TS_TRANSPORT_PACKET_IS_PAYLOAD_START(packetHeader),
					(int8_t) packetHeader & 0x0f);
		}
			//		case PID_TYPE_AUDIOSTREAM:
			//		{
			//			if (_pEventSink != NULL)
			//				return _pEventSink->ProcessData(pPIDDescriptor,
			//					pBuffer + cursor, _chunkSize - cursor,
			//					TS_TRANSPORT_PACKET_IS_PAYLOAD_START(packetHeader), true,
			//					(int8_t) packetHeader & 0x0f);
			//			return true;
			//		}
			//		case PID_TYPE_VIDEOSTREAM:
			//		{
			//			if (_pEventSink != NULL)
			//				return _pEventSink->ProcessData(pPIDDescriptor,
			//					pBuffer + cursor, _chunkSize - cursor,
			//					TS_TRANSPORT_PACKET_IS_PAYLOAD_START(packetHeader), false,
			//					(int8_t) packetHeader & 0x0f);
			//			return true;
			//		}
		case PID_TYPE_RESERVED:
		{
			WARN("This PID %hu should not be used because is reserved according to iso13818-1.pdf", pPIDDescriptor->pid);
			return true;
		}
		case PID_TYPE_UNKNOWN:
		{
			if (!MAP_HAS1(_unknownPids, pPIDDescriptor->pid)) {
				//WARN("PID %"PRIu16" not known yet", pPIDDescriptor->pid);
				_unknownPids[pPIDDescriptor->pid] = pPIDDescriptor->pid;
			}
			return true;

		}
		case PID_TYPE_NULL:
		{
			//Ignoring NULL pids
			return true;
		}
		default:
		{
			WARN("PID type not implemented: %d. Pid number: %"PRIu16,
					pPIDDescriptor->type, pPIDDescriptor->pid);
			return false;
		}
	}
}

bool TSParser::ProcessBuffer(IOBuffer &buffer, bool chunkByChunk) {
	while (GETAVAILABLEBYTESCOUNT(buffer) >= _chunkSize) {
		if (GETIBPOINTER(buffer)[0] != 0x47) {
			WARN("Bogus chunk detected");
			if (_pEventSink != NULL) {
				_pEventSink->SignalResetChunkSize();
			}
			return true;
		}

		uint32_t packetHeader = ENTOHLP(GETIBPOINTER(buffer));

		if (!ProcessPacket(packetHeader, buffer, _chunkSize)) {
			FATAL("Unable to process packet");
			return false;
		}

		_totalParsedBytes += _chunkSize;

		if (!buffer.Ignore(_chunkSize)) {
			FATAL("Unable to ignore %u bytes", _chunkSize);
			return false;
		}

		if (chunkByChunk)
			break;
	}

	if (!chunkByChunk)
		buffer.MoveData();

	return true;
}

uint64_t TSParser::GetTotalParsedBytes() {
	return _totalParsedBytes;
}

void TSParser::FreePidDescriptor(PIDDescriptor *pPIDDescriptor) {
	if (pPIDDescriptor != NULL) {
		if (pPIDDescriptor->pAVContext != NULL)
			delete pPIDDescriptor->pAVContext;
		delete pPIDDescriptor;
	}
}

bool TSParser::ProcessPidTypePAT(uint32_t packetHeader,
		PIDDescriptor &pidDescriptor, uint8_t *pBuffer, uint32_t &cursor,
		uint32_t maxCursor) {
	//1. Advance the pointer field
	if (TS_TRANSPORT_PACKET_IS_PAYLOAD_START(packetHeader)) {
		TS_CHECK_BOUNDS(1);
		TS_CHECK_BOUNDS(pBuffer[cursor]);
		cursor += pBuffer[cursor] + 1;
	}

	//1. Get the crc from the packet and compare it with the last crc.
	//if it is the same, ignore this packet
	uint32_t crc = TSPacketPAT::PeekCRC(pBuffer, cursor, maxCursor);
	if (crc == 0) {
		FATAL("Unable to read crc");
		return false;
	}
	if (pidDescriptor.crc == crc) {
		if (_pEventSink != NULL) {
			_pEventSink->SignalPAT(NULL);
		}
		return true;
	}

	//2. read the packet
	TSPacketPAT packetPAT;
	if (!packetPAT.Read(pBuffer, cursor, maxCursor)) {
		FATAL("Unable to read PAT");
		return false;
	}

	//3. Store the crc
	pidDescriptor.crc = packetPAT.GetCRC();


	//4. Store the pid types found in PAT

	FOR_MAP(packetPAT.GetPMTs(), uint16_t, uint16_t, i) {
		PIDDescriptor *pDescr = new PIDDescriptor;
		pDescr->pid = MAP_VAL(i);
		pDescr->type = PID_TYPE_PMT;
		pDescr->crc = 0;
		pDescr->pAVContext = NULL;
		_pidMapping[pDescr->pid] = pDescr;
	}

	FOR_MAP(packetPAT.GetNITs(), uint16_t, uint16_t, i) {
		PIDDescriptor *pDescr = new PIDDescriptor;
		pDescr->pid = MAP_VAL(i);
		pDescr->type = PID_TYPE_NIT;
		pDescr->pAVContext = NULL;
		_pidMapping[pDescr->pid] = pDescr;
	}

	if (_pEventSink != NULL) {
		_pEventSink->SignalPAT(&packetPAT);
	}

	//5. Done
	return true;
}

bool TSParser::ProcessPidTypePMT(uint32_t packetHeader,
		PIDDescriptor &pidDescriptor, uint8_t *pBuffer, uint32_t &cursor,
		uint32_t maxCursor) {
	//1. Advance the pointer field
	if (TS_TRANSPORT_PACKET_IS_PAYLOAD_START(packetHeader)) {
		TS_CHECK_BOUNDS(1);
		TS_CHECK_BOUNDS(pBuffer[cursor]);
		cursor += pBuffer[cursor] + 1;
	}

	//1. Get the crc from the packet and compare it with the last crc.
	//if it is the same, ignore this packet. Also test if we have a protocol handler
	uint32_t crc = TSPacketPMT::PeekCRC(pBuffer, cursor, maxCursor);
	if (crc == 0) {
		FATAL("Unable to read crc");
		return false;
	}
	if (pidDescriptor.crc == crc) {
		if (_pEventSink != NULL) {
			_pEventSink->SignalPMT(NULL);
		}
		return true;
	}

	//2. read the packet
	TSPacketPMT packetPMT;
	if (!packetPMT.Read(pBuffer, cursor, maxCursor)) {
		FATAL("Unable to read PAT");
		return false;
	}
	if (_pEventSink != NULL) {
		_pEventSink->SignalPMT(&packetPMT);
	}

	//3. Store the CRC
	pidDescriptor.crc = packetPMT.GetCRC();

	//4. Gather the info about the streams present inside the program
	//videoPid will contain the selected video stream
	//audioPid will contain the selected audio stream
	//unknownPids will contain the rest of the streams that will be ignored
	map<uint16_t, uint16_t> unknownPids;
	vector<PIDDescriptor *> pidDescriptors;
	PIDDescriptor *pPIDDescriptor = NULL;

	map<uint16_t, TSStreamInfo> streamsInfo = packetPMT.GetStreamsInfo();
	if (_pEventSink != NULL) {
		if (!_pEventSink->SignalStreamsPIDSChanged(streamsInfo)) {
			FATAL("SignalStreamsPIDSChanged failed");
			return false;
		}
	}

	bool ignore = true;
	PIDType pidType = PID_TYPE_NULL;
	BaseAVContext *pAVContext = NULL;

	FOR_MAP(streamsInfo, uint16_t, TSStreamInfo, i) {
		ignore = true;
		pidType = PID_TYPE_NULL;
		pAVContext = NULL;
		switch (MAP_VAL(i).streamType) {
			case TS_STREAMTYPE_VIDEO_H264:
			{
				pidType = PID_TYPE_VIDEOSTREAM;
				pAVContext = new H264AVContext();
				if (_pEventSink != NULL) {
					if (!_pEventSink->SignalStreamPIDDetected(MAP_VAL(i),
							pAVContext, pidType, ignore)) {
						FATAL("SignalStreamsPIDSChanged failed");
						return false;
					}
				}
				break;
			}
			case TS_STREAMTYPE_AUDIO_AAC:
			{
				pidType = PID_TYPE_AUDIOSTREAM;
				pAVContext = new AACAVContext();
				if (_pEventSink != NULL) {
					if (!_pEventSink->SignalStreamPIDDetected(MAP_VAL(i),
							pAVContext, pidType, ignore)) {
						FATAL("SignalStreamsPIDSChanged failed");
						return false;
					}
				}
				break;
			}
			default:
			{
				if (_pEventSink != NULL)
					_pEventSink->SignalUnknownPIDDetected(MAP_VAL(i));
				break;
			}
		}
		if (ignore) {
			pidType = PID_TYPE_NULL;
			if (pAVContext != NULL) {
				delete pAVContext;
				pAVContext = NULL;
			}
		}
		pPIDDescriptor = new PIDDescriptor();
		pPIDDescriptor->pid = MAP_KEY(i);
		pPIDDescriptor->type = pidType;
		pPIDDescriptor->pAVContext = pAVContext;
		if (pPIDDescriptor->pAVContext != NULL)
			pPIDDescriptor->pAVContext->_pEventsSink = _pEventSink;
		ADD_VECTOR_END(pidDescriptors, pPIDDescriptor);
	}

	//7. Mount the newly created pids

	FOR_VECTOR(pidDescriptors, i) {
		if (MAP_HAS1(_pidMapping, pidDescriptors[i]->pid)) {
			FreePidDescriptor(_pidMapping[pidDescriptors[i]->pid]);
		}
		_pidMapping[pidDescriptors[i]->pid] = pidDescriptors[i];
	}

	if (_pEventSink != NULL) {
		_pEventSink->SignalPMTComplete();
	}

	return true;
}

bool TSParser::ProcessPidTypeAV(PIDDescriptor *pPIDDescriptor, uint8_t *pData,
		uint32_t length, bool packetStart, int8_t sequenceNumber) {
	if (pPIDDescriptor->pAVContext == NULL) {
		FATAL("No AVContext cerated");
		return false;
	}

	if (pPIDDescriptor->pAVContext->_sequenceNumber == -1) {
		pPIDDescriptor->pAVContext->_sequenceNumber = sequenceNumber;
	} else {
		if (((pPIDDescriptor->pAVContext->_sequenceNumber + 1)&0x0f) != sequenceNumber) {
			pPIDDescriptor->pAVContext->_sequenceNumber = sequenceNumber;
			pPIDDescriptor->pAVContext->DropPacket();
			return true;
		} else {
			pPIDDescriptor->pAVContext->_sequenceNumber = sequenceNumber;
		}
	}

	if (packetStart) {
		if (!pPIDDescriptor->pAVContext->HandleData()) {
			FATAL("Unable to handle AV data");
			return false;
		}
		if (length >= 8) {
			if (((pData[3]&0xE0) != 0xE0)&&((pData[3]&0xC0) != 0xC0)) {
				BaseInStream *pTemp = pPIDDescriptor->pAVContext->GetInStream();
				WARN("PID %"PRIu16" from %s is not h264/aac. The type is 0x%02"PRIx8,
						pPIDDescriptor->pid,
						pTemp != NULL ? STR(*pTemp) : "",
						pData[3]);
				pPIDDescriptor->type = PID_TYPE_NULL;
				return true;
			}

			uint32_t pesHeaderLength = pData[8];
			if (pesHeaderLength + 9 > length) {
				WARN("Not enough data");
				pPIDDescriptor->pAVContext->DropPacket();
				return true;
			}

			//compute the time:
			//http://dvd.sourceforge.net/dvdinfo/pes-hdr.html
			uint8_t *pPTS = NULL;
			uint8_t *pDTS = NULL;

			uint8_t ptsdstflags = pData[7] >> 6;

			if (ptsdstflags == 2) {
				pPTS = pData + 9;
			} else if (ptsdstflags == 3) {
				pPTS = pData + 9;
				pDTS = pData + 14;
			}

			if (pPTS != NULL) {
				uint64_t value = (pPTS[0]&0x0f) >> 1;
				value = (value << 8) | pPTS[1];
				value = (value << 7) | (pPTS[2] >> 1);
				value = (value << 8) | pPTS[3];
				value = (value << 7) | (pPTS[4] >> 1);
				if (((pPIDDescriptor->pAVContext->_pts.lastRaw >> 32) == 1) && ((value >> 32) == 0)) {
					pPIDDescriptor->pAVContext->_pts.rollOverCount++;
				}
				pPIDDescriptor->pAVContext->_pts.lastRaw = value;
				value += (pPIDDescriptor->pAVContext->_pts.rollOverCount * 0x1ffffffffLL);
				pPIDDescriptor->pAVContext->_pts.time = (double) value / 90.00;
			} else {
				WARN("No PTS!");
				pPIDDescriptor->pAVContext->DropPacket();
				return true;
			}

			double tempDtsTime = 0;
			if (pDTS != NULL) {
				uint64_t value = (pDTS[0]&0x0f) >> 1;
				value = (value << 8) | pDTS[1];
				value = (value << 7) | (pDTS[2] >> 1);
				value = (value << 8) | pDTS[3];
				value = (value << 7) | (pDTS[4] >> 1);
				if (((pPIDDescriptor->pAVContext->_dts.lastRaw >> 32) == 1) && ((value >> 32) == 0)) {
					pPIDDescriptor->pAVContext->_dts.rollOverCount++;
				}
				pPIDDescriptor->pAVContext->_dts.lastRaw = value;
				value += (pPIDDescriptor->pAVContext->_dts.rollOverCount * 0x1ffffffffLL);
				tempDtsTime = (double) value / 90.00;
			} else {
				tempDtsTime = pPIDDescriptor->pAVContext->_pts.time;
			}

			if (pPIDDescriptor->pAVContext->_dts.time > tempDtsTime) {
				WARN("Back timestamp: %.2f -> %.2f on pid %"PRIu16,
						pPIDDescriptor->pAVContext->_dts.time, tempDtsTime, pPIDDescriptor->pid);
				pPIDDescriptor->pAVContext->DropPacket();
				return true;
			}
			pPIDDescriptor->pAVContext->_dts.time = tempDtsTime;

			pData += 9 + pesHeaderLength;
			length -= (9 + pesHeaderLength);

		} else {
			WARN("Not enoght data");
			pPIDDescriptor->pAVContext->DropPacket();
			return true;
		}
	}

	pPIDDescriptor->pAVContext->_bucket.ReadFromBuffer(pData, length);

	return true;
}

#endif	/* HAS_MEDIA_TS */
