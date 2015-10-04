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


#ifdef NET_IOCP

#include "netio/iocp/udpcarrier.h"
#include "netio/iocp/iohandlermanager.h"
#include "protocols/baseprotocol.h"
#include "eventlogger/eventlogger.h"

UDPCarrier::UDPCarrier(SOCKET_TYPE fd)
: IOHandler(fd, fd, IOHT_UDP_CARRIER) {
	_nearIp = "";
	_nearPort = 0;
	_rx = 0;
	_tx = 0;
	_ioAmount = 0;
	_pReadEvent = iocp_event_udp_read::GetInstance(this);
	IOHandlerManager::RegisterIOCPFD(GetInboundFd());

	Variant stats;
	GetStats(stats);
	EventLogger::GetDefaultLogger()->LogCarrierCreated(stats);
}

UDPCarrier::~UDPCarrier() {
	Variant stats;
	GetStats(stats);
	EventLogger::GetDefaultLogger()->LogCarrierClosed(stats);
	SOCKET_CLOSE(_inboundFd);
	_pReadEvent->Release();
	_pReadEvent = NULL;
}

void UDPCarrier::CancelIO() {
	CancelIoEx((HANDLE) _inboundFd, _pReadEvent);
}

bool UDPCarrier::OnEvent(iocp_event &event) {
	//FINEST("%"PRIu32,_inboundFd);
	DWORD temp = 0;
	BOOL result = GetOverlappedResult((HANDLE) _inboundFd, &event, &temp, true);
	if (!result) {
		DWORD err = GetLastError();
		FATAL("UDPCarrier failed. Error code was: %"PRIu32, err);
		return false;
	}
	switch (event.type) {
		case iocp_event_type_udp:
		{
			if (_pReadEvent->transferedBytes == 0) {
				FATAL("EOF encountered");
				return false;
			}
			IOBuffer *pInputBuffer = _pProtocol->GetInputBuffer();
			o_assert(pInputBuffer != NULL);
			if (!pInputBuffer->ReadFromBuffer((uint8_t *) _pReadEvent->inputBuffer.buf, _pReadEvent->transferedBytes)) {
				FATAL("Unable to read data");
				return false;
			}
			_ioAmount = _pReadEvent->transferedBytes;
			_rx += _ioAmount;
			ADD_IN_BYTES_MANAGED(_type, _ioAmount);
			if (!_pProtocol->SignalInputData(_ioAmount, & _pReadEvent->sourceAddress)) {
				FATAL("Unable to signal upper protocols");
				return false;
			}

			_pReadEvent->transferedBytes = 0;
			_pReadEvent->flags = 0;
			if (WSARecvFrom(_inboundFd, &_pReadEvent->inputBuffer, 1, &_pReadEvent->transferedBytes, &_pReadEvent->flags, (sockaddr *) & _pReadEvent->sourceAddress, &_pReadEvent->sourceLen, _pReadEvent, NULL)) {

				DWORD err = WSAGetLastError();
				if (err != WSA_IO_PENDING) {
					FATAL("Unable to call WSARecvFrom. Error was: %"PRIu32, err);
					_pReadEvent->isValid = false;
					return false;
				}

			}
			_pReadEvent->pendingOperation = true;
			return true;
		}
		default:
		{
			FATAL("Invalid event type: %d", event.type);
			return false;
		}
	}

}

bool UDPCarrier::SignalOutputData() {
	NYIR;
}

void UDPCarrier::GetStats(Variant &info, uint32_t namespaceId) {
	info["id"] = (((uint64_t) namespaceId) << 32) | GetId();
	if (!GetEndpointsInfo()) {
		FATAL("Unable to get endpoints info");
		info = "unable to get endpoints info";
		return;
	}
	info["type"] = "IOHT_UDP_CARRIER";
	info["nearIP"] = _nearIp;
	info["nearPort"] = _nearPort;
	info["rx"] = _rx;
}

Variant &UDPCarrier::GetParameters() {
	return _parameters;
}

void UDPCarrier::SetParameters(Variant parameters) {
	_parameters = parameters;
}

bool UDPCarrier::StartAccept() {
	return IOHandlerManager::EnableReadData(this);
}

string UDPCarrier::GetFarEndpointAddress() {
	ASSERT("Operation not supported");
	return "";
}

uint16_t UDPCarrier::GetFarEndpointPort() {
	ASSERT("Operation not supported");
	return 0;
}

string UDPCarrier::GetNearEndpointAddress() {
	if (_nearIp != "")
		return _nearIp;
	GetEndpointsInfo();
	return _nearIp;
}

uint16_t UDPCarrier::GetNearEndpointPort() {
	if (_nearPort != 0)
		return _nearPort;
	GetEndpointsInfo();
	return _nearPort;
}

