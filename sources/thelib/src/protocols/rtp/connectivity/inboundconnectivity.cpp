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
#include "protocols/rtp/connectivity/inboundconnectivity.h"
#include "protocols/baseprotocol.h"
#include "protocols/protocolfactorymanager.h"
#include "protocols/rtp/inboundrtpprotocol.h"
#include "protocols/rtp/rtcpprotocol.h"
#include "protocols/rtp/rtspprotocol.h"
#include "netio/netio.h"
#include "protocols/rtp/sdp.h"
#include "application/baseclientapplication.h"
#include "protocols/rtp/streaming/innetrtpstream.h"
#include "protocols/udpprotocol.h"
#include "protocols/http/basehttpprotocol.h"
#include "streaming/codectypes.h"
#include "protocols/protocolmanager.h"

InboundConnectivity::InboundConnectivity(RTSPProtocol *pRTSP, string streamName,
		uint32_t bandwidthHint, uint8_t rtcpDetectionInterval)
: BaseConnectivity() {
	_pRTSP = pRTSP;
	_rtpVideoId = 0;
	_rtcpVideoId = 0;
	_rtpAudioId = 0;
	_rtcpAudioId = 0;
	_pInStream = NULL;
	_forceTcp = false;
	memset(_pProtocols, 0, sizeof (_pProtocols));
	memset(&_dummyAddress, 0, sizeof (_dummyAddress));

	memset(_audioRR, 0, sizeof (_audioRR));
	_audioRR[0] = '$'; //marker
	_audioRR[1] = 0; //channel
	_audioRR[2] = 0; //size
	_audioRR[3] = 56; //size
	_audioRR[4] = 0x81; //V,P,RC
	_audioRR[5] = 0xc9; //PT
	_audioRR[6] = 0x00; //length
	_audioRR[7] = 0x07; //length
	EHTONLP(_audioRR + 16, 0x00ffffff); //fraction lost/cumulative number of packets lost
	EHTONLP(_audioRR + 24, 0); //interarrival jitter
	EHTONLP(_audioRR + 32, 0); // delay since last SR (DLSR)
	_audioRR[36] = 0x81; //V,P,RC
	_audioRR[37] = 0xca; //PT
	_audioRR[38] = 0x00; //length
	_audioRR[39] = 0x05; //length
	_audioRR[44] = 0x01; //type
	_audioRR[45] = 0x0d; //length
	memcpy(_audioRR + 46, "machine.local", 0x0d); //name of the machine
	_audioRR[59] = 0; //padding

	memset(_videoRR, 0, sizeof (_videoRR));
	_videoRR[0] = '$'; //marker
	_videoRR[1] = 0; //channel
	_videoRR[2] = 0; //size
	_videoRR[3] = 56; //size
	_videoRR[4] = 0x81; //V,P,RC
	_videoRR[5] = 0xc9; //PT
	_videoRR[6] = 0x00; //length
	_videoRR[7] = 0x07; //length
	EHTONLP(_videoRR + 16, 0x00ffffff); //fraction lost/cumulative number of packets lost
	EHTONLP(_videoRR + 24, 0); //interarrival jitter
	EHTONLP(_videoRR + 32, 0); // delay since last SR (DLSR)
	_videoRR[36] = 0x81; //V,P,RC
	_videoRR[37] = 0xca; //PT
	_videoRR[38] = 0x00; //length
	_videoRR[39] = 0x05; //length
	_videoRR[44] = 0x01; //type
	_videoRR[45] = 0x0d; //length
	memcpy(_videoRR + 46, "machine.local", 0x0d); //name of the machine
	_videoRR[59] = 0; //padding

	_streamName = streamName;
	_bandwidthHint = bandwidthHint;
	_rtcpDetectionInterval = rtcpDetectionInterval;
}

InboundConnectivity::~InboundConnectivity() {
	Cleanup();
}

void InboundConnectivity::EnqueueForDelete() {
	Cleanup();
	_pRTSP->EnqueueForDelete();
}

