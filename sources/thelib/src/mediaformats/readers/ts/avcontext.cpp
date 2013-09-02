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

#ifdef HAS_MEDIA_TS
#include "mediaformats/readers/ts/avcontext.h"
#include "mediaformats/readers/ts/tsparsereventsink.h"
#include "streaming/streamcapabilities.h"
#include "streaming/codectypes.h"
#include "streaming/nalutypes.h"
#include "streaming/baseinstream.h"

BaseAVContext::BaseAVContext() {
	InternalReset();
};

BaseAVContext::~BaseAVContext() {
	InternalReset();
}

void BaseAVContext::Reset() {
	InternalReset();
}

void BaseAVContext::DropPacket() {
	_bucket.IgnoreAll();
	_droppedPacketsCount++;
}

BaseInStream *BaseAVContext::GetInStream() {
	if (_pEventsSink == NULL)
		return NULL;
	return _pEventsSink->GetInStream();
}

bool BaseAVContext::FeedData(uint8_t *pData, uint32_t dataLength, double pts,
		double dts, bool isAudio) {
	if (_pEventsSink != NULL)
		return _pEventsSink->FeedData(this, pData, dataLength, pts, dts, isAudio);
	return true;
}

void BaseAVContext::InternalReset() {
	memset(&_pts, 0, sizeof (_pts));
	memset(&_dts, 0, sizeof (_dts));
	_sequenceNumber = -1;
	_droppedPacketsCount = 0;
	_droppedBytesCount = 0;
	_packetsCount = 0;
	_bytesCount = 0;
	_bucket.IgnoreAll();
	_pStreamCapabilities = NULL;
}

H264AVContext::H264AVContext()
: BaseAVContext() {
	InternalReset();
}

H264AVContext::~H264AVContext() {
	InternalReset();
}

void H264AVContext::Reset() {
	BaseAVContext::Reset();
	InternalReset();
}

bool H264AVContext::HandleData() {
	if (_pts.time < 0 || GETAVAILABLEBYTESCOUNT(_bucket) == 0) {
		_droppedPacketsCount++;
		_droppedBytesCount += GETAVAILABLEBYTESCOUNT(_bucket);
		_bucket.IgnoreAll();
		return true;
	}
	_packetsCount++;
	_bytesCount += GETAVAILABLEBYTESCOUNT(_bucket);

	uint32_t cursor = 0;
	uint32_t length = GETAVAILABLEBYTESCOUNT(_bucket);
	uint8_t *pBuffer = GETIBPOINTER(_bucket);
	uint8_t *pNalStart = NULL;
	uint8_t *pNalEnd = NULL;
	uint32_t testValue;
	uint8_t markerSize = 0;
	bool found = false;

	while (cursor + 4 < length) {
		testValue = ENTOHLP(pBuffer + cursor);
		if (testValue == 1) {
			markerSize = 4;
			pNalEnd = pBuffer + cursor;
			found = true;
		} else if ((testValue >> 8) == 1) {
			markerSize = 3;
			pNalEnd = pBuffer + cursor;
			found = true;
		}
		if (!found) {
			cursor++;
			continue;
		} else {
			cursor += markerSize;
		}
		found = false;
		if (pNalStart != NULL) {
			if (!ProcessNal(pNalStart, (int32_t) (pNalEnd - pNalStart), _pts.time, _dts.time)) {
				FATAL("Unable to process NAL");
				return false;
			}
		}
		pNalStart = pNalEnd + markerSize;
	}
	if (pNalStart != NULL) {
		int32_t lastLength = (int32_t) (length - (pNalStart - pBuffer));
		if (!ProcessNal(pNalStart, lastLength, _pts.time, _dts.time)) {
			FATAL("Unable to process NAL");
			return false;
		}
	}
	_bucket.IgnoreAll();
	return true;
}

void H264AVContext::EmptyBackBuffers() {

	FOR_VECTOR(_backBuffers, i) {
		ADD_VECTOR_END(_backBuffersCache, _backBuffers[i]);
	}
	_backBuffers.clear();
}

void H264AVContext::DiscardBackBuffers() {
	_backBuffersPts = -1;
	_backBuffersDts = -1;

	FOR_VECTOR(_backBuffers, i) {
		delete _backBuffers[i];
	}
	_backBuffers.clear();

	FOR_VECTOR(_backBuffersCache, i) {
		delete _backBuffersCache[i];
	}
	_backBuffersCache.clear();
}

