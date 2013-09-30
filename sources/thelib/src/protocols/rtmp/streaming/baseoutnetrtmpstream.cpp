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
#include "protocols/rtmp/streaming/baseoutnetrtmpstream.h"
#include "streaming/baseinstream.h"
#include "protocols/rtmp/basertmpprotocol.h"
#include "protocols/rtmp/streaming/innetrtmpstream.h"
#include "protocols/rtmp/streaming/infilertmpstream.h"
#include "protocols/rtmp/messagefactories/streammessagefactory.h"
#include "protocols/rtmp/messagefactories/genericmessagefactory.h"
#include "streaming/streamstypes.h"
#include "protocols/liveflv/innetliveflvstream.h"
#include "protocols/rtmp/streaming/outnetrtmp4rtmpstream.h"
#include "protocols/rtmp/streaming/outnetrtmp4tsstream.h"
#include "application/baseclientapplication.h"
#include "streaming/codectypes.h"

//#define TRACK_HEADER(header,processed) do{if(processed==0) FINEST("%s",STR(header));}while(0)
#define TRACK_HEADER(header,processed)

//#define TRACK_PAYLOAD(processedLength,dataLength,pData) do {if (processedLength == 0) {uint32_t ___temp = dataLength > 32 ? 32 : dataLength; FINEST("%s", STR(hex(pData, ___temp)));}} while (0)
#define TRACK_PAYLOAD(processedLength,dataLength,pData)

//#define TRACK_MESSAGE(...) FINEST(__VA_ARGS__)
#define TRACK_MESSAGE(...)

#define ALLOW_EXECUTION(feederProcessed,dataLength,isAudio) if (!AllowExecution(feederProcessed,dataLength,isAudio)) { /*FINEST("We are not allowed to send this data");*/ return true; }

//if needed, we can simulate dropping frames
//the number represents the percent of frames that we will drop (0-100)
//#define SIMULATE_DROPPING_FRAMES 40

BaseOutNetRTMPStream::BaseOutNetRTMPStream(BaseProtocol *pProtocol, uint64_t type,
		string name, uint32_t rtmpStreamId,
		uint32_t chunkSize)
: BaseOutNetStream(pProtocol, type, name) {
	if (!TAG_KIND_OF(type, ST_OUT_NET_RTMP)) {
		ASSERT("Incorrect stream type. Wanted a stream type in class %s and got %s",
				STR(tagToString(ST_OUT_NET_RTMP)), STR(tagToString(type)));
	}
	_rtmpStreamId = rtmpStreamId;
	_chunkSize = chunkSize;
	_pRTMPProtocol = (BaseRTMPProtocol *) pProtocol;
	_pChannelAudio = _pRTMPProtocol->ReserveChannel();
	_pChannelVideo = _pRTMPProtocol->ReserveChannel();
	_pChannelCommands = _pRTMPProtocol->ReserveChannel();

	_feederChunkSize = 0xffffffff;
	_canDropFrames = true;
	_audioCurrentFrameDropped = false;
	_videoCurrentFrameDropped = false;
	_attachedStreamType = 0;
	_clientId = format("%d_%d_%"PRIz"u", _pProtocol->GetId(), _rtmpStreamId, (size_t)this);

	_paused = false;

	_sendOnStatusPlayMessages = true;
	_metaFileSize = 0;
	_metaFileDuration = 0;

	if ((pProtocol != NULL)
			&& (pProtocol->GetApplication() != NULL)
			&& (pProtocol->GetApplication()->GetConfiguration().HasKeyChain(_V_NUMERIC, false, 1, "maxRtmpOutBuffer"))) {
		_maxBufferSize = (uint32_t) pProtocol->GetApplication()->GetConfiguration().GetValue("maxRtmpOutBuffer", false);
	} else {
		_maxBufferSize = 65536 * 2;
	}

	_absoluteTimestamps = false;
	if (pProtocol != NULL) {
		Variant &params = pProtocol->GetCustomParameters();
		if (params.HasKeyChain(V_BOOL, false, 3, "customParameters", "localStreamConfig", "rtmpAbsoluteTimestamps")) {
			_absoluteTimestamps = (bool)(params.GetValue("customParameters", false)
					.GetValue("localStreamConfig", false)
					.GetValue("rtmpAbsoluteTimestamps", false));
		}
	}

	InternalReset();
}