bool InboundConnectivity::AddTrack(Variant& track, bool isAudio) {
	Variant &_track = isAudio ? _audioTrack : _videoTrack;
	Variant &_oppositeTrack = isAudio ? _videoTrack : _audioTrack;
	uint32_t &rtpId = isAudio ? _rtpAudioId : _rtpVideoId;
	uint32_t &rtcpId = isAudio ? _rtcpAudioId : _rtcpVideoId;
	uint8_t *pRR = isAudio ? _audioRR : _videoRR;

	if (_track != V_NULL)
		return false;

	BaseClientApplication *pApplication = _pRTSP->GetApplication();
	if (pApplication == NULL) {
		FATAL("RTSP protocol not yet assigned to an application");
		return false;
	}

	_track = track;
	if (_oppositeTrack != V_NULL) {
		if (_oppositeTrack["isTcp"] != _track["isTcp"])
			return false;
	}
	_forceTcp = (bool)_track["isTcp"];

	Variant dummy;
	InboundRTPProtocol *pRTP = (InboundRTPProtocol *) ProtocolFactoryManager::CreateProtocolChain(
			CONF_PROTOCOL_INBOUND_UDP_RTP, dummy);
	if (pRTP == NULL) {
		FATAL("Unable to create the protocol chain");
		Cleanup();
		return false;
	}
	rtpId = pRTP->GetId();

	RTCPProtocol *pRTCP = (RTCPProtocol *) ProtocolFactoryManager::CreateProtocolChain(
			CONF_PROTOCOL_UDP_RTCP, dummy);
	if (pRTCP == NULL) {
		FATAL("Unable to create the protocol chain");
		Cleanup();
		return false;
	}
	rtcpId = pRTCP->GetId();

	if ((bool)_track["isTcp"]) {
		uint16_t dataIdx = 0;
		uint16_t rtcpIdx = 0;

		//2. Add them in the fast-pickup array
		if ((_track.HasKeyChain(V_UINT16, true, 2, "portsOrChannels", "data"))
				&& (_track.HasKeyChain(V_UINT16, true, 2, "portsOrChannels", "rtcp"))) {
			dataIdx = (uint16_t) _track["portsOrChannels"]["data"];
			rtcpIdx = (uint16_t) _track["portsOrChannels"]["rtcp"];
		} else {
			uint8_t idx = (uint8_t) ((uint32_t) SDP_TRACK_GLOBAL_INDEX(_track)*2);
			dataIdx = idx;
			rtcpIdx = idx + 1;
		}

		if ((dataIdx >= 256) || (rtcpIdx >= 256)) {
			FATAL("Invalid channel numbers");
			Cleanup();
			return false;
		}
		if ((_pProtocols[dataIdx] != NULL) || (_pProtocols[rtcpIdx] != NULL)) {
			FATAL("Invalid channel numbers");
			Cleanup();
			return false;
		}
		_pProtocols[dataIdx] = pRTP;
		_pProtocols[rtcpIdx] = pRTCP;
		EHTONLP(pRR + 8, pRTCP->GetSSRC()); //SSRC of packet sender
		EHTONLP(pRR + 40, pRTCP->GetSSRC()); //SSRC of packet sender
		pRR[1] = (uint8_t) rtcpIdx;
	} else {
		if (!CreateCarriers(pRTP, pRTCP)) {
			FATAL("Unable to create carriers");
			Cleanup();
			return false;
		}
	}
	pRTP->SetApplication(pApplication);
	pRTCP->SetApplication(pApplication);
	return true;
}

