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
#include "protocols/rtp/streaming/innetrtpstream.h"
#include "streaming/streamstypes.h"
#include "streaming/nalutypes.h"
#include "streaming/baseoutstream.h"
#include "protocols/baseprotocol.h"
#include "protocols/rtmp/basertmpprotocol.h"
#include "protocols/rtmp/streaming/baseoutnetrtmpstream.h"
#include "protocols/rtp/sdp.h"

InNetRTPStream::InNetRTPStream(BaseProtocol *pProtocol, string name, Variant &videoTrack,
		Variant &audioTrack, uint32_t bandwidthHint, uint8_t rtcpDetectionInterval)
: BaseInNetStream(pProtocol, ST_IN_NET_RTP, name) {
	_hasAudio = false;
	_isLatm = false;
	_audioSampleRate = 1;
	if (audioTrack != V_NULL) {
		string raw = unhex(SDP_AUDIO_CODEC_SETUP(audioTrack));
		//IC TODO
		//		DEBUG("Audio codec: %s", STR(SDP_AUDIO_CODEC_SETUP(audioTrack)));
		//		DEBUG("audioTrack: length %"PRIz"u", raw.length());
		//		for (uint8_t i = 0; i < raw.length(); i++) {
		//			DEBUG("audioTrack: %"PRIx8" : %"PRIx8, i, ((uint8_t *) raw.data())[i]);
		//		}
		_isLatm = (SDP_AUDIO_TRANSPORT(audioTrack) == "mp4a-latm");
		AudioCodecInfo *pInfo = _capabilities.AddTrackAudioAAC(
				(uint8_t *) raw.data(),
				(uint8_t) raw.length(),
				!_isLatm,
				this);
		_hasAudio = (bool)(pInfo != NULL);
		if (pInfo != NULL) {
			_audioSampleRate = pInfo->_samplingRate;
		}
	}

	_hasVideo = false;
	_videoSampleRate = 1;
	if (videoTrack != V_NULL) {
		string rawSps = unb64(SDP_VIDEO_CODEC_H264_SPS(videoTrack));
		string rawPps = unb64(SDP_VIDEO_CODEC_H264_PPS(videoTrack));
		//		//IC TODO
		//		DEBUG("SPS: %s", STR(SDP_VIDEO_CODEC_H264_SPS(videoTrack)));
		//		DEBUG("PPS: %s", STR(SDP_VIDEO_CODEC_H264_PPS(videoTrack)));
		//		DEBUG("videoTrack SPS: length %"PRIz"u", rawSps.length());
		//		for (uint8_t i = 0; i < rawSps.length(); i++) {
		//			DEBUG("SPS: %"PRIx8" : %"PRIx8, i, ((uint8_t *) rawSps.data())[i]);
		//		}
		//		DEBUG("videoTrack PPS: length %"PRIz"u", rawPps.length());
		//		for (uint8_t i = 0; i < rawPps.length(); i++) {
		//			DEBUG("PPS: %"PRIx8" : %"PRIx8, i, ((uint8_t *) rawPps.data())[i]);
		//		}
		VideoCodecInfo *pInfo = _capabilities.AddTrackVideoH264(
				(uint8_t *) rawSps.data(),
				(uint32_t) rawSps.length(),
				(uint8_t *) rawPps.data(),
				(uint32_t) rawPps.length(),
				(uint32_t) SDP_TRACK_CLOCKRATE(videoTrack),
				this
				);
		_hasVideo = (bool)(pInfo != NULL);
		if (_hasVideo) {
			_videoSampleRate = pInfo->_samplingRate;
		}
	}
	if (bandwidthHint > 0)
		_capabilities.SetTransferRate(bandwidthHint);

	_audioSequence = 0;
	_audioPacketsCount = 0;
	_audioDroppedPacketsCount = 0;
	_audioBytesCount = 0;
	_audioNTP = 0;
	_audioRTP = 0;
	_audioLastDts = -1;
	_audioRTPRollCount = 0;
	_audioLastRTP = 0;
	_audioFirstTimestamp = -1;
	_lastAudioRTCPRTP = 0;
	_audioRTCPRTPRollCount = 0;

	_videoSequence = 0;
	_videoPacketsCount = 0;
	_videoDroppedPacketsCount = 0;
	_videoBytesCount = 0;
	_videoNTP = 0;
	_videoRTP = 0;
	_videoLastPts = -1;
	_videoLastDts = -1;
	_videoRTPRollCount = 0;
	_videoLastRTP = 0;
	_videoFirstTimestamp = -1;
	_lastVideoRTCPRTP = 0;
	_videoRTCPRTPRollCount = 0;

	_rtcpPresence = RTCP_PRESENCE_UNKNOWN;
	_rtcpDetectionInterval = rtcpDetectionInterval;
	_rtcpDetectionStart = 0;

	_dtsCacheSize = 1;
	//_maxDts = -1;
}