BaseOutNetRTMPStream *BaseOutNetRTMPStream::GetInstance(BaseProtocol *pProtocol,
		StreamsManager *pStreamsManager,
		string name, uint32_t rtmpStreamId,
		uint32_t chunkSize,
		uint64_t inStreamType) {
	BaseOutNetRTMPStream *pResult = NULL;
	if (TAG_KIND_OF(inStreamType, ST_IN_NET_RTMP)
			|| TAG_KIND_OF(inStreamType, ST_IN_NET_LIVEFLV)
			|| TAG_KIND_OF(inStreamType, ST_IN_FILE_RTMP)
			) {
		pResult = new OutNetRTMP4RTMPStream(pProtocol, name, rtmpStreamId, chunkSize);
	} else if (TAG_KIND_OF(inStreamType, ST_IN_NET_TS)
			|| TAG_KIND_OF(inStreamType, ST_IN_NET_RTP)) {
		pResult = new OutNetRTMP4TSStream(pProtocol, name, rtmpStreamId, chunkSize);
	} else {
		FATAL("Can't instantiate a network rtmp outbound stream for type %s",
				STR(tagToString(inStreamType)));
	}

	if (pResult != NULL) {
		if (!pResult->SetStreamsManager(pStreamsManager)) {
			FATAL("Unable to set the streams manager");
			delete pResult;
			pResult = NULL;
			return NULL;
		}
		if ((pResult->_pChannelAudio == NULL)
				|| (pResult->_pChannelVideo == NULL)
				|| (pResult->_pChannelCommands == NULL)) {
			FATAL("No more channels left");
			delete pResult;
			pResult = NULL;
		}
	}

	return pResult;
}

BaseOutNetRTMPStream::~BaseOutNetRTMPStream() {
	_pRTMPProtocol->ReleaseChannel(_pChannelAudio);
	_pRTMPProtocol->ReleaseChannel(_pChannelVideo);
	_pRTMPProtocol->ReleaseChannel(_pChannelCommands);
}

uint32_t BaseOutNetRTMPStream::GetRTMPStreamId() {
	return _rtmpStreamId;
}

uint32_t BaseOutNetRTMPStream::GetCommandsChannelId() {
	return 3;
}

void BaseOutNetRTMPStream::SetChunkSize(uint32_t chunkSize) {
	_chunkSize = chunkSize;
}

uint32_t BaseOutNetRTMPStream::GetChunkSize() {
	return _chunkSize;
}

void BaseOutNetRTMPStream::SetFeederChunkSize(uint32_t feederChunkSize) {
	_feederChunkSize = feederChunkSize;
}

bool BaseOutNetRTMPStream::CanDropFrames() {
	return _canDropFrames;
}

void BaseOutNetRTMPStream::CanDropFrames(bool canDropFrames) {
	_canDropFrames = canDropFrames;
}

void BaseOutNetRTMPStream::SetSendOnStatusPlayMessages(bool value) {
	_sendOnStatusPlayMessages = value;
}

void BaseOutNetRTMPStream::GetStats(Variant &info, uint32_t namespaceId) {
	BaseOutNetStream::GetStats(info, namespaceId);
	info["canDropFrames"] = (bool)_canDropFrames;
}

bool BaseOutNetRTMPStream::SendStreamMessage(Variant &message) {
	//1. Set the channel id
	VH_CI(message) = (uint32_t) 3;

	//2. Reset the timer
	VH_TS(message) = (uint32_t) _pChannelAudio->lastOutAbsTs > _pChannelVideo->lastOutAbsTs ?
			_pChannelAudio->lastOutAbsTs : _pChannelVideo->lastOutAbsTs;

	//3. Set as absolute ts
	VH_IA(message) = true;

	//4. Set the stream id
	VH_SI(message) = _rtmpStreamId;

	//5. Send it
	return _pRTMPProtocol->SendMessage(message);
}

