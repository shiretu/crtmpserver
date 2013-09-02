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
#include "protocols/rtp/streaming/outnetrtpudph264stream.h"
#include "streaming/nalutypes.h"
#include "streaming/streamstypes.h"
#include "streaming/baseinstream.h"
#include "protocols/rtp/connectivity/outboundconnectivity.h"
#include "streaming/codectypes.h"

#define MAX_RTP_PACKET_SIZE 1350
#define MAX_AUS_COUNT 8

OutNetRTPUDPH264Stream::OutNetRTPUDPH264Stream(BaseProtocol *pProtocol,
		string name, bool forceTcp)
: BaseOutNetRTPUDPStream(pProtocol, name) {
	_forceTcp = forceTcp;
	if (_forceTcp)
		_maxRTPPacketSize = 1500;
	else
		_maxRTPPacketSize = MAX_RTP_PACKET_SIZE;

	memset(&_videoData, 0, sizeof (_videoData));
	_videoData.MSGHDR_MSG_IOV = new IOVEC[2];
	_videoData.MSGHDR_MSG_IOVLEN = 2;
	_videoData.MSGHDR_MSG_NAMELEN = sizeof (sockaddr_in);
	_videoData.MSGHDR_MSG_IOV[0].IOVEC_IOV_BASE = new IOVEC_IOV_BASE_TYPE[14];
	((uint8_t *) _videoData.MSGHDR_MSG_IOV[0].IOVEC_IOV_BASE)[0] = 0x80;
	EHTONLP(((uint8_t *) _videoData.MSGHDR_MSG_IOV[0].IOVEC_IOV_BASE) + 8, _videoSsrc);
	_videoSampleRate = 0;

	memset(&_audioData, 0, sizeof (_audioData));
	_audioData.MSGHDR_MSG_IOV = new IOVEC[3];
	_audioData.MSGHDR_MSG_IOVLEN = 3;
	_audioData.MSGHDR_MSG_NAMELEN = sizeof (sockaddr_in);
	_audioData.MSGHDR_MSG_IOV[0].IOVEC_IOV_LEN = 14;
	_audioData.MSGHDR_MSG_IOV[0].IOVEC_IOV_BASE = new IOVEC_IOV_BASE_TYPE[14];
	((uint8_t *) _audioData.MSGHDR_MSG_IOV[0].IOVEC_IOV_BASE)[0] = 0x80;
	((uint8_t *) _audioData.MSGHDR_MSG_IOV[0].IOVEC_IOV_BASE)[1] = 0xe0;
	EHTONLP(((uint8_t *) _audioData.MSGHDR_MSG_IOV[0].IOVEC_IOV_BASE) + 8, _audioSsrc);
	_audioData.MSGHDR_MSG_IOV[1].IOVEC_IOV_LEN = 0;
	_audioData.MSGHDR_MSG_IOV[1].IOVEC_IOV_BASE = new IOVEC_IOV_BASE_TYPE[MAX_AUS_COUNT * 2];
	_audioSampleRate = 0;

	_auPts = -1;
	_auCount = 0;

	_pVideoInfo = NULL;
	_pAudioInfo = NULL;
	_firstVideoFrame = true;
	_lastVideoPts = -1;
}

OutNetRTPUDPH264Stream::~OutNetRTPUDPH264Stream() {
	delete[] (uint8_t *) _videoData.MSGHDR_MSG_IOV[0].IOVEC_IOV_BASE;
	delete[] _videoData.MSGHDR_MSG_IOV;
	memset(&_videoData, 0, sizeof (_videoData));

	delete[] (uint8_t *) _audioData.MSGHDR_MSG_IOV[0].IOVEC_IOV_BASE;
	delete[] (uint8_t *) _audioData.MSGHDR_MSG_IOV[1].IOVEC_IOV_BASE;
	delete[] _audioData.MSGHDR_MSG_IOV;
	memset(&_audioData, 0, sizeof (_audioData));
}

