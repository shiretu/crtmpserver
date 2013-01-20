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

#include "recording/baseoutrecording.h"
#include "application/baseclientapplication.h"
#include "streaming/streamstypes.h"
#include "streaming/nalutypes.h"
#include "streaming/baseinstream.h"
#include "protocols/baseprotocol.h"

BaseOutRecording::BaseOutRecording(BaseProtocol *pProtocol, uint64_t type,
		string name, Variant &settings)
: BaseOutFileStream(pProtocol, type, name) {
	_settings = settings;

	//audio
	_audioBytesCount = 0;
	_audioPacketsCount = 0;

	//video
	_videoBytesCount = 0;
	_videoPacketsCount = 0;
}

BaseOutRecording::~BaseOutRecording() {
}

void BaseOutRecording::GetStats(Variant &info, uint32_t namespaceId) {
	BaseOutStream::GetStats(info, namespaceId);
	info["audio"]["bytesCount"] = _audioBytesCount;
	info["audio"]["packetsCount"] = _audioPacketsCount;
	info["audio"]["droppedPacketsCount"] = (uint64_t) 0;
	info["video"]["bytesCount"] = _videoBytesCount;
	info["video"]["packetsCount"] = _videoPacketsCount;
	info["video"]["droppedPacketsCount"] = (uint64_t) 0;
	info["record"] = _settings;
}

bool BaseOutRecording::IsCompatibleWithType(uint64_t type) {
	return TAG_KIND_OF(type, ST_IN_NET_RTMP)
			|| TAG_KIND_OF(type, ST_IN_NET_RTP)
			|| TAG_KIND_OF(type, ST_IN_NET_TS)
			|| TAG_KIND_OF(type, ST_IN_NET_LIVEFLV)
			|| TAG_KIND_OF(type, ST_IN_FILE);
}

bool BaseOutRecording::SignalPlay(double &dts, double &length) {
	return true;
}

bool BaseOutRecording::SignalPause() {
	return true;
}

bool BaseOutRecording::SignalResume() {
	return true;
}

bool BaseOutRecording::SignalSeek(double &dts) {
	return true;
}

bool BaseOutRecording::SignalStop() {
	return true;
}

void BaseOutRecording::SignalAttachedToInStream() {
}

void BaseOutRecording::SignalDetachedFromInStream() {
	_pProtocol->EnqueueForDelete();
}

void BaseOutRecording::SignalStreamCompleted() {
	_pProtocol->EnqueueForDelete();
}

bool BaseOutRecording::FeedData(uint8_t *pData, uint32_t dataLength,
		uint32_t processedLength, uint32_t totalLength,
		double pts, double dts, bool isAudio) {
	uint64_t &bytesCount = isAudio ? _audioBytesCount : _videoBytesCount;
	uint64_t &packetsCount = isAudio ? _audioPacketsCount : _videoPacketsCount;
	packetsCount++;
	bytesCount += dataLength;
	return GenericProcessData(pData, dataLength, processedLength, totalLength,
			pts, dts, isAudio);
}

void BaseOutRecording::SignalAudioStreamCapabilitiesChanged(
		StreamCapabilities *pCapabilities, AudioCodecInfo *pOld,
		AudioCodecInfo *pNew) {
	if ((pOld == NULL)&&(pNew != NULL))
		return;
	EnqueueForDelete();
}

void BaseOutRecording::SignalVideoStreamCapabilitiesChanged(
		StreamCapabilities *pCapabilities, VideoCodecInfo *pOld,
		VideoCodecInfo *pNew) {
	if ((pOld == NULL)&&(pNew != NULL))
		return;
	EnqueueForDelete();
}
