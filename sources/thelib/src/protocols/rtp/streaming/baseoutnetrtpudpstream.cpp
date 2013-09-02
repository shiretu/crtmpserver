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


#ifdef HAS_PROTOCOL_RTP
#include "protocols/rtp/streaming/baseoutnetrtpudpstream.h"
#include "streaming/streamstypes.h"
#include "protocols/protocolmanager.h"
#include "protocols/baseprotocol.h"
#include "protocols/rtp/connectivity/outboundconnectivity.h"

BaseOutNetRTPUDPStream::BaseOutNetRTPUDPStream(BaseProtocol *pProtocol, string name)
: BaseOutNetStream(pProtocol, ST_OUT_NET_RTP, name) {
	_audioSsrc = 0x80000000 | (rand()&0x00ffffff);
	_videoSsrc = _audioSsrc + 1;
	_pConnectivity = NULL;
	_videoCounter = (uint16_t) rand();
	_audioCounter = (uint16_t) rand();
	_hasAudio = false;
	_hasVideo = false;
	_enabled = false;
}

BaseOutNetRTPUDPStream::~BaseOutNetRTPUDPStream() {
}

void BaseOutNetRTPUDPStream::Enable(bool value) {
	_enabled = value;
}

OutboundConnectivity *BaseOutNetRTPUDPStream::GetConnectivity() {
	return _pConnectivity;
}

void BaseOutNetRTPUDPStream::SetConnectivity(OutboundConnectivity *pConnectivity) {
	_pConnectivity = pConnectivity;
}

void BaseOutNetRTPUDPStream::HasAudioVideo(bool hasAudio, bool hasVideo) {
	_hasAudio = hasAudio;
	_hasVideo = hasVideo;
}

uint32_t BaseOutNetRTPUDPStream::AudioSSRC() {
	return _audioSsrc;
}

uint32_t BaseOutNetRTPUDPStream::VideoSSRC() {
	return _videoSsrc;
}

uint16_t BaseOutNetRTPUDPStream::VideoCounter() {
	return _videoCounter;
}

uint16_t BaseOutNetRTPUDPStream::AudioCounter() {
	return _audioCounter;
}

bool BaseOutNetRTPUDPStream::SignalPlay(double &dts, double &length) {
	return true;
}

bool BaseOutNetRTPUDPStream::SignalPause() {
	return true;
}

bool BaseOutNetRTPUDPStream::SignalResume() {
	return true;
}

bool BaseOutNetRTPUDPStream::SignalSeek(double &dts) {
	return true;
}

bool BaseOutNetRTPUDPStream::SignalStop() {
	return true;
}

bool BaseOutNetRTPUDPStream::IsCompatibleWithType(uint64_t type) {
	return type == ST_IN_NET_RTMP
			|| type == ST_IN_NET_TS
			|| type == ST_IN_NET_RTP
			|| type == ST_IN_NET_LIVEFLV
			|| type == ST_IN_FILE_RTMP
			;
}

void BaseOutNetRTPUDPStream::SignalDetachedFromInStream() {
	_pConnectivity->SignalDetachedFromInStream();
}

void BaseOutNetRTPUDPStream::SignalStreamCompleted() {
	EnqueueForDelete();
}

bool BaseOutNetRTPUDPStream::FeedData(uint8_t *pData, uint32_t dataLength,
		uint32_t processedLength, uint32_t totalLength,
		double pts, double dts, bool isAudio) {
	if (!_enabled)
		return true;
	return GenericProcessData(pData, dataLength, processedLength, totalLength,
			pts, dts, isAudio);
}

bool BaseOutNetRTPUDPStream::FinishInitialization(
		GenericProcessDataSetup *pGenericProcessDataSetup) {
	if (!BaseOutStream::FinishInitialization(pGenericProcessDataSetup)) {
		FATAL("Unable to finish output stream initialization");
		return false;
	}

	//video setup
	pGenericProcessDataSetup->video.avc.Init(
			NALU_MARKER_TYPE_NONE, //naluMarkerType,
			false, //insertPDNALU,
			false, //insertRTMPPayloadHeader,
			false, //insertSPSPPSBeforeIDR,
			false //aggregateNALU
			);

	//audio setup
	pGenericProcessDataSetup->audio.aac._insertADTSHeader = false;
	pGenericProcessDataSetup->audio.aac._insertRTMPPayloadHeader = false;

	//misc setup
	pGenericProcessDataSetup->_timeBase = -1;
	pGenericProcessDataSetup->_maxFrameSize = 8 * 1024 * 1024;

	pGenericProcessDataSetup->_hasAudio = _hasAudio;
	pGenericProcessDataSetup->_hasVideo = _hasVideo;

	return true;
}

#endif /* HAS_PROTOCOL_RTP */