void H264AVContext::InsertBackBuffer(uint8_t *pBuffer, int32_t length) {
	IOBuffer *pTemp = NULL;
	if (_backBuffersCache.size() > 0) {
		pTemp = _backBuffersCache[0];
		_backBuffersCache.erase(_backBuffersCache.begin());
	} else {
		pTemp = new IOBuffer();
	}
	pTemp->IgnoreAll();
	pTemp->ReadFromBuffer(pBuffer, length);
	ADD_VECTOR_END(_backBuffers, pTemp);
}

bool H264AVContext::ProcessNal(uint8_t *pBuffer, int32_t length, double pts, double dts) {
	if ((pBuffer == NULL) || (length <= 0))
		return true;
	if (_pStreamCapabilities != NULL) {
		InitializeCapabilities(pBuffer, length);
		if (_pStreamCapabilities->GetVideoCodecType() != CODEC_VIDEO_H264) {
			if (_backBuffersPts != pts) {
				EmptyBackBuffers();
				_backBuffersPts = pts;
				_backBuffersDts = dts;
			}
			InsertBackBuffer(pBuffer, length);
			return true;
		} else {
			if (_backBuffersPts >= 0) {

				FOR_VECTOR(_backBuffers, i) {
					if (!FeedData(
							GETIBPOINTER(*_backBuffers[i]),
							GETAVAILABLEBYTESCOUNT(*_backBuffers[i]),
							_backBuffersPts,
							_backBuffersDts,
							false)) {
						DiscardBackBuffers();
						return false;
					}
				}
				DiscardBackBuffers();
			}
		}
	}

	return FeedData(pBuffer, length, pts, dts, false);
}

void H264AVContext::InitializeCapabilities(uint8_t *pData, uint32_t length) {
	switch (NALU_TYPE(pData[0])) {
		case NALU_TYPE_SPS:
		{
			_SPS.IgnoreAll();
			_SPS.ReadFromBuffer(pData, length);
			break;
		}
		case NALU_TYPE_PPS:
		{
			if (GETAVAILABLEBYTESCOUNT(_SPS) == 0)
				break;
			_PPS.IgnoreAll();
			_PPS.ReadFromBuffer(pData, length);
			BaseInStream *pInStream = NULL;
			if (_pEventsSink != NULL)
				pInStream = _pEventsSink->GetInStream();
			if (_pStreamCapabilities->AddTrackVideoH264(
					GETIBPOINTER(_SPS),
					GETAVAILABLEBYTESCOUNT(_SPS),
					GETIBPOINTER(_PPS),
					GETAVAILABLEBYTESCOUNT(_PPS),
					90000,
					pInStream) == NULL) {
				_pStreamCapabilities->ClearVideo();
				WARN("Unable to initialize h264 codec");
				break;
			}
			break;
		}
		default:
		{
			break;
		}
	}
}

void H264AVContext::InternalReset() {
	_SPS.IgnoreAll();
	_PPS.IgnoreAll();
	DiscardBackBuffers();
}

AACAVContext::AACAVContext()
: BaseAVContext() {
	InternalReset();
}

AACAVContext::~AACAVContext() {
	InternalReset();
}

void AACAVContext::Reset() {
	BaseAVContext::Reset();
	InternalReset();
}

#define TS_MAX_ADTS_DETECTION_COUNT 1024