iocp_event_udp_read *UDPCarrier::GetReadEvent() {
	return _pReadEvent;
}

UDPCarrier* UDPCarrier::Create(string bindIp, uint16_t bindPort, uint16_t ttl,
		uint16_t tos, string ssmIp) {

	//TODO : to check
	//1. Create the socket
	SOCKET_TYPE sock = socket(AF_INET, SOCK_DGRAM, 0); //NOINHERIT
	if (SOCKET_IS_INVALID(sock)) {
		int err = SOCKET_LAST_ERROR;
		FATAL("Unable to create socket: %d", err);
		return NULL;
	}

	//2. fd options
	if (!setFdOptions(sock, true)) {
		FATAL("Unable to set fd options");
		SOCKET_CLOSE(sock);
		return NULL;
	}

	if (tos <= 255) {
		if (!setFdTOS(sock, (uint8_t) tos)) {
			FATAL("Unable to set tos");
			SOCKET_CLOSE(sock);
			return NULL;
		}
	}

	ULONG testVal1 = EHTONL(inet_addr(bindIp.c_str()));
	bool isMulticast = ((testVal1 > 0xe0000000) && (testVal1 < 0xefffffff));


	//3. Bind
	if (bindIp == "") {
		bindIp = "0.0.0.0";
		bindPort = 0;
	}

	sockaddr_in bindAddress;
	memset(&bindAddress, 0, sizeof (bindAddress));
	bindAddress.sin_family = PF_INET;
	if (isMulticast)
		bindAddress.sin_addr.s_addr = INADDR_ANY;
	else
		bindAddress.sin_addr.s_addr = inet_addr(bindIp.c_str());
	bindAddress.sin_port = EHTONS(bindPort); //----MARKED-SHORT----
	if (bindAddress.sin_addr.s_addr == INADDR_NONE) {
		FATAL("Unable to bind on address %s:%hu", STR(bindIp), bindPort);
		SOCKET_CLOSE(sock);
		return NULL;
	}
	if (isMulticast) {
		INFO("Subscribe to multicast %s:%"PRIu16, STR(bindIp), bindPort);
		if (ttl <= 255) {
			if (!setFdMulticastTTL(sock, (uint8_t) ttl)) {
				FATAL("Unable to set ttl");
				SOCKET_CLOSE(sock);
				return NULL;
			}
		}
	} else {
		if (ttl <= 255) {
			if (!setFdTTL(sock, (uint8_t) ttl)) {
				FATAL("Unable to set ttl");
				SOCKET_CLOSE(sock);
				return NULL;
			}
		}
	}
	if (bind(sock, (sockaddr *) & bindAddress, sizeof (sockaddr)) != 0) {
		int err = SOCKET_LAST_ERROR;
		FATAL("Unable to bind on address: udp://%s:%"PRIu16"; Error was: %d",
				STR(bindIp), bindPort, err);
		SOCKET_CLOSE(sock);
		return NULL;
	}
	if (isMulticast) {
		if (!setFdJoinMulticast(sock, bindIp, bindPort, ssmIp)) {
			FATAL("Adding multicast failed");
			SOCKET_CLOSE(sock);
			return NULL;
		}
	}


	//4. Create the carrier
	UDPCarrier *pResult = new UDPCarrier(sock);
	pResult->_pReadEvent->sourceAddress = bindAddress;

	return pResult;
}

UDPCarrier* UDPCarrier::Create(string bindIp, uint16_t bindPort,
		BaseProtocol *pProtocol, uint16_t ttl, uint16_t tos, string ssmIp) {
	if (pProtocol == NULL) {
		FATAL("Protocol can't be null");
		return NULL;
	}

	UDPCarrier *pResult = Create(bindIp, bindPort, ttl, tos, ssmIp);
	if (pResult == NULL) {
		FATAL("Unable to create UDP carrier");
		return NULL;
	}

	pResult->SetProtocol(pProtocol->GetFarEndpoint());
	pProtocol->GetFarEndpoint()->SetIOHandler(pResult);

	return pResult;
}

bool UDPCarrier::Setup(Variant &settings) {
	NYIR;
}

bool UDPCarrier::GetEndpointsInfo() {
	if (getsockname(_inboundFd, (sockaddr *) & _pReadEvent->sourceAddress, &_pReadEvent->sourceLen) != 0) {
		FATAL("Unable to get peer's address");
		return false;
	}
	_nearIp = format("%s", inet_ntoa(((sockaddr_in *) & _pReadEvent->sourceAddress)->sin_addr));
	_nearPort = ENTOHS(((sockaddr_in *) & _pReadEvent->sourceAddress)->sin_port); //----MARKED-SHORT----
	return true;
}
#endif /* NET_IOCP */