void BaseOutNetRTMPStream::SignalAttachedToInStream() {
	//1. Store the attached stream type to know how we should proceed on detach
	_attachedStreamType = _pInStream->GetType();

	//2. Mirror the feeder chunk size
	if (TAG_KIND_OF(_attachedStreamType, ST_IN_NET_RTMP)) {
		_feederChunkSize = ((InNetRTMPStream *) _pInStream)->GetChunkSize();
	} else if (TAG_KIND_OF(_attachedStreamType, ST_IN_FILE)) {
		_feederChunkSize = ((InFileRTMPStream *) _pInStream)->GetChunkSize();
	} else {
		_feederChunkSize = 0xffffffff;
	}

	//4. Store the file metadata
	GetMetadata();

	Variant message;

	//5. Send abort messages on audio/video channels
	if (_pChannelAudio->lastOutProcBytes != 0) {
		message = GenericMessageFactory::GetAbortMessage(_pChannelAudio->id);
		TRACK_MESSAGE("Message:\n%s", STR(message.ToString()));
		if (!_pRTMPProtocol->SendMessage(message)) {
			FATAL("Unable to send message");
			_pRTMPProtocol->EnqueueForDelete();
			return;
		}
		_pChannelAudio->Reset();
	}

	if (_pChannelVideo->lastOutProcBytes != 0) {
		message = GenericMessageFactory::GetAbortMessage(_pChannelVideo->id);
		TRACK_MESSAGE("Message:\n%s", STR(message.ToString()));
		if (!_pRTMPProtocol->SendMessage(message)) {
			FATAL("Unable to send message");
			_pRTMPProtocol->EnqueueForDelete();
			return;
		}
		_pChannelVideo->Reset();
	}

	//6. Stream is recorded
	if (TAG_KIND_OF(_attachedStreamType, ST_IN_FILE)) {
		message = StreamMessageFactory::GetUserControlStreamIsRecorded(_rtmpStreamId);
		TRACK_MESSAGE("Message:\n%s", STR(message.ToString()));
		if (!_pRTMPProtocol->SendMessage(message)) {
			FATAL("Unable to send message");
			_pRTMPProtocol->EnqueueForDelete();
			return;
		}
	}

	//7. Stream begin
	message = StreamMessageFactory::GetUserControlStreamBegin(_rtmpStreamId);
	TRACK_MESSAGE("Message:\n%s", STR(message.ToString()));
	if (!_pRTMPProtocol->SendMessage(message)) {
		FATAL("Unable to send message");
		_pRTMPProtocol->EnqueueForDelete();
		return;
	}

	if (_sendOnStatusPlayMessages) {
		//8. Send NetStream.Play.Reset
		message = StreamMessageFactory::GetInvokeOnStatusStreamPlayReset(
				_pChannelAudio->id, _rtmpStreamId, 0, true, 0, "reset...", GetName(),
				_clientId);
		TRACK_MESSAGE("Message:\n%s", STR(message.ToString()));
		if (!_pRTMPProtocol->SendMessage(message)) {
			FATAL("Unable to send message");
			_pRTMPProtocol->EnqueueForDelete();
			return;
		}

		//9. NetStream.Play.Start
		message = StreamMessageFactory::GetInvokeOnStatusStreamPlayStart(
				_pChannelAudio->id, _rtmpStreamId, 0, true, 0, "start...", GetName(),
				_clientId);
		TRACK_MESSAGE("Message:\n%s", STR(message.ToString()));
		if (!_pRTMPProtocol->SendMessage(message)) {
			FATAL("Unable to send message");
			_pRTMPProtocol->EnqueueForDelete();
			return;
		}

		//10. notify |RtmpSampleAccess
		message = StreamMessageFactory::GetNotifyRtmpSampleAccess(
				_pChannelAudio->id, _rtmpStreamId, 0, true, false, false);
		TRACK_MESSAGE("Message:\n%s", STR(message.ToString()));
		if (!_pRTMPProtocol->SendMessage(message)) {
			FATAL("Unable to send message");
			_pRTMPProtocol->EnqueueForDelete();
			return;
		}
	}
	//	else {
	//		FINEST("Skip sending NetStream.Play.Reset, NetStream.Play.Start and notify |RtmpSampleAcces");
	//	}

	//11. notify onStatus code="NetStream.Data.Start"
	if (TAG_KIND_OF(_attachedStreamType, ST_IN_FILE)) {
		message = StreamMessageFactory::GetNotifyOnStatusDataStart(
				_pChannelAudio->id, _rtmpStreamId, 0, true);
		TRACK_MESSAGE("Message:\n%s", STR(message.ToString()));
		if (!_pRTMPProtocol->SendMessage(message)) {
			FATAL("Unable to send message");
			_pRTMPProtocol->EnqueueForDelete();
			return;
		}
	}

	//12. notify onMetaData
	if (!SendOnMetadata()) {
		FATAL("Unable to send onMetadata message");
		_pRTMPProtocol->EnqueueForDelete();
		return;
	}
}

