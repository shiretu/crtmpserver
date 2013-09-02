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
}

BaseOutRecording::~BaseOutRecording() {
}

void BaseOutRecording::GetStats(Variant &info, uint32_t namespaceId) {
	BaseOutStream::GetStats(info, namespaceId);
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
	uint64_t &bytesCount = isAudio ? _stats.audio.bytesCount : _stats.video.bytesCount;
	uint64_t &packetsCount = isAudio ? _stats.audio.packetsCount : _stats.video.packetsCount;
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
	WARN("Codecs changed and the recordings does not support it. Closing recording");
	if (pOld != NULL)
		FINEST("pOld: %s", STR(*pOld));
	if (pNew != NULL)
		FINEST("pNew: %s", STR(*pNew));
	else
		FINEST("pNew: NULL");
	EnqueueForDelete();
}

void BaseOutRecording::SignalVideoStreamCapabilitiesChanged(
		StreamCapabilities *pCapabilities, VideoCodecInfo *pOld,
		VideoCodecInfo *pNew) {
	if ((pOld == NULL)&&(pNew != NULL))
		return;
	WARN("Codecs changed and the recordings does not support it. Closing recording");
	if (pOld != NULL)
		FINEST("pOld: %s", STR(*pOld));
	if (pNew != NULL)
		FINEST("pNew: %s", STR(*pNew));
	else
		FINEST("pNew: NULL");
	EnqueueForDelete();
}