bool InboundConnectivity::Initialize() {
	//1. get the application
	BaseClientApplication *pApplication = _pRTSP->GetApplication();
	if (pApplication == NULL) {
		FATAL("RTSP protocol not yet assigned to an application");
		return false;
	}

	//2. Compute the bandwidthHint
	uint32_t bandwidth = 0;
	if (_videoTrack != V_NULL) {
		bandwidth += (uint32_t) SDP_TRACK_BANDWIDTH(_videoTrack);
	}
	if (_audioTrack != V_NULL) {
		bandwidth += (uint32_t) SDP_TRACK_BANDWIDTH(_audioTrack);
	}
	if (bandwidth == 0) {
		bandwidth = _bandwidthHint;
	}

	//5. Create the in stream
	if (_streamName == "")
		_streamName = format("rtsp_%u", _pRTSP->GetId());
	if (!pApplication->StreamNameAvailable(_streamName, _pRTSP)) {
		FATAL("Stream name %s already taken", STR(_streamName));
		return false;
	}
	_pInStream = new InNetRTPStream(_pRTSP, _streamName, _videoTrack, _audioTrack,
			bandwidth, _rtcpDetectionInterval);
	if (!_pInStream->SetStreamsManager(pApplication->GetStreamsManager())) {
		FATAL("Unable to set the streams manager");
		delete _pInStream;
		_pInStream = NULL;
		return false;
	}

	//	pCap->avc._widthOverride=session["width"];
	//	pCap->avc._widthOverride=session[""];

	//6. make the stream known to inbound RTP protocols
	//and plug in the connectivity
	InboundRTPProtocol *pTempRTP = NULL;
	RTCPProtocol *pTempRTCP = NULL;

	if ((pTempRTP = (InboundRTPProtocol *) ProtocolManager::GetProtocol(_rtpVideoId)) != NULL) {
		pTempRTP->SetStream(_pInStream, false, false);
		pTempRTP->SetInbboundConnectivity(this);
	}
	if ((pTempRTCP = (RTCPProtocol *) ProtocolManager::GetProtocol(_rtcpVideoId)) != NULL) {
		pTempRTCP->SetInbboundConnectivity(this, false);
	}
	if ((pTempRTP = (InboundRTPProtocol *) ProtocolManager::GetProtocol(_rtpAudioId)) != NULL) {
		pTempRTP->SetStream(_pInStream, true, false);
		pTempRTP->SetInbboundConnectivity(this);
	}
	if ((pTempRTCP = (RTCPProtocol *) ProtocolManager::GetProtocol(_rtcpAudioId)) != NULL) {
		pTempRTCP->SetInbboundConnectivity(this, true);
	}

	//7. Pickup all outbound waiting streams
	map<uint32_t, BaseOutStream *> subscribedOutStreams =
			pApplication->GetStreamsManager()->GetWaitingSubscribers(
			_streamName, _pInStream->GetType(), true);
	//FINEST("subscribedOutStreams count: %"PRIz"u", subscribedOutStreams.size());


	//8. Bind the waiting subscribers

	FOR_MAP(subscribedOutStreams, uint32_t, BaseOutStream *, i) {
		BaseOutStream *pBaseOutStream = MAP_VAL(i);
		pBaseOutStream->Link(_pInStream);
	}

	//10. Done
	return true;
}

string InboundConnectivity::GetTransportHeaderLine(bool isAudio, bool isClient) {
	if (_forceTcp) {
		uint32_t &rtpId = isAudio ? _rtpAudioId : _rtpVideoId;
		for (uint32_t i = 0; i < 255; i++) {
			if ((_pProtocols[i] != NULL) && (_pProtocols[i]->GetId() == rtpId)) {
				string result = format("RTP/AVP/TCP;unicast;interleaved=%u-%u", i, i + 1);
				return result;
			}
		}
		return "";
	} else {
		Variant &track = isAudio ? _audioTrack : _videoTrack;
		InboundRTPProtocol *pRTP = (InboundRTPProtocol *) ProtocolManager::GetProtocol(
				isAudio ? _rtpAudioId : _rtpVideoId);
		RTCPProtocol *pRTCP = (RTCPProtocol *) ProtocolManager::GetProtocol(
				isAudio ? _rtcpAudioId : _rtcpVideoId);
		if ((pRTP == NULL) || (pRTCP == NULL)) {
			return "";
		}
		if (isClient) {
			return format("RTP/AVP;unicast;client_port=%"PRIu16"-%"PRIu16,
					((UDPCarrier *) pRTP->GetIOHandler())->GetNearEndpointPort(),
					((UDPCarrier *) pRTCP->GetIOHandler())->GetNearEndpointPort());
		} else {
			return format("RTP/AVP;unicast;client_port=%s;server_port=%"PRIu16"-%"PRIu16,
					STR(track["portsOrChannels"]["all"]),
					((UDPCarrier *) pRTP->GetIOHandler())->GetNearEndpointPort(),
					((UDPCarrier *) pRTCP->GetIOHandler())->GetNearEndpointPort());
		}
	}
}

