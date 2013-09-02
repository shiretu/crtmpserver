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

#include "utils/udpsenderprotocol.h"
#include "netio/netio.h"

UDPSenderProtocol::UDPSenderProtocol()
: UDPProtocol() {
	memset(&_destAddress, 0, sizeof (_destAddress));
	_fd = (SOCKET) - 1;
	_pReadyForSend = NULL;
	_bindIp = "";
	_bindPort = 0;
}

UDPSenderProtocol::~UDPSenderProtocol() {
}

void UDPSenderProtocol::SetIOHandler(IOHandler *pCarrier) {
	UDPProtocol::SetIOHandler(pCarrier);
	if (pCarrier != NULL) {
		_fd = (SOCKET) pCarrier->GetOutboundFd();
	} else {
		_fd = (SOCKET) - 1;
	}
}

void UDPSenderProtocol::ReadyForSend() {
	if (_pReadyForSend != NULL)
		_pReadyForSend->ReadyForSend();
}

UDPSenderProtocol *UDPSenderProtocol::GetInstance(string bindIp, uint16_t bindPort,
		string targetIp, uint16_t targetPort, uint16_t ttl, uint16_t tos,
		ReadyForSendInterface *pReadyForSend
		) {
	//1. Create the protocol first
	UDPSenderProtocol *pResult = new UDPSenderProtocol();

	//2. Create the UDPCarrier
	UDPCarrier *pCarrier = UDPCarrier::Create(bindIp, bindPort, pResult, ttl,
			tos, "");
	if (pCarrier == NULL) {
		FATAL("Unable to create carrier");
		pResult->EnqueueForDelete();
		return NULL;
	}
	pResult->_bindIp = pCarrier->GetNearEndpointAddress();
	pResult->_bindPort = pCarrier->GetNearEndpointPort();

	//3. compute the send address
	sockaddr_in &_destAddress = pResult->_destAddress;
	memset(&_destAddress, 0, sizeof (_destAddress));
	_destAddress.sin_family = PF_INET;
	_destAddress.sin_addr.s_addr = inet_addr(STR(targetIp));
	_destAddress.sin_port = EHTONS(targetPort);
	if (_destAddress.sin_addr.s_addr == INADDR_NONE) {
		FATAL("Unable to compute destination address %s:%"PRIu16, STR(targetIp),
				targetPort);
		pResult->EnqueueForDelete();
		return NULL;
	}

	//4. Apply TOS
	if (tos <= 255) {
		if (!setFdTOS(pCarrier->GetOutboundFd(), (uint8_t) tos)) {
			FATAL("Unable to set tos");
			pResult->EnqueueForDelete();
			return NULL;
		}
	}

	//5. Apply TTL and also switch on broadcast mode if necessary
	uint32_t testVal = EHTONL(_destAddress.sin_addr.s_addr);
	if (ttl <= 255) {
		if ((testVal > 0xe0000000) && (testVal < 0xefffffff)) {
			int activateBroadcast = 1;
			if (setsockopt(pCarrier->GetOutboundFd(), SOL_SOCKET, SO_BROADCAST,
					(char *) &activateBroadcast, sizeof (activateBroadcast)) != 0) {
				int err = LASTSOCKETERROR;
				FATAL("Unable to activate SO_BROADCAST on the socket: %d", err);
				pResult->EnqueueForDelete();
				return NULL;
			}
			if (!setFdMulticastTTL(pCarrier->GetOutboundFd(), (uint8_t) ttl)) {
				FATAL("Unable to set ttl");
				pResult->EnqueueForDelete();
				return NULL;
			}
		} else {
			if (!setFdTTL(pCarrier->GetOutboundFd(), (uint8_t) ttl)) {
				FATAL("Unable to set ttl");
				pResult->EnqueueForDelete();
				return NULL;
			}
		}
	}

	//6. Setup the readyForSend interface and enable the protocol for write events
	pResult->_pReadyForSend = pReadyForSend;

	//7. Done
	return pResult;
}

void UDPSenderProtocol::ResetReadyForSendInterface() {
	_pReadyForSend = NULL;
}

uint16_t UDPSenderProtocol::GetUdpBindPort() {
	return _bindPort;
}

bool UDPSenderProtocol::SendChunked(uint8_t *pData, uint32_t dataLength,
		uint32_t maxChunkSize) {
	uint32_t totalSent = 0;
	while (totalSent != dataLength) {
		uint32_t chunkSize = dataLength - totalSent;
		chunkSize = chunkSize > maxChunkSize ? maxChunkSize : chunkSize;
		if ((uint32_t) sendto(_fd, (char *) (pData + totalSent), chunkSize, MSG_NOSIGNAL,
				(sockaddr *) & _destAddress, sizeof (_destAddress)) != chunkSize) {
			int err = LASTSOCKETERROR;
			if (err != SOCKERROR_ENOBUFS) {
				FATAL("Unable to send bytes over UDP: (%d) %s", err, strerror(err));
				return false;
			}
		}
		totalSent += chunkSize;
		ADD_OUT_BYTES_RAWUDP(chunkSize);
	}
	return true;
}

bool UDPSenderProtocol::SendBlock(uint8_t *pData, uint32_t dataLength) {
	if ((uint32_t) sendto(_fd, (char *) pData, dataLength, MSG_NOSIGNAL,
			(sockaddr *) & _destAddress, sizeof (_destAddress)) != dataLength) {
		int err = errno;
		FATAL("Unable to send bytes over UDP: (%d) %s", err, strerror(err));
		return false;
	}
	ADD_OUT_BYTES_RAWUDP(dataLength);
	return true;
}
