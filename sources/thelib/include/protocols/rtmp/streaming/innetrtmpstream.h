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


#ifdef HAS_PROTOCOL_RTMP
#ifndef _INNETRTMPSTREAM_H
#define	_INNETRTMPSTREAM_H

#include "streaming/baseinnetstream.h"
#include "streaming/streamcapabilities.h"
#include "mediaformats/readers/streammetadataresolver.h"

class BaseRTMPProtocol;
class BaseOutStream;

class DLLEXP InNetRTMPStream
: public BaseInNetStream {
private:
	uint32_t _rtmpStreamId;
	uint32_t _chunkSize;
	uint32_t _channelId;
	string _clientId;
	int32_t _videoCts;

	bool _dummy;
	uint8_t _lastAudioCodec;
	uint8_t _lastVideoCodec;
	StreamCapabilities _streamCapabilities;

	IOBuffer _aggregate;
public:
	InNetRTMPStream(BaseProtocol *pProtocol, string name, uint32_t rtmpStreamId,
			uint32_t chunkSize, uint32_t channelId);
	virtual ~InNetRTMPStream();
	virtual StreamCapabilities * GetCapabilities();

	virtual void ReadyForSend();
	virtual bool IsCompatibleWithType(uint64_t type);

	uint32_t GetRTMPStreamId();
	uint32_t GetChunkSize();
	void SetChunkSize(uint32_t chunkSize);

	bool SendStreamMessage(Variant &message);
	virtual bool SendStreamMessage(string functionName, Variant &parameters);
	bool SendOnStatusStreamPublished();
	bool RecordFLV(Metadata &meta, bool append);
	bool RecordMP4(Metadata &meta);

	virtual void SignalOutStreamAttached(BaseOutStream *pOutStream);
	virtual void SignalOutStreamDetached(BaseOutStream *pOutStream);
	virtual bool SignalPlay(double &dts, double &length);
	virtual bool SignalPause();
	virtual bool SignalResume();
	virtual bool SignalSeek(double &dts);
	virtual bool SignalStop();
	virtual bool FeedDataAggregate(uint8_t *pData, uint32_t dataLength,
			uint32_t processedLength, uint32_t totalLength,
			double pts, double dts, bool isAudio);
	virtual bool FeedData(uint8_t *pData, uint32_t dataLength,
			uint32_t processedLength, uint32_t totalLength,
			double pts, double dts, bool isAudio);

	static bool InitializeAudioCapabilities(BaseInStream *pStream,
			StreamCapabilities &streamCapabilities,
			bool &capabilitiesInitialized, uint8_t *pData, uint32_t length);
	static bool InitializeVideoCapabilities(BaseInStream *pStream,
			StreamCapabilities &streamCapabilities,
			bool &capabilitiesInitialized, uint8_t *pData, uint32_t length);
	virtual uint32_t GetInputVideoTimescale();
	virtual uint32_t GetInputAudioTimescale();
private:
	string GetRecordedFileName(Metadata &meta);
	BaseRTMPProtocol *GetRTMPProtocol();
};


#endif	/* _INNETRTMPSTREAM_H */

#endif /* HAS_PROTOCOL_RTMP */

