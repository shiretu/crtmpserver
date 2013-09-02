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
#ifndef _TSPARSEREVENTSINK_H
#define	_TSPARSEREVENTSINK_H

#include "common.h"
#include "mediaformats/readers/ts/tsstreaminfo.h"
#include "mediaformats/readers/ts/pidtypes.h"

class TSPacketPAT;
class TSPacketPMT;
class BaseAVContext;
class BaseInStream;

class TSParserEventsSink {
public:

	virtual ~TSParserEventsSink() {
	}
	virtual BaseInStream *GetInStream() = 0;
	virtual void SignalResetChunkSize() = 0;
	virtual void SignalPAT(TSPacketPAT *pPAT) = 0;
	virtual void SignalPMT(TSPacketPMT *pPMT) = 0;
	virtual void SignalPMTComplete() = 0;
	virtual bool SignalStreamsPIDSChanged(map<uint16_t, TSStreamInfo> &streams) = 0;
	virtual bool SignalStreamPIDDetected(TSStreamInfo &streamInfo,
			BaseAVContext *pContext, PIDType type, bool &ignore) = 0;
	virtual void SignalUnknownPIDDetected(TSStreamInfo &streamInfo) = 0;
	virtual bool FeedData(BaseAVContext *pContext, uint8_t *pData,
			uint32_t dataLength, double pts, double dts, bool isAudio) = 0;
};

#endif	/* _TSPARSEREVENTSINK_H */
#endif	/* HAS_MEDIA_TS */
 
