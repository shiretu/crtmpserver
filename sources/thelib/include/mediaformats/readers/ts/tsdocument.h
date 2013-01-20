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
#ifndef _TSDOCUMENT_H
#define	_TSDOCUMENT_H

#include "common.h"
#include "mediaformats/readers/basemediadocument.h"
#include "mediaformats/readers/ts/tsparsereventsink.h"

class TSParser;
class BaseInStream;

class TSDocument
: public BaseMediaDocument, TSParserEventsSink {
private:
	uint8_t _chunkSizeDetectionCount;
	uint8_t _chunkSize;
	TSParser *_pParser;
	bool _chunkSizeErrors;
	uint64_t _lastOffset;
public:
	TSDocument(Metadata &metadata);
	virtual ~TSDocument();

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
protected:
	virtual bool ParseDocument();
	virtual bool BuildFrames();
	virtual Variant GetPublicMeta();
private:
	void AddFrame(double pts, double dts, uint8_t frameType);
	bool DetermineChunkSize();
	bool GetByteAt(uint64_t offset, uint8_t &byte);
	bool TestChunkSize(uint8_t chunkSize);
};


#endif	/* _TSDOCUMENT_H */
#endif /* HAS_MEDIA_TS */
