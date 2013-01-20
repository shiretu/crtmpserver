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
#include "protocols/ts/innettsstream.h"
#include "streaming/streamstypes.h"
#include "protocols/ts/inboundtsprotocol.h"
#include "streaming/baseoutstream.h"
#include "streaming/nalutypes.h"
#include "streaming/codectypes.h"

InNetTSStream::InNetTSStream(BaseProtocol *pProtocol, string name,
		uint32_t bandwidthHint)
: BaseInNetStream(pProtocol, ST_IN_NET_TS, name) {
	//audio section
	_hasAudio = false;
	_audioPacketsCount = 0;
	_audioDroppedPacketsCount = 0;
	_audioBytesCount = 0;
	_audioDroppedBytesCount = 0;

	//video section
	_hasVideo = false;
	_videoPacketsCount = 0;
	_videoDroppedPacketsCount = 0;
	_videoBytesCount = 0;
	_videoDroppedBytesCount = 0;

	_streamCapabilities.SetTransferRate(bandwidthHint);
	_enabled = false;
}

InNetTSStream::~InNetTSStream() {
}

StreamCapabilities * InNetTSStream::GetCapabilities() {
	return &_streamCapabilities;
}

bool InNetTSStream::HasAudio() {
	return _hasAudio;
}

void InNetTSStream::HasAudio(bool value) {
	_hasAudio = value;
}

bool InNetTSStream::HasVideo() {
	return _hasVideo;
}

void InNetTSStream::HasVideo(bool value) {
	_hasVideo = value;
}

void InNetTSStream::Enable(bool value) {
	_enabled = value;
}

bool InNetTSStream::FeedData(uint8_t *pData, uint32_t dataLength,
		uint32_t processedLength, uint32_t totalLength,
		double pts, double dts, bool isAudio) {
	if (((_hasAudio)&&(_streamCapabilities.GetAudioCodecType() != CODEC_AUDIO_AAC))
			|| ((_hasVideo)&&(_streamCapabilities.GetVideoCodecType() != CODEC_VIDEO_H264))
			|| (!_enabled)) {
		if (isAudio) {
			_audioDroppedBytesCount += dataLength;
			_audioDroppedPacketsCount++;
		} else {
			_videoDroppedBytesCount += dataLength;
			_videoDroppedPacketsCount++;
		}
		return true;
	}
	if (isAudio) {
		_audioBytesCount += dataLength;
		_audioPacketsCount++;
	} else {
		_videoBytesCount += dataLength;
		_videoPacketsCount++;
	}
	LinkedListNode<BaseOutStream *> *pTemp = _pOutStreams;
	while (pTemp != NULL) {
		if (!pTemp->info->IsEnqueueForDelete()) {
			if (!pTemp->info->FeedData(pData, dataLength, processedLength, totalLength,
					pts, dts, isAudio)) {
				FINEST("Unable to feed OS: %p", pTemp->info);
				pTemp->info->EnqueueForDelete();
				if (GetProtocol() == pTemp->info->GetProtocol()) {
					return false;
				}
			}
		}
		pTemp = pTemp->pPrev;
	}
	return true;
}

void InNetTSStream::ReadyForSend() {
	NYI;
}

bool InNetTSStream::IsCompatibleWithType(uint64_t type) {
	return TAG_KIND_OF(type, ST_OUT_NET_RTMP_4_TS)
			|| (type == ST_OUT_NET_RTP);
}

void InNetTSStream::SignalOutStreamAttached(BaseOutStream *pOutStream) {
	//NYI;
}

void InNetTSStream::SignalOutStreamDetached(BaseOutStream *pOutStream) {
	//NYI;
}

bool InNetTSStream::SignalPlay(double &dts, double &length) {
	return true;
}

bool InNetTSStream::SignalPause() {
	return true;
}

bool InNetTSStream::SignalResume() {
	return true;
}

bool InNetTSStream::SignalSeek(double &dts) {
	return true;
}

bool InNetTSStream::SignalStop() {
	return true;
}

void InNetTSStream::GetStats(Variant &info, uint32_t namespaceId) {
	BaseInNetStream::GetStats(info, namespaceId);
	info["audio"]["packetsCount"] = _audioPacketsCount;
	info["audio"]["droppedPacketsCount"] = _audioDroppedPacketsCount;
	info["audio"]["bytesCount"] = _audioBytesCount;
	info["audio"]["droppedBytesCount"] = _audioDroppedBytesCount;
	info["video"]["packetsCount"] = _videoPacketsCount;
	info["video"]["droppedPacketsCount"] = _videoDroppedPacketsCount;
	info["video"]["bytesCount"] = _videoBytesCount;
	info["video"]["droppedBytesCount"] = _videoDroppedBytesCount;
}

#endif	/* defined HAS_PROTOCOL_TS && defined HAS_MEDIA_TS */