InNetRTPStream::~InNetRTPStream() {
}

StreamCapabilities * InNetRTPStream::GetCapabilities() {
	return &_capabilities;
}

bool InNetRTPStream::IsCompatibleWithType(uint64_t type) {
	return (type == ST_OUT_NET_RTMP_4_TS)
			|| (type == ST_OUT_NET_RTP)
			|| (type == ST_OUT_FILE_RTMP_FLV);
}

void InNetRTPStream::ReadyForSend() {

}

void InNetRTPStream::SignalOutStreamAttached(BaseOutStream *pOutStream) {
#ifdef HAS_PROTOCOL_RTMP
	if (TAG_KIND_OF(pOutStream->GetType(), ST_OUT_NET_RTMP)) {
		((BaseRTMPProtocol *) pOutStream->GetProtocol())->TrySetOutboundChunkSize(4 * 1024 * 1024);
		((BaseOutNetRTMPStream *) pOutStream)->SetFeederChunkSize(4 * 1024 * 1024);
		((BaseOutNetRTMPStream *) pOutStream)->CanDropFrames(true);
	}
#endif /* HAS_PROTOCOL_RTMP */
}

void InNetRTPStream::SignalOutStreamDetached(BaseOutStream *pOutStream) {
}

bool InNetRTPStream::SignalPlay(double &dts, double &length) {
	return true;
}

bool InNetRTPStream::SignalPause() {
	return true;
}

bool InNetRTPStream::SignalResume() {
	return true;
}

bool InNetRTPStream::SignalSeek(double &dts) {
	return true;
}

bool InNetRTPStream::SignalStop() {
	return true;
}

bool InNetRTPStream::FeedData(uint8_t *pData, uint32_t dataLength,
		uint32_t processedLength, uint32_t totalLength,
		double pts, double dts, bool isAudio) {
	ASSERT("Operation not supported");
	return false;
}