void BaseOutNetRTMPStream::SignalDetachedFromInStream() {
	//1. send the required messages depending on the feeder
	Variant message;
	if (TAG_KIND_OF(_attachedStreamType, ST_IN_FILE)) {
		//2. notify onPlayStatus code="NetStream.Play.Complete", bytes=xxx, duration=yyy, level status
		message = StreamMessageFactory::GetNotifyOnPlayStatusPlayComplete(
				_pChannelAudio->id, _rtmpStreamId, 0, false,
				(double) _metaFileSize, _metaFileDuration);
		TRACK_MESSAGE("Message:\n%s", STR(message.ToString()));
		if (!_pRTMPProtocol->SendMessage(message)) {
			FATAL("Unable to send message");
			_pRTMPProtocol->EnqueueForDelete();
			return;
		}
	} else {
		//3. Send the unpublish notify
		message = StreamMessageFactory::GetInvokeOnStatusStreamPlayUnpublishNotify(
				_pChannelAudio->id, _rtmpStreamId, 0, true, 0, "unpublished...",
				_clientId);
		TRACK_MESSAGE("Message:\n%s", STR(message.ToString()));
		if (!_pRTMPProtocol->SendMessage(message)) {
			FATAL("Unable to send message");
			_pRTMPProtocol->EnqueueForDelete();
			return;
		}
	}

	//4. NetStream.Play.Stop
	message = StreamMessageFactory::GetInvokeOnStatusStreamPlayStop(
			_pChannelAudio->id, _rtmpStreamId, 0, false, 0, "stop...", GetName(),
			_clientId);
	TRACK_MESSAGE("Message:\n%s", STR(message.ToString()));
	if (!_pRTMPProtocol->SendMessage(message)) {
		FATAL("Unable to send message");
		_pRTMPProtocol->EnqueueForDelete();
		return;
	}

	//5. Stream eof
	message = StreamMessageFactory::GetUserControlStreamEof(_rtmpStreamId);
	TRACK_MESSAGE("Message:\n%s", STR(message.ToString()));
	if (!_pRTMPProtocol->SendMessage(message)) {
		FATAL("Unable to send message");
		_pRTMPProtocol->EnqueueForDelete();
		return;
	}

	//6. Reset internally
	InternalReset();
}

bool BaseOutNetRTMPStream::SignalPlay(double &dts, double &length) {
	_paused = false;
	return true;
}

bool BaseOutNetRTMPStream::SignalPause() {
	_paused = true;

	Variant message = StreamMessageFactory::GetInvokeOnStatusStreamPauseNotify(
			_pChannelAudio->id, _rtmpStreamId, 0, false, 0, "Pausing...", GetName(),
			_clientId);
	TRACK_MESSAGE("Message:\n%s", STR(message.ToString()));
	if (!_pRTMPProtocol->SendMessage(message)) {
		FATAL("Unable to send message");
		_pRTMPProtocol->EnqueueForDelete();
		return false;
	}
	return true;
}

bool BaseOutNetRTMPStream::SignalResume() {
	_paused = false;

	Variant message = StreamMessageFactory::GetInvokeOnStatusStreamUnpauseNotify(
			_pChannelAudio->id, _rtmpStreamId, 0, false, 0, "Un-pausing...", GetName(),
			_clientId);
	TRACK_MESSAGE("Message:\n%s", STR(message.ToString()));
	if (!_pRTMPProtocol->SendMessage(message)) {
		FATAL("Unable to send message");
		_pRTMPProtocol->EnqueueForDelete();
		return false;
	}
	return true;
}

bool BaseOutNetRTMPStream::SignalSeek(double &dts) {
	//1. Stream eof
	Variant message = StreamMessageFactory::GetUserControlStreamEof(_rtmpStreamId);
	TRACK_MESSAGE("Message:\n%s", STR(message.ToString()));
	if (!_pRTMPProtocol->SendMessage(message)) {
		FATAL("Unable to send message");
		_pRTMPProtocol->EnqueueForDelete();
		return false;
	}

	//2. Stream is recorded
	message = StreamMessageFactory::GetUserControlStreamIsRecorded(_rtmpStreamId);
	TRACK_MESSAGE("Message:\n%s", STR(message.ToString()));
	if (!_pRTMPProtocol->SendMessage(message)) {
		FATAL("Unable to send message");
		_pRTMPProtocol->EnqueueForDelete();
		return false;
	}

	//3. Stream begin
	message = StreamMessageFactory::GetUserControlStreamBegin(_rtmpStreamId);
	TRACK_MESSAGE("Message:\n%s", STR(message.ToString()));
	if (!_pRTMPProtocol->SendMessage(message)) {
		FATAL("Unable to send message");
		_pRTMPProtocol->EnqueueForDelete();
		return false;
	}

	//4. NetStream.Seek.Notify
	message = StreamMessageFactory::GetInvokeOnStatusStreamSeekNotify(
			_pChannelAudio->id, _rtmpStreamId, dts, true, 0, "seeking...", GetName(),
			_clientId);
	TRACK_MESSAGE("Message:\n%s", STR(message.ToString()));
	if (!_pRTMPProtocol->SendMessage(message)) {
		FATAL("Unable to send message");
		_pRTMPProtocol->EnqueueForDelete();
		return false;
	}

	//5. NetStream.Play.Start
	message = StreamMessageFactory::GetInvokeOnStatusStreamPlayStart(
			_pChannelAudio->id, _rtmpStreamId, 0, false, 0, "start...", GetName(),
			_clientId);
	TRACK_MESSAGE("Message:\n%s", STR(message.ToString()));
	if (!_pRTMPProtocol->SendMessage(message)) {
		FATAL("Unable to send message");
		_pRTMPProtocol->EnqueueForDelete();
		return false;
	}

	//6. notify |RtmpSampleAccess
	message = StreamMessageFactory::GetNotifyRtmpSampleAccess(
			_pChannelAudio->id, _rtmpStreamId, 0, false, false, false);
	TRACK_MESSAGE("Message:\n%s", STR(message.ToString()));
	if (!_pRTMPProtocol->SendMessage(message)) {
		FATAL("Unable to send message");
		_pRTMPProtocol->EnqueueForDelete();
		return false;
	}

	//7. notify onStatus code="NetStream.Data.Start"
	message = StreamMessageFactory::GetNotifyOnStatusDataStart(
			_pChannelAudio->id, _rtmpStreamId, 0, false);
	TRACK_MESSAGE("Message:\n%s", STR(message.ToString()));
	if (!_pRTMPProtocol->SendMessage(message)) {
		FATAL("Unable to send message");
		_pRTMPProtocol->EnqueueForDelete();
		return false;
	}

	//12. notify onMetaData
	if (!SendOnMetadata()) {
		FATAL("Unable to send onMetadata message");
		_pRTMPProtocol->EnqueueForDelete();
		return false;
	}

	InternalReset();

	_seekTime = dts;

	return true;
}