bool InboundConnectivity::FeedData(uint32_t channelId, uint8_t *pBuffer,
		uint32_t bufferLength) {
	//1. Is the chanel number a valid chanel?
	if (channelId >= 4) {
		FATAL("Invalid chanel number: %u", channelId);
		return false;
	}

	//2. Get the protocol
	BaseProtocol *pProtocol = _pProtocols[channelId];
	if (pProtocol == NULL) {
		FATAL("Invalid chanel number: %u", channelId);
		return false;
	}

	//3. prepare the buffer
	_inputBuffer.IgnoreAll();
	_inputBuffer.ReadFromBuffer(pBuffer, bufferLength);

	//4. feed the data
	return pProtocol->SignalInputData(_inputBuffer, &_dummyAddress);
}

string InboundConnectivity::GetClientPorts(bool isAudio) {
	InboundRTPProtocol *pRTP = (InboundRTPProtocol *) ProtocolManager::GetProtocol(
			isAudio ? _rtpAudioId : _rtpVideoId);
	RTCPProtocol *pRTCP = (RTCPProtocol *) ProtocolManager::GetProtocol(
			isAudio ? _rtcpAudioId : _rtcpVideoId);
	if ((pRTP == NULL) || (pRTCP == NULL))
		return "";
	return format("%"PRIu16"-%"PRIu16,
			((UDPCarrier *) pRTP->GetIOHandler())->GetNearEndpointPort(),
			((UDPCarrier *) pRTCP->GetIOHandler())->GetNearEndpointPort());
}

string InboundConnectivity::GetAudioClientPorts() {
	return GetClientPorts(true);
}

string InboundConnectivity::GetVideoClientPorts() {
	return GetClientPorts(false);
}

bool InboundConnectivity::SendRR(bool isAudio) {
	if (_forceTcp)
		return true;
	/*
			0                   1                   2                   3
			0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	header |V=2|P|    RC   |   PT=RR=201   |             length            |0
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   |                     SSRC of packet sender                     |4
		   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
	report |                 SSRC_1 (SSRC of first source)                 |8
	block  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	  1    | fraction lost |       cumulative number of packets lost       |12
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   |           extended highest sequence number received           |16
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   |                      interarrival jitter                      |20
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   |                         last SR (LSR)                         |24
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   |                   delay since last SR (DLSR)                  |28
		   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
		   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	header |V=2|P|    SC   |  PT=SDES=202  |             length            |
		   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
	chunk  |                          SSRC/CSRC_1                          |
	  1    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   |                           SDES items                          |
		   |                              ...                              |
		   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
	chunk  |                          SSRC/CSRC_2                          |
	  2    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		   |                           SDES items                          |
		   |                              ...                              |
		   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
	 */

	InboundRTPProtocol *pRTP = (InboundRTPProtocol *) ProtocolManager::GetProtocol(
			isAudio ? _rtpAudioId : _rtpVideoId);
	RTCPProtocol *pRTCP = (RTCPProtocol *) ProtocolManager::GetProtocol(
			isAudio ? _rtcpAudioId : _rtcpVideoId);
	if ((pRTP == NULL) || (pRTCP == NULL))
		return true;

	uint8_t *pBuffer = isAudio ? _audioRR : _videoRR;

	//1. prepare the buffer
	EHTONLP(pBuffer + 12, pRTP->GetSSRC()); //SSRC_1 (SSRC of first source)
	EHTONLP(pBuffer + 20, pRTP->GetExtendedSeq()); //extended highest sequence number received
	EHTONLP(pBuffer + 28, pRTCP->GetLastSenderReport()); //last SR (LSR)

	if (_forceTcp) {
		return _pRTSP->SendRaw(pBuffer, 60, true);
	} else {
		if (pRTCP->GetLastAddress() != NULL) {
			if (sendto(pRTCP->GetIOHandler()->GetOutboundFd(),
					(char *) (pBuffer + 4), 56, 0,
					(sockaddr *) pRTCP->GetLastAddress(), sizeof (sockaddr)) != 56) {
				int err = LASTSOCKETERROR;
				FATAL("Unable to send data: %d", err);
				return false;
			}
			ADD_OUT_BYTES_MANAGED(IOHT_UDP_CARRIER, 56);
		}
		return true;
	}
}

