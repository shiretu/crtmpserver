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


#if defined HAS_PROTOCOL_TS && defined HAS_MEDIA_TS
#ifndef _INBOUNDTSPROTOCOL_H
#define	_INBOUNDTSPROTOCOL_H

#include "protocols/baseprotocol.h"
#include "mediaformats/readers/ts/pidtypes.h"
#include "mediaformats/readers/ts/tspacketheader.h"
#include "mediaformats/readers/ts/piddescriptor.h"
#include "mediaformats/readers/ts/tsparsereventsink.h"

#define TS_CHUNK_188 188
#define TS_CHUNK_204 204
#define TS_CHUNK_208 208

class TSParser;
class BaseTSAppProtocolHandler;
class InNetTSStream;

class DLLEXP InboundTSProtocol
: public BaseProtocol, TSParserEventsSink {
private:
	uint32_t _chunkSizeDetectionCount;
	uint32_t _chunkSize;
	BaseTSAppProtocolHandler *_pProtocolHandler;
	TSParser *_pParser;
	InNetTSStream *_pInStream;
public:
	InboundTSProtocol();
	virtual ~InboundTSProtocol();

	virtual bool Initialize(Variant &parameters);
	virtual bool AllowFarProtocol(uint64_t type);
	virtual bool AllowNearProtocol(uint64_t type);
	virtual bool SignalInputData(int32_t recvAmount);
	virtual bool SignalInputData(IOBuffer &buffer, sockaddr_in *pPeerAddress);
	virtual bool SignalInputData(IOBuffer &buffer);

	virtual void SetApplication(BaseClientApplication *pApplication);
	BaseTSAppProtocolHandler *GetProtocolHandler();
	uint32_t GetChunkSize();

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
	bool DetermineChunkSize(IOBuffer &buffer);
};


#endif	/* _INBOUNDTSPROTOCOL_H */
#endif	/* defined HAS_PROTOCOL_TS && defined HAS_MEDIA_TS */