bool BaseOutNetRTMPStream::SignalStop() {
	return true;
}

void BaseOutNetRTMPStream::SignalStreamCompleted() {
	//1. notify onPlayStatus code="NetStream.Play.Complete", bytes=xxx, duration=yyy, level status
	Variant message = StreamMessageFactory::GetNotifyOnPlayStatusPlayComplete(
			_pChannelAudio->id, _rtmpStreamId, 0, false,
			(double) _metaFileSize, _metaFileDuration);
	TRACK_MESSAGE("Message:\n%s", STR(message.ToString()));
	if (!_pRTMPProtocol->SendMessage(message)) {
		FATAL("Unable to send message");
		_pRTMPProtocol->EnqueueForDelete();
		return;
	}

	//3. NetStream.Play.Stop
	message = StreamMessageFactory::GetInvokeOnStatusStreamPlayStop(
			_pChannelAudio->id, _rtmpStreamId, 0, false, 0, "stop...", GetName(),
			_clientId);
	TRACK_MESSAGE("Message:\n%s", STR(message.ToString()));
	if (!_pRTMPProtocol->SendMessage(message)) {
		FATAL("Unable to send message");
		_pRTMPProtocol->EnqueueForDelete();
		return;
	}

	//2. Stream eof
	message = StreamMessageFactory::GetUserControlStreamEof(_rtmpStreamId);
	TRACK_MESSAGE("Message:\n%s", STR(message.ToString()));
	if (!_pRTMPProtocol->SendMessage(message)) {
		FATAL("Unable to send message");
		_pRTMPProtocol->EnqueueForDelete();
		return;
	}

	InternalReset();
}

void BaseOutNetRTMPStream::SignalAudioStreamCapabilitiesChanged(
		StreamCapabilities *pCapabilities, AudioCodecInfo *pOld,
		AudioCodecInfo *pNew) {
	if (pCapabilities == NULL)
		return;
	if (!FeedAudioCodecBytes(pCapabilities, 0, false)) {
		FATAL("Unable to feed audio codec bytes");
		_pRTMPProtocol->EnqueueForDelete();
	}
	if (!SendOnMetadata()) {
		FATAL("Unable to send metadata");
		_pRTMPProtocol->EnqueueForDelete();
	}
}

void BaseOutNetRTMPStream::SignalVideoStreamCapabilitiesChanged(
		StreamCapabilities *pCapabilities, VideoCodecInfo *pOld,
		VideoCodecInfo *pNew) {
	if (pCapabilities == NULL)
		return;
	if (!FeedVideoCodecBytes(pCapabilities, 0, false)) {
		FATAL("Unable to feed video codec bytes");
		_pRTMPProtocol->EnqueueForDelete();
	}
	if (!SendOnMetadata()) {
		FATAL("Unable to send metadata");
		_pRTMPProtocol->EnqueueForDelete();
	}
}