bool OutNetRTPUDPH264Stream::FinishInitialization(
		GenericProcessDataSetup *pGenericProcessDataSetup) {
	if (!BaseOutNetRTPUDPStream::FinishInitialization(pGenericProcessDataSetup)) {
		FATAL("Unable to finish output stream initialization");
		return false;
	}
	if (pGenericProcessDataSetup->_hasVideo) {
		_pVideoInfo = pGenericProcessDataSetup->_pStreamCapabilities->GetVideoCodec<VideoCodecInfo > ();
		_videoSampleRate = _pVideoInfo->_samplingRate;
	} else {
		_videoSampleRate = 1;
	}

	if (pGenericProcessDataSetup->_hasAudio) {
		_pAudioInfo = pGenericProcessDataSetup->_pStreamCapabilities->GetAudioCodec<AudioCodecInfo > ();
		_audioSampleRate = _pAudioInfo->_samplingRate;
	} else {
		_audioSampleRate = 1;
	}
	return true;
}

bool OutNetRTPUDPH264Stream::PushVideoData(IOBuffer &buffer, double pts, double dts,
		bool isKeyFrame) {
	if (_pVideoInfo == NULL) {
		_stats.video.droppedPacketsCount++;
		_stats.video.droppedBytesCount += GETAVAILABLEBYTESCOUNT(buffer);
		return true;
	}
	if ((isKeyFrame || _firstVideoFrame)
			&&(_pVideoInfo->_type == CODEC_VIDEO_H264)
			&&(_lastVideoPts != pts)) {
		_firstVideoFrame = false;
		//fix for mode=0 kind of transfer where we get sliced IDRs
		//only send the SPS/PPS on the first IDR slice from the keyframe
		_lastVideoPts = pts;
		VideoCodecInfoH264 *pTemp = (VideoCodecInfoH264 *) _pVideoInfo;
		if (!PushVideoData(pTemp->GetSPSBuffer(), dts, dts, false)) {
			FATAL("Unable to feed SPS");
			return false;
		}
		if (!PushVideoData(pTemp->GetPPSBuffer(), dts, dts, false)) {
			FATAL("Unable to feed PPS");
			return false;
		}
	}

	/*
	 *
	 *  0                   1                   2                   3
	 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |V=2|P|X|  CC   |M|     PT      |       sequence number         |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |                           timestamp                           |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |           synchronization source (SSRC) identifier            |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |F|NRI|   28    |S|E|R|  Type   |                               |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
	 * |                                                               |
	 * |                         FU payload                            |
	 * |                                                               |
	 * |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 * |                               :...OPTIONAL RTP padding        |
	 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *
	 *
	 *
	 * Version: (2 bits) Indicates the version of the protocol. Current version is 2.[19]
	 *
	 * P (Padding): (1 bit) Used to indicate if there are extra padding bytes at
	 * the end of the RTP packet. A padding might be used to fill up a block of
	 * certain size, for example as required by an encryption algorithm. The
	 * last byte of the padding contains the number of how many padding
	 * bytes were added (including itself).[19][13]:12
	 *
	 * X (Extension): (1 bit) Indicates presence of an Extension header between
	 * standard header and payload data. This is application or profile specific.[19]
	 *
	 * CC (CSRC Count): (4 bits) Contains the number of CSRC identifiers
	 * (defined below) that follow the fixed header.[13]:12
	 *
	 * M (Marker): (1 bit) Used at the application level and defined by a
	 * profile. If it is set, it means that the current data has some special
	 * relevance for the application.[13]:13
	 *
	 * PT (Payload Type): (7 bits) Indicates the format of the payload and
	 * determines its interpretation by the application. This is specified by an
	 * RTP profile. For example, see RTP Profile for audio and video conferences
	 * with minimal control (RFC 3551).[20]
	 *
	 * Sequence Number: (16 bits) The sequence number is incremented by one for
	 * each RTP data packet sent and is to be used by the receiver to detect
	 * packet loss and to restore packet sequence. The RTP does not specify any
	 * action on packet loss; it is left to the application to take appropriate
	 * action. For example, video applications may play the last known frame in
	 * place of the missing frame.[21] According to RFC 3550, the initial value
	 * of the sequence number should be random to make known-plaintext attacks
	 * on encryption more difficult.[13]:13 RTP provides no guarantee of delivery,
	 * but the presence of sequence numbers makes it possible to detect missing
	 * packets.[1]
	 *
	 *
	 * Timestamp: (32 bits) Used to enable the receiver to play back the received
	 * samples at appropriate intervals. When several media streams are present,
	 * the timestamps are independent in each stream, and may not be relied upon
	 * for media synchronization. The granularity of the timing is application
	 * specific. For example, an audio application that samples data once every
	 * 125 µs (8 kHz, a common sample rate in digital telephony) could use that
	 * value as its clock resolution. The clock granularity is one of the details
	 * that is specified in the RTP profile for an application.[21]
	 *
	 *
	 * SSRC: (32 bits) Synchronization source identifier uniquely identifies the
	 * source of a stream. The synchronization sources within the same RTP
	 * session will be unique.[13]:15
	 */

	uint32_t dataLength = GETAVAILABLEBYTESCOUNT(buffer);
	uint8_t *pData = GETIBPOINTER(buffer);

	uint32_t sentAmount = 0;
	uint32_t chunkSize = 0;
	while (sentAmount != dataLength) {
		chunkSize = dataLength - sentAmount;
		chunkSize = chunkSize < _maxRTPPacketSize ? chunkSize : _maxRTPPacketSize;

		//1. Flags
		if (sentAmount + chunkSize == dataLength) {
			((uint8_t *) _videoData.MSGHDR_MSG_IOV[0].IOVEC_IOV_BASE)[1] = 0xe1;
		} else {
			((uint8_t *) _videoData.MSGHDR_MSG_IOV[0].IOVEC_IOV_BASE)[1] = 0x61;
		}

		//2. counter
		EHTONSP(((uint8_t *) _videoData.MSGHDR_MSG_IOV[0].IOVEC_IOV_BASE) + 2, _videoCounter);
		_videoCounter++;

		//3. Timestamp
		EHTONLP(((uint8_t *) _videoData.MSGHDR_MSG_IOV[0].IOVEC_IOV_BASE) + 4,
				BaseConnectivity::ToRTPTS(pts, (uint32_t) _videoSampleRate));

		if (chunkSize == dataLength) {
			//4. No chunking
			_videoData.MSGHDR_MSG_IOV[0].IOVEC_IOV_LEN = 12;
			_videoData.MSGHDR_MSG_IOV[1].IOVEC_IOV_BASE = (IOVEC_IOV_BASE_TYPE *) pData;
			_videoData.MSGHDR_MSG_IOV[1].IOVEC_IOV_LEN = chunkSize;
		} else {
			//5. Chunking
			_videoData.MSGHDR_MSG_IOV[0].IOVEC_IOV_LEN = 14;

			if (sentAmount == 0) {
				//6. First chunk
				((uint8_t *) _videoData.MSGHDR_MSG_IOV[0].IOVEC_IOV_BASE)[12] = (pData[0]&0xe0) | NALU_TYPE_FUA;
				((uint8_t *) _videoData.MSGHDR_MSG_IOV[0].IOVEC_IOV_BASE)[13] = (pData[0]&0x1f) | 0x80;
				_videoData.MSGHDR_MSG_IOV[1].IOVEC_IOV_BASE = (IOVEC_IOV_BASE_TYPE *) (pData + 1);
				_videoData.MSGHDR_MSG_IOV[1].IOVEC_IOV_LEN = chunkSize - 1;
			} else {
				if (sentAmount + chunkSize == dataLength) {
					//7. Last chunk
					((uint8_t *) _videoData.MSGHDR_MSG_IOV[0].IOVEC_IOV_BASE)[13] &= 0x1f;
					((uint8_t *) _videoData.MSGHDR_MSG_IOV[0].IOVEC_IOV_BASE)[13] |= 0x40;
				} else {
					//8. Middle chunk
					((uint8_t *) _videoData.MSGHDR_MSG_IOV[0].IOVEC_IOV_BASE)[13] &= 0x1f;
				}
				_videoData.MSGHDR_MSG_IOV[1].IOVEC_IOV_BASE = (IOVEC_IOV_BASE_TYPE *) pData;
				_videoData.MSGHDR_MSG_IOV[1].IOVEC_IOV_LEN = chunkSize;
			}
		}

		_pConnectivity->FeedVideoData(_videoData, pts, dts);
		sentAmount += chunkSize;
		pData += chunkSize;
	}
	_stats.video.packetsCount++;
	_stats.video.bytesCount += GETAVAILABLEBYTESCOUNT(buffer);
	return true;
}

