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
#ifndef _TSFRAMEREADER_H
#define	_TSFRAMEREADER_H

#include "mediaformats/readers/ts/tsparser.h"
#include "mediaformats/readers/ts/tsparsereventsink.h"
#include "mediaformats/readers/mediafile.h"
#include "mediaformats/readers/mediaframe.h"
#include "streaming/streamcapabilities.h"

class TSFrameReaderInterface;
class BaseInStream;

class TSFrameReader
: public TSParser, TSParserEventsSink {
private:
	MediaFile *_pFile;
	bool _freeFile;
	uint8_t _chunkSizeDetectionCount;
	uint8_t _chunkSize;
	uint32_t _defaultBlockSize;
	bool _chunkSizeErrors;
	IOBuffer _chunkBuffer;
	bool _frameAvailable;
	StreamCapabilities _streamCapabilities;
	bool _eof;
	TSFrameReaderInterface *_pInterface;
public:
	TSFrameReader(TSFrameReaderInterface *pInterface);
	virtual ~TSFrameReader();

	bool SetFile(string filePath);
	bool IsEOF();
	bool Seek(uint64_t offset);
	bool ReadFrame();

	virtual BaseInStream *GetInStream();
	virtual void SignalResetChunkSize();
	virtual void SignalPAT(TSPacketPAT *pPAT);
	virtual void SignalPMT(TSPacketPMT *pPMT);
	virtual void SignalPMTComplete();
	virtual bool SignalStreamsPIDSChanged(map<uint16_t, TSStreamInfo> &streams);
	virtual bool SignalStreamPIDDetected(TSStreamInfo &streamInfo,
			BaseAVContext *pContext, PIDType type, bool &ignore);
	virtual void SignalUnknownPIDDetected(TSStreamInfo &streamInfo);
	virtual bool FeedData(BaseAVContext *pContext, uint8_t *pData,
			uint32_t dataLength, double pts, double dts, bool isAudio);
private:
	void FreeFile();
	bool DetermineChunkSize();
	bool TestChunkSize(uint8_t chunkSize);
	bool GetByteAt(uint64_t offset, uint8_t &byte);
};


#endif	/* _TSFRAMEREADER_H */
#endif	/* HAS_MEDIA_TS */