bool BaseOutNetRTMPStream::InternalFeedData(uint8_t *pData, uint32_t dataLength,
		uint32_t processedLength, uint32_t totalLength,
		double dts, bool isAudio) {
	if (_start < 0)
		_start = dts;
	dts -= _start;
	if (_paused)
		return true;
	if (isAudio) {
		if (processedLength == 0)
			_stats.audio.packetsCount++;
		_stats.audio.bytesCount += dataLength;
		if (_isFirstAudioFrame) {
			_audioCurrentFrameDropped = false;
			if (dataLength == 0)
				return true;

			//first frame
			if (processedLength != 0) {
				//middle of packet
				_pRTMPProtocol->EnqueueForOutbound();
				return true;
			}

			StreamCapabilities *pCapabilities = GetCapabilities();
			if (pCapabilities == NULL) {
				return true;
			}

			if (!FeedAudioCodecBytes(pCapabilities, dts + _seekTime, true)) {
				FATAL("Unable to feed audio codec bytes");
				return false;
			}

			_isFirstAudioFrame = false;

			H_IA(_audioHeader) = true;
			H_TS(_audioHeader) = (uint32_t) (dts + _seekTime);
		} else {
			ALLOW_EXECUTION(processedLength, dataLength, isAudio);
			H_IA(_audioHeader) = _absoluteTimestamps;
			if (processedLength == 0)
				H_TS(_audioHeader) = (uint32_t) ((dts + _seekTime)
					- _pChannelAudio->lastOutAbsTs);
		}

		H_ML(_audioHeader) = totalLength;

		TRACK_PAYLOAD(processedLength, dataLength, pData);
		return ChunkAndSend(pData, dataLength, _audioBucket,
				_audioHeader, *_pChannelAudio);
	} else {
		if (processedLength == 0)
			_stats.video.packetsCount++;
		_stats.video.bytesCount += dataLength;
		if (_isFirstVideoFrame) {
			_videoCurrentFrameDropped = false;
			if (dataLength == 0)
				return true;

			//first frame
			if (processedLength != 0) {
				//middle of packet
				_pRTMPProtocol->EnqueueForOutbound();
				return true;
			}

			if (dataLength == 0) {
				_pRTMPProtocol->EnqueueForOutbound();
				return true;
			}
#ifndef WIN32
			if ((pData[0] >> 4) != 1) {
				_pRTMPProtocol->EnqueueForOutbound();
				return true;
			}
#endif /* WIN32 */

			StreamCapabilities *pCapabilities = GetCapabilities();
			if (pCapabilities == NULL) {
				return true;
			}

			if (!FeedVideoCodecBytes(pCapabilities, dts + _seekTime, true)) {
				FATAL("Unable to feed video codec bytes");
				return false;
			}

			_isFirstVideoFrame = false;

			H_IA(_videoHeader) = true;
			H_TS(_videoHeader) = (uint32_t) (dts + _seekTime);
		} else {
			ALLOW_EXECUTION(processedLength, dataLength, isAudio);
			H_IA(_videoHeader) = _absoluteTimestamps;
			if (processedLength == 0)
				H_TS(_videoHeader) = (uint32_t) ((dts + _seekTime)
					- _pChannelVideo->lastOutAbsTs);
		}

		H_ML(_videoHeader) = totalLength;

		TRACK_PAYLOAD(processedLength, dataLength, pData);
		return ChunkAndSend(pData, dataLength, _videoBucket,
				_videoHeader, *_pChannelVideo);
	}
}

bool BaseOutNetRTMPStream::FeedAudioCodecBytes(StreamCapabilities *pCapabilities,
		double dts, bool isAbsolute) {
	if (dts < 0)
		dts = 0;
	if ((pCapabilities == NULL) || (pCapabilities->GetAudioCodecType() != CODEC_AUDIO_AAC))
		return true;
	AudioCodecInfoAAC *pInfo = pCapabilities->GetAudioCodec<AudioCodecInfoAAC > ();
	if (pInfo == NULL)
		return true;
	IOBuffer &buffer = pInfo->GetRTMPRepresentation();
	H_IA(_audioHeader) = isAbsolute;
	H_TS(_audioHeader) = (uint32_t) dts;
	H_ML(_audioHeader) = GETAVAILABLEBYTESCOUNT(buffer);
	return ChunkAndSend(GETIBPOINTER(buffer),
			GETAVAILABLEBYTESCOUNT(buffer),
			_audioBucket,
			_audioHeader,
			*_pChannelAudio);
}

