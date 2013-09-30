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

	//video section
	_hasVideo = false;

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
			_stats.audio.droppedBytesCount += dataLength;
			_stats.audio.droppedPacketsCount++;
		} else {
			_stats.video.droppedBytesCount += dataLength;
			_stats.video.droppedPacketsCount++;
		}
		return true;
	}
	if (isAudio) {
		_stats.audio.packetsCount++;
		_stats.audio.bytesCount += dataLength;
	} else {
		_stats.video.packetsCount++;
		_stats.video.bytesCount += dataLength;
	}
	LinkedListNode<BaseOutStream *> *pIterator = _pOutStreams;
	LinkedListNode<BaseOutStream *> *pCurrent = NULL;
	while (pIterator != NULL) {
		pCurrent = pIterator;
		pIterator = pIterator->pPrev;
		if (pCurrent->info->IsEnqueueForDelete())
			continue;
		if (!pCurrent->info->FeedData(pData, dataLength, processedLength, totalLength,
				pts, dts, isAudio)) {
			if ((pIterator != NULL)&&(pIterator->pNext == pCurrent)) {
				pCurrent->info->EnqueueForDelete();
				if (GetProtocol() == pCurrent->info->GetProtocol()) {
					return false;
				}
			}
		}
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

#endif	/* defined HAS_PROTOCOL_TS && defined HAS_MEDIA_TS */

