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

#ifdef HAS_PROTOCOL_LIVEFLV
#include "protocols/liveflv/innetliveflvstream.h"
#include "protocols/baseprotocol.h"
#include "streaming/baseoutstream.h"
#include "streaming/streamstypes.h"
#include "protocols/rtmp/streaming/baseoutnetrtmpstream.h"
#include "protocols/rtmp/messagefactories/streammessagefactory.h"
#include "protocols/rtmp/streaming/innetrtmpstream.h"

InNetLiveFLVStream::InNetLiveFLVStream(BaseProtocol *pProtocol, string name)
: BaseInNetStream(pProtocol, ST_IN_NET_LIVEFLV, name) {
	_lastVideoPts = 0;
	_lastVideoDts = 0;

	_lastAudioTime = 0;

	_audioCapabilitiesInitialized = false;
	_videoCapabilitiesInitialized = false;
}

InNetLiveFLVStream::~InNetLiveFLVStream() {
}

StreamCapabilities * InNetLiveFLVStream::GetCapabilities() {
	return &_streamCapabilities;
}

bool InNetLiveFLVStream::FeedData(uint8_t *pData, uint32_t dataLength,
		uint32_t processedLength, uint32_t totalLength,
		double pts, double dts, bool isAudio) {
	if (isAudio) {
		_stats.audio.packetsCount++;
		_stats.audio.bytesCount += dataLength;
		if ((!_audioCapabilitiesInitialized) && (processedLength == 0)) {
			if (!InNetRTMPStream::InitializeAudioCapabilities(this,
					_streamCapabilities, _audioCapabilitiesInitialized, pData,
					dataLength)) {
				FATAL("Unable to initialize audio capabilities");
				return false;
			}
		}
		_lastAudioTime = pts;
	} else {
		_stats.video.packetsCount++;
		_stats.video.bytesCount += dataLength;
		if ((!_videoCapabilitiesInitialized) && (processedLength == 0)) {
			if (!InNetRTMPStream::InitializeVideoCapabilities(this,
					_streamCapabilities, _videoCapabilitiesInitialized, pData,
					dataLength)) {
				FATAL("Unable to initialize audio capabilities");
				return false;
			}
		}
		_lastVideoPts = pts;
		_lastVideoDts = dts;
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

void InNetLiveFLVStream::ReadyForSend() {

}

bool InNetLiveFLVStream::IsCompatibleWithType(uint64_t type) {
	return TAG_KIND_OF(type, ST_OUT_NET_RTMP)
			|| TAG_KIND_OF(type, ST_OUT_NET_RTP)
			;
}

void InNetLiveFLVStream::SignalOutStreamAttached(BaseOutStream *pOutStream) {
	if (_lastStreamMessage != V_NULL) {
		if (TAG_KIND_OF(pOutStream->GetType(), ST_OUT_NET_RTMP)) {
			if (!((BaseOutNetRTMPStream *) pOutStream)->SendStreamMessage(
					_lastStreamMessage)) {
				FATAL("Unable to send notify on stream. The connection will go down");
				pOutStream->EnqueueForDelete();
			}
		}
	}
}

void InNetLiveFLVStream::SignalOutStreamDetached(BaseOutStream *pOutStream) {

}

bool InNetLiveFLVStream::SignalPlay(double &dts, double &length) {
	return true;
}

bool InNetLiveFLVStream::SignalPause() {
	return true;
}

bool InNetLiveFLVStream::SignalResume() {
	return true;
}

bool InNetLiveFLVStream::SignalSeek(double &dts) {
	return true;
}

bool InNetLiveFLVStream::SignalStop() {
	return true;
}

bool InNetLiveFLVStream::SendStreamMessage(Variant &completeMessage, bool persistent) {
	//2. Loop on the subscribed streams and send the message
	LinkedListNode<BaseOutStream *> *pIterator = _pOutStreams;
	LinkedListNode<BaseOutStream *> *pCurrent = NULL;
	while (pIterator != NULL) {
		pCurrent = pIterator;
		pIterator = pIterator->pPrev;
		if ((pCurrent->info->IsEnqueueForDelete()) || (!TAG_KIND_OF(pCurrent->info->GetType(), ST_OUT_NET_RTMP)))
			continue;
		if (!((BaseOutNetRTMPStream *) pCurrent->info)->SendStreamMessage(completeMessage)) {
			FATAL("Unable to send notify on stream. The connection will go down");
			pCurrent->info->EnqueueForDelete();
		}
	}

	//3. Test to see if we are still alive. One of the target streams might
	//be on the same RTMP connection as this stream is and our connection
	//here might be enque for delete
	if (IsEnqueueForDelete())
		return false;

	//4. Save the message for future use if necessary
	if (persistent)
		_lastStreamMessage = completeMessage;

	//5. Done
	return true;
}

bool InNetLiveFLVStream::SendStreamMessage(string functionName, Variant &parameters,
		bool persistent) {

	//1. Prepare the message
	Variant message = StreamMessageFactory::GetFlexStreamSend(0, 0, 0, false,
			functionName, parameters);

	return SendStreamMessage(message, persistent);
}

#endif /* HAS_PROTOCOL_LIVEFLV */