bool AACAVContext::HandleData() {
	if (_pts.time < 0) {
		_bucket.IgnoreAll();
		return true;
	}

	_bytesCount += GETAVAILABLEBYTESCOUNT(_bucket);
	_packetsCount++;

	//2. Get the buffer details: length and pointer
	uint32_t bufferLength = GETAVAILABLEBYTESCOUNT(_bucket);
	uint8_t *pBuffer = GETIBPOINTER(_bucket);

	if (!_initialMarkerFound) {
		for (;;) {
			bufferLength = GETAVAILABLEBYTESCOUNT(_bucket);
			pBuffer = GETIBPOINTER(_bucket);
			//3. Do we have at least 6 bytes to read the length?
			if (bufferLength < 6) {
				break;
			}

			if ((ENTOHSP(pBuffer)&0xfff0) != 0xfff0) {
				_bucket.Ignore(1);
				_droppedBytesCount++;
				_markerRetryCount++;
				//FINEST("_markerRetryCount: %"PRIu32, _markerRetryCount);
				if (_markerRetryCount >= TS_MAX_ADTS_DETECTION_COUNT) {
					BaseInStream *pInStream = GetInStream();
					FATAL("Unable to reliably detect AAC ADTS headers after %"PRIu32" bytes scanned for ADTS marker. Stream %s corrupted",
							TS_MAX_ADTS_DETECTION_COUNT,
							pInStream != NULL ? STR(*pInStream) : "");
					return false;
				}
				continue;
			}

			//the payload here respects this format:
			//6.2  Audio Data Transport Stream, ADTS
			//iso13818-7 page 26/206
			if (_pStreamCapabilities != NULL) {
				if (_pStreamCapabilities->GetAudioCodecType() != CODEC_AUDIO_AAC) {
					InitializeCapabilities(GETIBPOINTER(_bucket), GETAVAILABLEBYTESCOUNT(_bucket));
					if (_pStreamCapabilities->GetAudioCodecType() != CODEC_AUDIO_AAC) {
						_pStreamCapabilities->ClearAudio();
						_bucket.Ignore(1);
						_droppedBytesCount++;
						_markerRetryCount++;
						continue;
					}
				}
			}

			_initialMarkerFound = true;
			break;
		}
	}

	uint32_t _audioPacketsCount = 0;

	for (;;) {
		bufferLength = GETAVAILABLEBYTESCOUNT(_bucket);
		pBuffer = GETIBPOINTER(_bucket);

		//3. Do we have at least 6 bytes to read the length?
		if (bufferLength < 6) {
			break;
		}

		if ((ENTOHSP(pBuffer)&0xfff0) != 0xfff0) {
			_bucket.Ignore(1);
			_droppedBytesCount++;
			continue;
		}

		//4. Read the frame length and see if we have enough data.
		//NOTE: frameLength INCLUDES the headers, that is why we test it directly
		//on bufferLength
		uint32_t frameLength = ((((pBuffer[3]&0x03) << 8) | pBuffer[4]) << 3) | (pBuffer[5] >> 5);
		if (frameLength < 8) {
			//WARN("Bogus frameLength %u. Skip one byte", frameLength);
			//FINEST("_audioBuffer:\n%s", STR(_bucket));
			_bucket.Ignore(1);
			continue;
		}
		if (bufferLength < frameLength) {
			break;
		}

		double ts = _pts.time + (((double) _audioPacketsCount * 1024.00) / _samplingRate)*1000.0;
		_audioPacketsCount++;
		if (_lastSentTimestamp >= ts) {
			ts = _lastSentTimestamp;
		}
		_lastSentTimestamp = ts;

		//5. Feed
		if (!FeedData(pBuffer, frameLength, ts, ts, true)) {
			FATAL("Unable to feed audio data");
			return false;
		}

		//6. Ignore frameLength bytes
		_bucket.Ignore(frameLength);
	}

	return true;
}

void AACAVContext::InitializeCapabilities(uint8_t *pData, uint32_t length) {
	if (_pStreamCapabilities->GetAudioCodecType() == CODEC_AUDIO_UNKNOWN) {
		_samplingRate = 1;
		BitArray codecSetup;
		//profile_index from MPEG-TS different from profile_index from RTMP
		//so we need a map
		uint8_t profile = pData[2] >> 6;
		codecSetup.PutBits<uint8_t > (profile + 1, 5);

		//frequence mapping is the same
		//iso13818-7.pdf page 46/206
		uint8_t sampling_frequency_index = (pData[2] >> 2)&0x0f;
		codecSetup.PutBits<uint8_t > (sampling_frequency_index, 4);

		uint8_t channelsCount = ((pData[2]&0x01) << 2) | (pData[3] >> 6);
		codecSetup.PutBits<uint8_t > (channelsCount, 4);

		BaseInStream *pInStream = NULL;
		if (_pEventsSink != NULL)
			pInStream = _pEventsSink->GetInStream();

		AudioCodecInfoAAC *pCodecInfo = _pStreamCapabilities->AddTrackAudioAAC(GETIBPOINTER(codecSetup),
				(uint8_t) GETAVAILABLEBYTESCOUNT(codecSetup), true, pInStream);
		if (pCodecInfo == NULL)
			return;
		_samplingRate = pCodecInfo->_samplingRate;
	}
}

void AACAVContext::InternalReset() {
	_lastSentTimestamp = 0;
	_samplingRate = 1;
	_initialMarkerFound = false;
	_markerRetryCount = 0;
}
#endif	/* HAS_MEDIA_TS */