void InboundConnectivity::ReportSR(uint64_t ntpMicroseconds,
		uint32_t rtpTimestamp, bool isAudio) {
	if (_pInStream != NULL) {
		_pInStream->ReportSR(ntpMicroseconds, rtpTimestamp, isAudio);
	}
}

void InboundConnectivity::Cleanup() {
	_audioTrack.Reset();
	_videoTrack.Reset();
	memset(_pProtocols, 0, sizeof (_pProtocols));
	if (_pInStream != NULL) {
		delete _pInStream;
		_pInStream = NULL;
	}

	InboundRTPProtocol *pTempRTP = NULL;
	RTCPProtocol *pTempRTCP = NULL;

	if ((pTempRTP = (InboundRTPProtocol *) ProtocolManager::GetProtocol(_rtpVideoId)) != NULL) {
		pTempRTP->SetStream(NULL, false, false);
		pTempRTP->EnqueueForDelete();
		_rtpVideoId = 0;
	}
	if ((pTempRTCP = (RTCPProtocol *) ProtocolManager::GetProtocol(_rtcpVideoId)) != NULL) {
		pTempRTCP->EnqueueForDelete();
		_rtcpVideoId = 0;
	}
	if ((pTempRTP = (InboundRTPProtocol *) ProtocolManager::GetProtocol(_rtpAudioId)) != NULL) {
		pTempRTP->SetStream(NULL, true, false);
		pTempRTP->EnqueueForDelete();
		_rtpAudioId = 0;
	}
	if ((pTempRTCP = (RTCPProtocol *) ProtocolManager::GetProtocol(_rtcpAudioId)) != NULL) {
		pTempRTCP->EnqueueForDelete();
		_rtcpAudioId = 0;
	}
}

bool InboundConnectivity::CreateCarriers(InboundRTPProtocol *pRTP, RTCPProtocol * pRTCP) {
	UDPCarrier *pCarrier1 = NULL;
	UDPCarrier *pCarrier2 = NULL;
	for (uint32_t i = 0; i < 10; i++) {
		if (pCarrier1 != NULL) {
			delete pCarrier1;
			pCarrier1 = NULL;
		}
		if (pCarrier2 != NULL) {
			delete pCarrier2;
			pCarrier2 = NULL;
		}

		pCarrier1 = UDPCarrier::Create("0.0.0.0", 0, 256, 256, "");
		if (pCarrier1 == NULL) {
			WARN("Unable to create UDP carrier for RTP");
			continue;
		}

		if ((pCarrier1->GetNearEndpointPort() % 2) == 0) {
			pCarrier2 = UDPCarrier::Create("0.0.0.0",
					pCarrier1->GetNearEndpointPort() + 1, 256, 256, "");
		} else {
			pCarrier2 = UDPCarrier::Create("0.0.0.0",
					pCarrier1->GetNearEndpointPort() - 1, 256, 256, "");
		}

		if (pCarrier2 == NULL) {
			WARN("Unable to create UDP carrier for RTP");
			continue;
		}

		if (pCarrier1->GetNearEndpointPort() > pCarrier2->GetNearEndpointPort()) {
			//WARN("Switch carriers");
			UDPCarrier *pTemp = pCarrier1;
			pCarrier1 = pCarrier2;
			pCarrier2 = pTemp;
		}

		pCarrier1->SetProtocol(pRTP->GetFarEndpoint());
		pRTP->GetFarEndpoint()->SetIOHandler(pCarrier1);

		pCarrier2->SetProtocol(pRTCP->GetFarEndpoint());
		pRTCP->GetFarEndpoint()->SetIOHandler(pCarrier2);

		return pCarrier1->StartAccept() | pCarrier2->StartAccept();
	}

	if (pCarrier1 != NULL) {
		delete pCarrier1;
		pCarrier1 = NULL;
	}
	if (pCarrier2 != NULL) {
		delete pCarrier2;
		pCarrier2 = NULL;
	}

	return false;
}
#endif /* HAS_PROTOCOL_RTP */