bool InNetRTPStream::FeedVideoData(uint8_t *pData, uint32_t dataLength,
		RTPHeader &rtpHeader) {
	if (!_hasVideo)
		return false;
	//1. Check the counter first
	if (_videoSequence == 0) {
		//this is the first packet. Make sure we start with a M packet
		if (!GET_RTP_M(rtpHeader)) {
			return true;
		}
		_videoSequence = GET_RTP_SEQ(rtpHeader);
		return true;
	} else {
		if ((uint16_t) (_videoSequence + 1) != (uint16_t) GET_RTP_SEQ(rtpHeader)) {
			WARN("Missing video packet. Wanted: %"PRIu16"; got: %"PRIu16" on stream: %s",
					(uint16_t) (_videoSequence + 1),
					(uint16_t) GET_RTP_SEQ(rtpHeader),
					STR(GetName()));
			_currentNalu.IgnoreAll();
			_videoDroppedPacketsCount++;
			_videoSequence = 0;
			return true;
		} else {
			_videoSequence++;
		}
	}

	//2. get the nalu
	uint64_t rtpTs = ComputeRTP(rtpHeader._timestamp, _videoLastRTP, _videoRTPRollCount);
	double ts = (double) rtpTs / _videoSampleRate * 1000.0;
	uint8_t naluType = NALU_TYPE(pData[0]);
	if (naluType <= 23) {
		//3. Standard NALU
		//FINEST("V: %08"PRIx32, rtpHeader._timestamp);
		_videoPacketsCount++;
		_videoBytesCount += dataLength;
		return InternalFeedData(pData, dataLength, 0, dataLength, ts, false);
	} else if (naluType == NALU_TYPE_FUA) {
		if (GETAVAILABLEBYTESCOUNT(_currentNalu) == 0) {
			_currentNalu.IgnoreAll();
			//start NAL
			if ((pData[1] >> 7) == 0) {
				WARN("Bogus nalu");
				_currentNalu.IgnoreAll();
				_videoSequence = 0;
				return true;
			}
			pData[1] = (pData[0]&0xe0) | (pData[1]&0x1f);
			_currentNalu.ReadFromBuffer(pData + 1, dataLength - 1);
			return true;
		} else {
			//middle NAL
			_currentNalu.ReadFromBuffer(pData + 2, dataLength - 2);
			if (((pData[1] >> 6)&0x01) == 1) {
				//FINEST("V: %08"PRIx32, rtpHeader._timestamp);
				_videoPacketsCount++;
				_videoBytesCount += GETAVAILABLEBYTESCOUNT(_currentNalu);
				if (!InternalFeedData(GETIBPOINTER(_currentNalu),
						GETAVAILABLEBYTESCOUNT(_currentNalu),
						0,
						GETAVAILABLEBYTESCOUNT(_currentNalu),
						ts,
						false)) {
					FATAL("Unable to feed NALU");
					return false;
				}
				_currentNalu.IgnoreAll();
			}
			return true;
		}
	} else if (naluType == NALU_TYPE_STAPA) {
		uint32_t index = 1;
		while (index + 3 < dataLength) {
			uint16_t length = ENTOHSP(pData + index);
			index += 2;
			if (index + length > dataLength) {
				WARN("Bogus STAP-A");
				_currentNalu.IgnoreAll();
				_videoSequence = 0;
				return true;
			}
			_videoPacketsCount++;
			_videoBytesCount += length;
			if (!InternalFeedData(pData + index,
					length, 0,
					length,
					ts, false)) {
				FATAL("Unable to feed NALU");
				return false;
			}
			index += length;
		}
		return true;
	} else {
		WARN("invalid NAL: %s", STR(NALUToString(naluType)));
		_currentNalu.IgnoreAll();
		_videoSequence = 0;
		return true;
	}
}

bool InNetRTPStream::FeedAudioData(uint8_t *pData, uint32_t dataLength,
		RTPHeader &rtpHeader) {
	if (!_hasAudio)
		return false;
	if (_isLatm)
		return FeedAudioDataLATM(pData, dataLength, rtpHeader);
	else
		return FeedAudioDataAU(pData, dataLength, rtpHeader);
}

void InNetRTPStream::GetStats(Variant &info, uint32_t namespaceId) {
	BaseInNetStream::GetStats(info, namespaceId);
	info["audio"]["bytesCount"] = _audioBytesCount;
	info["audio"]["packetsCount"] = _audioPacketsCount;
	info["audio"]["droppedPacketsCount"] = _audioDroppedPacketsCount;
	info["video"]["bytesCount"] = _videoBytesCount;
	info["video"]["packetsCount"] = _videoPacketsCount;
	info["video"]["droppedPacketsCount"] = _videoDroppedPacketsCount;
}