bool BaseOutNetRTMPStream::FeedVideoCodecBytes(StreamCapabilities *pCapabilities,
		double dts, bool isAbsolute) {
	if (dts < 0)
		dts = 0;
	if ((pCapabilities == NULL) || (pCapabilities->GetVideoCodecType() != CODEC_VIDEO_H264))
		return true;
	VideoCodecInfoH264 *pInfo = pCapabilities->GetVideoCodec<VideoCodecInfoH264 > ();
	if (pInfo == NULL)
		return true;
	IOBuffer &buffer = pInfo->GetRTMPRepresentation();
	H_IA(_videoHeader) = isAbsolute;
	H_TS(_videoHeader) = (uint32_t) dts;
	H_ML(_videoHeader) = GETAVAILABLEBYTESCOUNT(buffer);
	return ChunkAndSend(GETIBPOINTER(buffer),
			GETAVAILABLEBYTESCOUNT(buffer),
			_videoBucket,
			_videoHeader,
			*_pChannelVideo);
}

bool BaseOutNetRTMPStream::ChunkAndSend(uint8_t *pData, uint32_t length,
		IOBuffer &bucket, Header &header, Channel &channel) {
	if (H_ML(header) == 0) {
		TRACK_HEADER(header, channel.lastOutProcBytes);
		return _pRTMPProtocol->SendRawData(header, channel, NULL, 0);
	}

	if ((_feederChunkSize == _chunkSize) &&
			(GETAVAILABLEBYTESCOUNT(bucket) == 0)) {
		TRACK_HEADER(header, channel.lastOutProcBytes);
		if (!_pRTMPProtocol->SendRawData(header, channel, pData, length)) {
			FATAL("Unable to feed data");
			return false;
		}
		channel.lastOutProcBytes += length;
		channel.lastOutProcBytes %= H_ML(header);
		return true;
	}

	uint32_t availableDataInBuffer = GETAVAILABLEBYTESCOUNT(bucket);
	uint32_t totalAvailableBytes = availableDataInBuffer + length;
	uint32_t leftBytesToSend = H_ML(header) - channel.lastOutProcBytes;

	if (totalAvailableBytes < _chunkSize &&
			totalAvailableBytes != leftBytesToSend) {
		bucket.ReadFromBuffer(pData, length);
		return true;
	}

	if (availableDataInBuffer != 0) {
		//Send data
		TRACK_HEADER(header, channel.lastOutProcBytes);
		if (!_pRTMPProtocol->SendRawData(header, channel,
				GETIBPOINTER(bucket), availableDataInBuffer)) {
			FATAL("Unable to send data");
			return false;
		}
		//cleanup buffer
		bucket.IgnoreAll();

		//update counters
		totalAvailableBytes -= availableDataInBuffer;
		leftBytesToSend -= availableDataInBuffer;
		channel.lastOutProcBytes += availableDataInBuffer;
		uint32_t leftOvers = _chunkSize - availableDataInBuffer;
		availableDataInBuffer = 0;

		//bite from the pData
		leftOvers = leftOvers <= length ? leftOvers : length;
		if (!_pRTMPProtocol->SendRawData(pData, leftOvers)) {
			FATAL("Unable to send data");
			return false;
		}

		//update the counters
		pData += leftOvers;
		length -= leftOvers;
		totalAvailableBytes -= leftOvers;
		leftBytesToSend -= leftOvers;
		channel.lastOutProcBytes += leftOvers;
	}

	while (totalAvailableBytes >= _chunkSize) {
		TRACK_HEADER(header, channel.lastOutProcBytes);
		if (!_pRTMPProtocol->SendRawData(header, channel, pData, _chunkSize)) {
			FATAL("Unable to send data");
			return false;
		}
		totalAvailableBytes -= _chunkSize;
		leftBytesToSend -= _chunkSize;
		channel.lastOutProcBytes += _chunkSize;
		length -= _chunkSize;
		pData += _chunkSize;
	}

	if (totalAvailableBytes > 0 && totalAvailableBytes == leftBytesToSend) {
		TRACK_HEADER(header, channel.lastOutProcBytes);
		if (!_pRTMPProtocol->SendRawData(header, channel, pData, leftBytesToSend)) {
			FATAL("Unable to send data");
			return false;
		}
		totalAvailableBytes -= leftBytesToSend;
		channel.lastOutProcBytes += leftBytesToSend;
		length -= leftBytesToSend;
		pData += leftBytesToSend;
		leftBytesToSend -= leftBytesToSend;
	}

	if (length > 0) {
		bucket.ReadFromBuffer(pData, length);
	}

	if (leftBytesToSend == 0) {
		o_assert(channel.lastOutProcBytes == H_ML(header));
		channel.lastOutProcBytes = 0;
	}

	return true;
}