//#define MULTIPLE_AUS

bool OutNetRTPUDPH264Stream::PushAudioData(IOBuffer &buffer, double pts, double dts) {
	if (_pAudioInfo == NULL) {
		_stats.audio.droppedPacketsCount++;
		_stats.audio.droppedBytesCount += GETAVAILABLEBYTESCOUNT(buffer);
		return true;
	}
	uint32_t dataLength = GETAVAILABLEBYTESCOUNT(buffer);
	uint8_t *pData = GETIBPOINTER(buffer);
#ifdef MULTIPLE_AUS
	//	FINEST("_auCount: %"PRIu32"; max: %"PRIu32"; have: %"PRIu32"; total: %"PRIu32,
	//			_auCount,
	//			MAX_AUS_COUNT,
	//			12 //RTP header
	//			+ 2 //AU-headers-length
	//			+ _auCount * 2 //n instances of AU-header
	//			+ GETAVAILABLEBYTESCOUNT(_auBuffer), //existing data
	//
	//			12 //RTP header
	//			+ 2 //AU-headers-length
	//			+ _auCount * 2 //n instances of AU-header
	//			+ GETAVAILABLEBYTESCOUNT(_auBuffer) //existing data
	//			+ dataLength
	//			);
	if ((_auCount >= MAX_AUS_COUNT)
			|| ((
			12 //RTP header
			+ 2 //AU-headers-length
			+ _auCount * 2 //n instances of AU-header
			+ GETAVAILABLEBYTESCOUNT(_auBuffer) //existing data
			+ dataLength) //new data about to be appended
			> MAX_RTP_PACKET_SIZE)) {

		//5. counter
		EHTONSP(((uint8_t *) _audioData.MSGHDR_MSG_IOV[0].IOVEC_IOV_BASE) + 2, _audioCounter);
		_audioCounter++;

		//6. Timestamp
		EHTONLP(((uint8_t *) _audioData.MSGHDR_MSG_IOV[0].IOVEC_IOV_BASE) + 4,
				(uint32_t) (_auPts * _audioSampleRate / 1000.000));

		//7. AU-headers-length
		EHTONSP(((uint8_t *) _audioData.MSGHDR_MSG_IOV[0].IOVEC_IOV_BASE) + 12, _auCount * 16);

		//7. put the actual buffer
		_audioData.MSGHDR_MSG_IOV[2].IOVEC_IOV_LEN = GETAVAILABLEBYTESCOUNT(_auBuffer);
		_audioData.MSGHDR_MSG_IOV[2].IOVEC_IOV_BASE = (IOVEC_IOV_BASE_TYPE *) (GETIBPOINTER(_auBuffer));

		//8. Send the data
		//FINEST("-----SEND------");
		if (!_pConnectivity->FeedAudioData(_audioData, pts, dts)) {
			FATAL("Unable to feed data");
			return false;
		}

		_auCount = 0;
	}

	//9. reset the pts and au buffer if this is the first AU
	if (_auCount == 0) {
		_auBuffer.IgnoreAll();
		_auPts = pts;
		_audioData.MSGHDR_MSG_IOV[1].IOVEC_IOV_LEN = 0;
	}

	//10. Store the data
	_auBuffer.ReadFromBuffer(pData, dataLength);

	//11. Store the AU-header
	uint16_t auHeader = (uint16_t) ((dataLength) << 3);
	EHTONSP(((uint8_t *) _audioData.MSGHDR_MSG_IOV[1].IOVEC_IOV_BASE + 2 * _auCount), auHeader);
	_audioData.MSGHDR_MSG_IOV[1].IOVEC_IOV_LEN += 2;

	//12. increment the number of AUs
	_auCount++;

	_stats.audio.packetsCount++;
	_stats.audio.bytesCount += GETAVAILABLEBYTESCOUNT(buffer);

	//13. Done
	return true;

#else /* MULTIPLE_AUS */

	/*
	0                   1                   2                   3
	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P|X|  CC   |M|     PT      |       sequence number         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           timestamp                           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |           synchronization source (SSRC) identifier            |
   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
   |            contributing source (CSRC) identifiers             |
   |                             ....                              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- .. -+-+-+-+-+-+-+-+-+-+
   |AU-headers-length|AU-header|AU-header|      |AU-header|padding|
   |                 |   (1)   |   (2)   |      |   (n)   | bits  |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- .. -+-+-+-+-+-+-+-+-+-+
	 */

	//5. counter
	EHTONSP(((uint8_t *) _audioData.MSGHDR_MSG_IOV[0].IOVEC_IOV_BASE) + 2, _audioCounter);
	_audioCounter++;

	//6. Timestamp
	EHTONLP(((uint8_t *) _audioData.MSGHDR_MSG_IOV[0].IOVEC_IOV_BASE) + 4,
			BaseConnectivity::ToRTPTS(pts, (uint32_t) _audioSampleRate));

	//7. AU-headers-length
	EHTONSP(((uint8_t *) _audioData.MSGHDR_MSG_IOV[0].IOVEC_IOV_BASE) + 12, 16);

	//8. AU-header
	uint16_t auHeader = (uint16_t) ((dataLength) << 3);
	EHTONSP(((uint8_t *) _audioData.MSGHDR_MSG_IOV[1].IOVEC_IOV_BASE), auHeader);
	_audioData.MSGHDR_MSG_IOV[1].IOVEC_IOV_LEN = 2;

	//7. put the actual buffer
	_audioData.MSGHDR_MSG_IOV[2].IOVEC_IOV_LEN = dataLength;
	_audioData.MSGHDR_MSG_IOV[2].IOVEC_IOV_BASE = (IOVEC_IOV_BASE_TYPE *) (pData);

	if (!_pConnectivity->FeedAudioData(_audioData, pts, dts)) {
		FATAL("Unable to feed data");
		return false;
	}

	_stats.audio.packetsCount++;
	_stats.audio.bytesCount += GETAVAILABLEBYTESCOUNT(buffer);

	return true;
#endif /* MULTIPLE_AUS */
}

