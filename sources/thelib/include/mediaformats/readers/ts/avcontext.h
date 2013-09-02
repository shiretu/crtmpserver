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
#ifndef _AVCONTEXT_H
#define	_AVCONTEXT_H

#include "common.h"

class StreamCapabilities;
class TSParserEventsSink;
class BaseInStream;

class BaseAVContext {
public:

	struct {
		double time;
		uint64_t lastRaw;
		uint32_t rollOverCount;
	} _pts, _dts;
	int8_t _sequenceNumber;
	uint64_t _droppedPacketsCount;
	uint64_t _droppedBytesCount;
	uint64_t _packetsCount;
	uint64_t _bytesCount;
	IOBuffer _bucket;
	StreamCapabilities *_pStreamCapabilities;
	TSParserEventsSink *_pEventsSink;
public:
	BaseAVContext();
	virtual ~BaseAVContext();
	virtual void Reset();
	void DropPacket();
	virtual bool HandleData() = 0;
	bool FeedData(uint8_t *pData, uint32_t dataLength, double pts, double dts,
			bool isAudio);
	BaseInStream *GetInStream();
private:
	void InternalReset();
};

class H264AVContext
: public BaseAVContext {
private:
	IOBuffer _SPS;
	IOBuffer _PPS;
	vector<IOBuffer *> _backBuffers;
	vector<IOBuffer *> _backBuffersCache;
	double _backBuffersPts;
	double _backBuffersDts;
public:
	H264AVContext();
	virtual ~H264AVContext();
	virtual void Reset();
	virtual bool HandleData();
private:
	void EmptyBackBuffers();
	void DiscardBackBuffers();
	void InsertBackBuffer(uint8_t *pBuffer, int32_t length);
	bool ProcessNal(uint8_t *pBuffer, int32_t length, double pts, double dts);
	void InitializeCapabilities(uint8_t *pData, uint32_t length);
	void InternalReset();
};

class AACAVContext
: public BaseAVContext {
private:
	double _lastSentTimestamp;
	double _samplingRate;
	bool _initialMarkerFound;
	uint32_t _markerRetryCount;
public:
	AACAVContext();
	virtual ~AACAVContext();
	virtual void Reset();
	virtual bool HandleData();
private:
	void InitializeCapabilities(uint8_t *pData, uint32_t length);
	void InternalReset();
};
#endif	/* _AVCONTEXT_H */
#endif	/* HAS_MEDIA_TS */