void InNetRTPStream::ReportSR(uint64_t ntpMicroseconds, uint32_t rtpTimestamp,
		bool isAudio) {
	if (isAudio) {
		_audioRTP = (double) ComputeRTP(rtpTimestamp, _lastAudioRTCPRTP,
				_audioRTCPRTPRollCount) / _audioSampleRate * 1000.0;
		_audioNTP = (double) ntpMicroseconds / 1000.0;
	} else {
		_videoRTP = (double) ComputeRTP(rtpTimestamp, _lastVideoRTCPRTP,
				_videoRTCPRTPRollCount) / _videoSampleRate * 1000.0;
		_videoNTP = (double) ntpMicroseconds / 1000.0;
	}
}

bool InNetRTPStream::FeedAudioDataAU(uint8_t *pData, uint32_t dataLength,
		RTPHeader &rtpHeader) {
	if (_audioSequence == 0) {
		//this is the first packet. Make sure we start with a M packet
		if (!GET_RTP_M(rtpHeader)) {
			return true;
		}
		_audioSequence = GET_RTP_SEQ(rtpHeader);
		return true;
	} else {
		if ((uint16_t) (_audioSequence + 1) != (uint16_t) GET_RTP_SEQ(rtpHeader)) {
			WARN("Missing audio packet. Wanted: %"PRIu16"; got: %"PRIu16" on stream: %s",
					(uint16_t) (_audioSequence + 1),
					(uint16_t) GET_RTP_SEQ(rtpHeader),
					STR(GetName()));
			_audioDroppedPacketsCount++;
			_audioSequence = 0;
			return true;
		} else {
			_audioSequence++;
		}
	}

	//1. Compute chunks count
	uint16_t chunksCount = ENTOHSP(pData);
	if ((chunksCount % 16) != 0) {
		FATAL("Invalid AU headers length: %"PRIx16, chunksCount);
		return false;
	}
	chunksCount = chunksCount / 16;

	//3. Feed the buffer chunk by chunk
	uint32_t cursor = 2 + 2 * chunksCount;
	uint16_t chunkSize = 0;
	double ts = 0;
	uint64_t rtpTs = ComputeRTP(rtpHeader._timestamp, _audioLastRTP, _audioRTPRollCount);
	for (uint32_t i = 0; i < chunksCount; i++) {
		if (i != (uint32_t) (chunksCount - 1)) {
			chunkSize = (ENTOHSP(pData + 2 + 2 * i)) >> 3;
		} else {
			chunkSize = (uint16_t) (dataLength - cursor);
		}
		ts = (double) (rtpTs + i * 1024) / _audioSampleRate * 1000.00;
		if ((cursor + chunkSize) > dataLength) {
			FATAL("Unable to feed data: cursor: %"PRIu32"; chunkSize: %"PRIu16"; dataLength: %"PRIu32"; chunksCount: %"PRIu16,
					cursor, chunkSize, dataLength, chunksCount);
			return false;
		}
		_audioPacketsCount++;
		_audioBytesCount += chunkSize;
		if (!InternalFeedData(pData + cursor - 2,
				chunkSize + 2,
				0,
				chunkSize + 2,
				ts, true)) {
			FATAL("Unable to feed data");
			return false;
		}
		cursor += chunkSize;

	}

	return true;
}

bool InNetRTPStream::FeedAudioDataLATM(uint8_t *pData, uint32_t dataLength,
		RTPHeader &rtpHeader) {
	_audioPacketsCount++;
	_audioBytesCount += dataLength;
	if (dataLength == 0)
		return true;
	uint32_t cursor = 0;
	uint32_t currentLength = 0;
	uint32_t index = 0;
	uint64_t rtpTs = ComputeRTP(rtpHeader._timestamp, _audioLastRTP, _audioRTPRollCount);
	double ts = (double) (rtpTs) / _audioSampleRate * 1000.00;
	double constant = 1024.0 / _audioSampleRate * 1000.00;

	while (cursor < dataLength) {
		currentLength = 0;
		while (cursor < dataLength) {
			uint8_t val = pData[cursor++];
			currentLength += val;
			if (val != 0xff)
				break;
		}
		if ((cursor + currentLength) > dataLength) {
			WARN("Invalid LATM packet size");
			return true;
		}
		if (!InternalFeedData(pData + cursor - 2,
				currentLength + 2,
				0,
				currentLength + 2,
				ts + (double) index * constant, true)) {
			FATAL("Unable to feed data");
			return false;
		}
		cursor += currentLength;
		//      FINEST("currentLength: %"PRIu32"; dataLength: %"PRIu32"; ts: %.2f",
		//              currentLength, dataLength, ts / 1000.0);
		index++;
	}

	return true;
}