bool OutNetRTPUDPH264Stream::IsCodecSupported(uint64_t codec) {
	return (codec == CODEC_VIDEO_H264)
			|| (codec == CODEC_AUDIO_AAC)
			;
}

void OutNetRTPUDPH264Stream::SignalAudioStreamCapabilitiesChanged(
		StreamCapabilities *pCapabilities, AudioCodecInfo *pOld,
		AudioCodecInfo *pNew) {
	GenericSignalAudioStreamCapabilitiesChanged(pCapabilities, pOld, pNew);
	if ((pNew == NULL) || (!IsCodecSupported(pNew->_type))) {
		_pAudioInfo = NULL;
		_audioSampleRate = 1;
	}
	_pAudioInfo = pNew;
	_audioSampleRate = _pAudioInfo->_samplingRate;
}

void OutNetRTPUDPH264Stream::SignalVideoStreamCapabilitiesChanged(
		StreamCapabilities *pCapabilities, VideoCodecInfo *pOld,
		VideoCodecInfo *pNew) {
	GenericSignalVideoStreamCapabilitiesChanged(pCapabilities, pOld, pNew);
	if ((pNew == NULL) || (!IsCodecSupported(pNew->_type))) {
		_pVideoInfo = NULL;
		_videoSampleRate = 1;
	}
	_pVideoInfo = pNew;
	_firstVideoFrame = true;
	_videoSampleRate = _pVideoInfo->_samplingRate;
}

void OutNetRTPUDPH264Stream::SignalAttachedToInStream() {
}
#endif /* HAS_PROTOCOL_RTP */

