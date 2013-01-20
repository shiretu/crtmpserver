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
#include "protocols/rtmp/streaming/outnetrtmp4tsstream.h"
#include "streaming/streamstypes.h"
#include "streaming/nalutypes.h"
#include "protocols/http/basehttpprotocol.h"
#include "streaming/codectypes.h"

#define SPSPPS_MAX_LENGTH 1024

OutNetRTMP4TSStream::OutNetRTMP4TSStream(BaseProtocol *pProtocol, string name,
		uint32_t rtmpStreamId, uint32_t chunkSize)
: BaseOutNetRTMPStream(pProtocol, ST_OUT_NET_RTMP_4_TS, name, rtmpStreamId, chunkSize) {
}

OutNetRTMP4TSStream::~OutNetRTMP4TSStream() {

}

bool OutNetRTMP4TSStream::IsCompatibleWithType(uint64_t type) {
	return TAG_KIND_OF(type, ST_IN_NET_TS)
			|| TAG_KIND_OF(type, ST_IN_NET_RTP);
}

bool OutNetRTMP4TSStream::FeedData(uint8_t *pData, uint32_t dataLength,
		uint32_t processedLength, uint32_t totalLength,
		double pts, double dts, bool isAudio) {
	return GenericProcessData(pData, dataLength, processedLength, totalLength,
			pts, dts, isAudio);
}

bool OutNetRTMP4TSStream::FinishInitialization(
		GenericProcessDataSetup *pGenericProcessDataSetup) {
	if (!BaseOutStream::FinishInitialization(pGenericProcessDataSetup)) {
		FATAL("Unable to finish output stream initialization");
		return false;
	}

	//video setup
	pGenericProcessDataSetup->video.avc.Init(
			NALU_MARKER_TYPE_SIZE, //naluMarkerType,
			false, //insertPDNALU,
			true, //insertRTMPPayloadHeader,
			false, //insertSPSPPSBeforeIDR,
			true //aggregateNALU
			);

	//audio setup
	pGenericProcessDataSetup->audio.aac._insertADTSHeader = false;
	pGenericProcessDataSetup->audio.aac._insertRTMPPayloadHeader = true;

	//misc setup
	pGenericProcessDataSetup->_timeBase = 0;
	pGenericProcessDataSetup->_maxFrameSize = 8 * 1024 * 1024;

	return true;
}

bool OutNetRTMP4TSStream::PushVideoData(IOBuffer &buffer, double pts, double dts,
		bool isKeyFrame) {
	return BaseOutNetRTMPStream::InternalFeedData(
			GETIBPOINTER(buffer), //pData
			GETAVAILABLEBYTESCOUNT(buffer), //dataLength
			0, //processedLength
			GETAVAILABLEBYTESCOUNT(buffer), //totalLength
			dts, //dts
			false //isAudio
			);
}

bool OutNetRTMP4TSStream::PushAudioData(IOBuffer &buffer, double pts, double dts) {
	return BaseOutNetRTMPStream::InternalFeedData(
			GETIBPOINTER(buffer), //pData
			GETAVAILABLEBYTESCOUNT(buffer), //dataLength
			0, //processedLength
			GETAVAILABLEBYTESCOUNT(buffer), //totalLength
			dts, //dts
			true //isAudio
			);
}

bool OutNetRTMP4TSStream::IsCodecSupported(uint64_t codec) {
	return (codec == CODEC_VIDEO_H264)
			|| (codec == CODEC_AUDIO_AAC)
			;
}

#endif /* HAS_PROTOCOL_RTMP */