bool BaseOutNetRTMPStream::AllowExecution(uint32_t totalProcessed, uint32_t dataLength, bool isAudio) {
	if (!_canDropFrames) {
		// we are not allowed to drop frames
		return true;
	}

	//we are allowed to drop frames
	uint64_t &bytesCounter = isAudio ? _stats.audio.droppedBytesCount : _stats.video.droppedBytesCount;
	uint64_t &packetsCounter = isAudio ? _stats.audio.droppedPacketsCount : _stats.video.droppedPacketsCount;
	bool &currentFrameDropped = isAudio ? _audioCurrentFrameDropped : _videoCurrentFrameDropped;

	if (currentFrameDropped) {
		//current frame was dropped. Test to see if we are in the middle
		//of it or this is a new one
		if (totalProcessed != 0) {
			//we are in the middle of it. Don't allow execution
			bytesCounter += dataLength;
			return false;
		} else {
			//this is a new frame. We will detect later if it can be sent
			currentFrameDropped = false;
		}
	}

	if (totalProcessed != 0) {
		//we are in the middle of a non-dropped frame. Send it anyway
		return true;
	}

#ifdef SIMULATE_DROPPING_FRAMES
	if ((rand() % 100) < SIMULATE_DROPPING_FRAMES) {
		//we have too many data left unsent. Drop the frame
		packetsCounter++;
		bytesCounter += dataLength;
		currentFrameDropped = true;
		return false;
	} else {
		//we can still pump data
		return true;
	}
#else /* SIMULATE_DROPPING_FRAMES */
	//do we have any data?
	if (_pRTMPProtocol->GetOutputBuffer() == NULL) {
		//no data in the output buffer. Allow to send it
		return true;
	}

	//we have some data in the output buffer
	uint32_t outBufferSize = GETAVAILABLEBYTESCOUNT(*_pRTMPProtocol->GetOutputBuffer());
	if (outBufferSize > _maxBufferSize) {
		//we have too many data left unsent. Drop the frame
		packetsCounter++;
		bytesCounter += dataLength;
		currentFrameDropped = true;
		_pRTMPProtocol->SignalOutBufferFull(outBufferSize, _maxBufferSize);
		return false;
	} else {
		//we can still pump data
		return true;
	}
#endif /* SIMULATE_DROPPING_FRAMES */
}

void BaseOutNetRTMPStream::InternalReset() {
	if ((_pChannelAudio == NULL)
			|| (_pChannelVideo == NULL)
			|| (_pChannelCommands == NULL))
		return;
	_start = -1;
	_seekTime = 0;

	_videoCurrentFrameDropped = false;
	_isFirstVideoFrame = true;
	H_CI(_videoHeader) = _pChannelVideo->id;
	H_MT(_videoHeader) = RM_HEADER_MESSAGETYPE_VIDEODATA;
	H_SI(_videoHeader) = _rtmpStreamId;
	_videoHeader.readCompleted = 0;
	_videoBucket.IgnoreAll();

	_audioCurrentFrameDropped = false;
	_isFirstAudioFrame = true;
	H_CI(_audioHeader) = _pChannelAudio->id;
	H_MT(_audioHeader) = RM_HEADER_MESSAGETYPE_AUDIODATA;
	H_SI(_audioHeader) = _rtmpStreamId;
	_audioHeader.readCompleted = 0;
	_audioBucket.IgnoreAll();

	_attachedStreamType = 0;
	GetMetadata();
}

void BaseOutNetRTMPStream::GetMetadata() {
	_metaFileSize = 0;
	_metaFileDuration = 0;

	_metadata = Variant();
	if ((_pInStream != NULL)
			&& (TAG_KIND_OF(_pInStream->GetType(), ST_IN_FILE))) {
		InFileRTMPStream *pInFileRTMPStream = (InFileRTMPStream *) _pInStream;
		_metadata = pInFileRTMPStream->GetCompleteMetadata().publicMetadata();
		_metaFileSize = _metadata.fileSize();
		_metaFileDuration = _metadata.duration();
	}

	_metadata[HTTP_HEADERS_SERVER] = BRANDING_BANNER;
	StreamCapabilities *pCapabilities = NULL;
	if (_pInStream != NULL)
		pCapabilities = _pInStream->GetCapabilities();
	if (pCapabilities == NULL)
		return;
	pCapabilities->GetRTMPMetadata(_metadata);
}

bool BaseOutNetRTMPStream::SendOnMetadata() {
	GetMetadata();

	Variant message = StreamMessageFactory::GetNotifyOnMetaData(_pChannelAudio->id,
			_rtmpStreamId, 0, false, _metadata,
			!_sendOnStatusPlayMessages);
	TRACK_MESSAGE("Message:\n%s", STR(message.ToString()));
	if (!_pRTMPProtocol->SendMessage(message)) {
		FATAL("Unable to send message");
		_pRTMPProtocol->EnqueueForDelete();
		return false;
	}

	return true;
}
#endif /* HAS_PROTOCOL_RTMP */

