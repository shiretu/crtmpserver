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
#ifndef _BASEOUTNETRTMPSTREAM_H
#define	_BASEOUTNETRTMPSTREAM_H

#include "streaming/baseoutnetstream.h"
#include "protocols/rtmp/header.h"
#include "protocols/rtmp/channel.h"
#include "mediaformats/readers/streammetadataresolver.h"

class BaseRTMPProtocol;

class DLLEXP BaseOutNetRTMPStream
: public BaseOutNetStream {
private:
	uint32_t _rtmpStreamId;
	uint32_t _chunkSize;
	BaseRTMPProtocol *_pRTMPProtocol;
	double _seekTime;
	double _start;

	uint32_t _isFirstVideoFrame;
	Header _videoHeader;
	IOBuffer _videoBucket;

	uint32_t _isFirstAudioFrame;
	Header _audioHeader;
	IOBuffer _audioBucket;

	Channel *_pChannelAudio;
	Channel *_pChannelVideo;
	Channel *_pChannelCommands;
	uint32_t _feederChunkSize;
	bool _canDropFrames;
	bool _audioCurrentFrameDropped;
	bool _videoCurrentFrameDropped;
	uint32_t _maxBufferSize;
	uint64_t _attachedStreamType;
	string _clientId;
	bool _paused;

	bool _sendOnStatusPlayMessages;
	PublicMetadata _metadata;
	uint64_t _metaFileSize;
	double _metaFileDuration;

	bool _absoluteTimestamps;
protected:
	BaseOutNetRTMPStream(BaseProtocol *pProtocol, uint64_t type, string name,
			uint32_t rtmpStreamId, uint32_t chunkSize);
public:
	static BaseOutNetRTMPStream *GetInstance(BaseProtocol *pProtocol,
			StreamsManager *pStreamsManager,
			string name, uint32_t rtmpStreamId,
			uint32_t chunkSize,
			uint64_t inStreamType);
	virtual ~BaseOutNetRTMPStream();

	uint32_t GetRTMPStreamId();
	uint32_t GetCommandsChannelId();
	void SetChunkSize(uint32_t chunkSize);
	uint32_t GetChunkSize();
	void SetFeederChunkSize(uint32_t feederChunkSize);
	bool CanDropFrames();
	void CanDropFrames(bool canDropFrames);
	void SetSendOnStatusPlayMessages(bool value);
	virtual void GetStats(Variant &info, uint32_t namespaceId = 0);

	virtual bool SendStreamMessage(Variant &message);
	virtual void SignalAttachedToInStream();
	virtual void SignalDetachedFromInStream();

	virtual bool SignalPlay(double &dts, double &length);
	virtual bool SignalPause();
	virtual bool SignalResume();
	virtual bool SignalSeek(double &dts);
	virtual bool SignalStop();

	virtual void SignalStreamCompleted();
	virtual void SignalAudioStreamCapabilitiesChanged(
			StreamCapabilities *pCapabilities, AudioCodecInfo *pOld,
			AudioCodecInfo *pNew);
	virtual void SignalVideoStreamCapabilitiesChanged(
			StreamCapabilities *pCapabilities, VideoCodecInfo *pOld,
			VideoCodecInfo *pNew);
protected:
	virtual bool InternalFeedData(uint8_t *pData, uint32_t dataLength,
			uint32_t processedLength, uint32_t totalLength,
			double dts, bool isAudio);
	bool FeedAudioCodecBytes(StreamCapabilities *pCapabilities, double dts, bool isAbsolute);
	bool FeedVideoCodecBytes(StreamCapabilities *pCapabilities, double dts, bool isAbsolute);
private:
	bool ChunkAndSend(uint8_t *pData, uint32_t length, IOBuffer &bucket,
			Header &header, Channel &channel);
	bool AllowExecution(uint32_t totalProcessed, uint32_t dataLength, bool isAudio);
	void InternalReset();
	void GetMetadata();
	bool SendOnMetadata();
};


#endif	/* _BASEOUTNETRTMPSTREAM_H */

#endif /* HAS_PROTOCOL_RTMP */