//#define DEBUG_RTCP_PRESENCE(...) FINEST(__VA_ARGS__)
#define DEBUG_RTCP_PRESENCE(...)

//#define RTSP_DUMP_PTSDTS
#ifdef RTSP_DUMP_PTSDTS
double __lastPts = 0;
double __lastDts = 0;
#endif /* RTSP_DUMP_PTSDTS */

bool InNetRTPStream::InternalFeedData(uint8_t *pData, uint32_t dataLength,
		uint32_t processedLength, uint32_t totalLength,
		double pts, bool isAudio) {
	switch (_rtcpPresence) {
		case RTCP_PRESENCE_UNKNOWN:
		{
			DEBUG_RTCP_PRESENCE("RTCP_PRESENCE_UNKNOWN: %"PRIz"u", (time(NULL) - _rtcpDetectionStart));
			if (_rtcpDetectionInterval == 0) {
				WARN("RTCP disabled on stream %s(%"PRIu32") with name %s. A/V drifting may occur over long periods of time",
						STR(tagToString(GetType())), GetUniqueId(), STR(GetName()));
				_rtcpPresence = RTCP_PRESENCE_ABSENT;
				return true;
			}
			if (_rtcpDetectionStart == 0) {
				_rtcpDetectionStart = time(NULL);
				return true;
			}
			if ((time(NULL) - _rtcpDetectionStart) > _rtcpDetectionInterval) {
				WARN("Stream %s(%"PRIu32") with name %s doesn't have RTCP. A/V drifting may occur over long periods of time",
						STR(tagToString(GetType())), GetUniqueId(), STR(GetName()));
				_rtcpPresence = RTCP_PRESENCE_ABSENT;
				return true;
			}
			bool audioRTCPPresent = false;
			bool videoRTCPPresent = false;
			if (_hasAudio) {
				if (_audioNTP != 0)
					DEBUG_RTCP_PRESENCE("Audio RTCP detected");
				audioRTCPPresent = (_audioNTP != 0);
			} else {
				audioRTCPPresent = true;
			}
			if (_hasVideo) {
				if (_videoNTP != 0)
					DEBUG_RTCP_PRESENCE("Video RTCP detected");
				videoRTCPPresent = (_videoNTP != 0);
			} else {
				videoRTCPPresent = true;
			}
			if (audioRTCPPresent && videoRTCPPresent) {
				DEBUG_RTCP_PRESENCE("RTCP available on stream %s(%"PRIu32") with name %s.",
						STR(tagToString(GetType())), GetUniqueId(), STR(GetName()));
				_rtcpPresence = RTCP_PRESENCE_AVAILABLE;
			}
			return true;
			break;
		}
		case RTCP_PRESENCE_AVAILABLE:
		{
			DEBUG_RTCP_PRESENCE("RTCP_PRESENCE_AVAILABLE");
			double &rtp = isAudio ? _audioRTP : _videoRTP;
			double &ntp = isAudio ? _audioNTP : _videoNTP;
			pts = ntp + pts - rtp;
			break;
		}
		case RTCP_PRESENCE_ABSENT:
		{
			DEBUG_RTCP_PRESENCE("RTCP_PRESENCE_ABSENT");
			double &firstTimestamp = isAudio ? _audioFirstTimestamp : _videoFirstTimestamp;
			if (firstTimestamp < 0)
				firstTimestamp = pts;
			pts -= firstTimestamp;
			break;
		}
		default:
		{
			ASSERT("Invalid _rtcpPresence: %"PRIu8, _rtcpPresence);
			return false;
		}
	}

	double dts = -1;
	if (isAudio) {
		dts = pts;
	} else {
		if (_videoLastPts == pts) {
			dts = _videoLastDts;
		} else {
			if (_dtsCacheSize == 1) {
				dts = pts;
			} else {
				_dtsCache[pts] = pts;
				if (_dtsCache.size() >= _dtsCacheSize) {
					map<double, double>::iterator i = _dtsCache.begin();
					dts = i->first;
					_dtsCache.erase(i);
				}
			}
#ifdef RTSP_DUMP_PTSDTS
			if (__lastPts != pts) {
				FINEST("pts: %8.2f\tdts: %8.2f\tcts: %8.2f\tptsd: %+6.2f\tdtsd: %+6.2f\t%s",
						pts,
						dts,
						pts - dts,
						pts - __lastPts,
						dts - __lastDts,
						pts == dts ? "" : "DTS Present");
				__lastPts = pts;
				__lastDts = dts;
			}
#endif /* RTSP_DUMP_PTSDTS */
		}
		_videoLastPts = pts;
	}

	if (dts < 0)
		return true;

	double &lastDts = isAudio ? _audioLastDts : _videoLastDts;
	//	if (!isAudio) {
	//		FINEST("last: %.2f; current: %.2f", lastDts, dts);
	//	}

	if (lastDts > dts) {
		WARN("Back time on %s. ATS: %.08f LTS: %.08f; D: %.8f; isAudio: %d; _dtsCacheSize: %"PRIz"u",
				STR(GetName()),
				dts,
				lastDts,
				dts - lastDts,
				isAudio, _dtsCacheSize);
		if (_dtsCacheSize < 6)
			_dtsCacheSize++;
		return true;
	}
	lastDts = dts;

	if (_hasAudio && (_audioLastDts < 0))
		return true;
	if (_hasVideo && (_videoLastDts < 0))
		return true;

	if (_hasAudio && _hasVideo) {
		if ((_audioLastDts < 0) || (_videoLastDts < 0))
			return true;
	}

	if ((!isAudio)&&(dataLength > 0)&&(_hasVideo)) {
		switch (NALU_TYPE(pData[0])) {
			case NALU_TYPE_SPS:
			{
				_pps.IgnoreAll();
				_sps.IgnoreAll();
				_sps.ReadFromBuffer(pData, dataLength);
				break;
			}
			case NALU_TYPE_PPS:
			{
				_pps.IgnoreAll();
				_pps.ReadFromBuffer(pData, dataLength);
				if (!_capabilities.AddTrackVideoH264(
						GETIBPOINTER(_sps),
						GETAVAILABLEBYTESCOUNT(_sps),
						GETIBPOINTER(_pps),
						GETAVAILABLEBYTESCOUNT(_pps),
						90000,
						this
						)) {
					WARN("Unable to initialize SPS/PPS for the video track");
				}
				break;
			}
			default:
			{
				break;
			}
		}
	}

	LinkedListNode<BaseOutStream *> *pTemp = _pOutStreams;
	while (pTemp != NULL) {
		if (!pTemp->info->IsEnqueueForDelete()) {
			if (!pTemp->info->FeedData(pData, dataLength, processedLength, totalLength,
					pts, dts, isAudio)) {
				WARN("Unable to feed OS: %p", pTemp->info);
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

uint64_t InNetRTPStream::ComputeRTP(uint32_t &currentRtp, uint32_t &lastRtp,
		uint32_t &rtpRollCount) {
	if (lastRtp > currentRtp) {
		if (((lastRtp >> 31) == 0x01) && ((currentRtp >> 31) == 0x00)) {
			FINEST("RollOver");
			rtpRollCount++;
		}
	}
	lastRtp = currentRtp;
	return (((uint64_t) rtpRollCount) << 32) | currentRtp;
}

#endif /* HAS_PROTOCOL_RTP */
