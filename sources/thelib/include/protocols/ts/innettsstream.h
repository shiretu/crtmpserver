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
#ifndef _INNETTSSTREAM_H
#define	_INNETTSSTREAM_H

#include "streaming/baseinnetstream.h"
#include "streaming/streamcapabilities.h"

class DLLEXP InNetTSStream
: public BaseInNetStream {
private:
	//audio section
	bool _hasAudio;

	//video section
	bool _hasVideo;

	StreamCapabilities _streamCapabilities;
	bool _enabled;
public:
	InNetTSStream(BaseProtocol *pProtocol, string name, uint32_t bandwidthHint);
	virtual ~InNetTSStream();
	virtual StreamCapabilities * GetCapabilities();

	bool HasAudio();
	void HasAudio(bool value);
	bool HasVideo();
	void HasVideo(bool value);
	void Enable(bool value);

	virtual bool FeedData(uint8_t *pData, uint32_t dataLength,
			uint32_t processedLength, uint32_t totalLength,
			double pts, double dts, bool isAudio);
	virtual void ReadyForSend();
	virtual bool IsCompatibleWithType(uint64_t type);
	virtual void SignalOutStreamAttached(BaseOutStream *pOutStream);
	virtual void SignalOutStreamDetached(BaseOutStream *pOutStream);
	virtual bool SignalPlay(double &dts, double &length);
	virtual bool SignalPause();
	virtual bool SignalResume();
	virtual bool SignalSeek(double &dts);
	virtual bool SignalStop();
};


#endif	/* _INNETTSSTREAM_H */
#endif	/* defined HAS_PROTOCOL_TS && defined HAS_MEDIA_TS */

