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
#include "netio/iocp/tcpacceptor.h"
#include "netio/iocp/iohandlermanager.h"
#include "protocols/protocolfactorymanager.h"
#include "protocols/tcpprotocol.h"
#include "netio/iocp/tcpcarrier.h"
#include "application/baseclientapplication.h"
#include <Mswsock.h>

TCPAcceptor::TCPAcceptor(string ipAddress, uint16_t port, Variant parameters,
		vector<uint64_t>/*&*/ protocolChain)
: IOHandler(0, 0, IOHT_ACCEPTOR) {
	_pEvent = iocp_event_accept::GetInstance(this);
	_pApplication = NULL;
	memset(&_address, 0, sizeof (sockaddr_in));

	_address.sin_family = PF_INET;
	_address.sin_addr.s_addr = inet_addr(ipAddress.c_str());
	o_assert(_address.sin_addr.s_addr != INADDR_NONE);
	_address.sin_port = EHTONS(port); //----MARKED-SHORT----

	_protocolChain = protocolChain;
	_parameters = parameters;
	_enabled = false;
	_acceptedCount = 0;
	_droppedCount = 0;
	_ipAddress = ipAddress;
	_port = port;
}

TCPAcceptor::~TCPAcceptor() {
	SOCKET_CLOSE(_inboundFd);
	_pEvent->listenSock = SOCKET_INVALID;
	_pEvent->Release();
	_pEvent = NULL;
}

void TCPAcceptor::CancelIO() {
	CancelIoEx((HANDLE) _inboundFd, _pEvent);
}

iocp_event_accept *TCPAcceptor::GetEvent() {
	return _pEvent;
}

bool TCPAcceptor::Bind() {
	//_pEvent->listenSock=_inboundFd = _outboundFd = (int) WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
	_pEvent->listenSock = _inboundFd = _outboundFd = socket(AF_INET, SOCK_STREAM, 0); //NOINHERIT
	if (SOCKET_IS_INVALID(_inboundFd)) {
		int err = SOCKET_LAST_ERROR;
		FATAL("Unable to create socket: %d", err);
		return false;
	}

	if (!setFdOptions(_inboundFd, false)) {
		FATAL("Unable to set socket options");
		return false;
	}

	if (bind(_inboundFd, (sockaddr *) & _address, sizeof (sockaddr)) != 0) {
		int err = SOCKET_LAST_ERROR;
		FATAL("Unable to bind on address: tcp://%s:%hu; Error was: %d",
				inet_ntoa(((sockaddr_in *) & _address)->sin_addr),
				ENTOHS(((sockaddr_in *) & _address)->sin_port),
				err);
		return false;
	}

	if (_port == 0) {
		socklen_t tempSize = sizeof (sockaddr);
		if (getsockname(_inboundFd, (sockaddr *) & _address, &tempSize) != 0) {
			FATAL("Unable to extract the random port");
			return false;
		}
		_parameters[CONF_PORT] = (uint16_t) ENTOHS(_address.sin_port);
	}

	if (listen(_inboundFd, 100) != 0) {
		FATAL("Unable to put the socket in listening mode");
		return false;
	}

	_enabled = true;
	return true;
}

void TCPAcceptor::SetApplication(BaseClientApplication *pApplication) {
	o_assert(_pApplication == NULL);
	_pApplication = pApplication;
}

bool TCPAcceptor::StartAccept() {
	if (!IOHandlerManager::RegisterIOCPFD(_inboundFd)) {
		FATAL("Unable to register IOCP Fd");
		return false;
	}
	return IOHandlerManager::EnableAcceptConnections(this);
}

bool TCPAcceptor::SignalOutputData() {
	ASSERT("Operation not supported");
	return false;
}

bool TCPAcceptor::OnEvent(iocp_event &event) {
	return OnConnectionAvailable(event);
}

bool TCPAcceptor::OnConnectionAvailable(iocp_event &event) {
	if (_pApplication == NULL)
		return Accept();
	return _pApplication->AcceptTCPConnection(this);
}

bool TCPAcceptor::Accept() {
	DWORD temp = 0;
	BOOL result = GetOverlappedResult((HANDLE) _inboundFd, _pEvent, &temp, true);
	if (!result) {
		DWORD err = GetLastError();
		FATAL("Acceptor failed. Error code was: %"PRIu32, err);
		return false;
	}

	sockaddr *pLocalAddress = NULL;
	sockaddr *pRemoteAddress = NULL;
	int dummy = 0;
	GetAcceptExSockaddrs(
			_pEvent->pBuffer, // lpOutputBuffer,
			0, //dwReceiveDataLength,
			sizeof (sockaddr_in) + 16, //dwLocalAddressLength,
			sizeof (sockaddr_in) + 16, //dwRemoteAddressLength,
			&pLocalAddress, // LocalSockaddr,
			&dummy, //LocalSockaddrLength,
			&pRemoteAddress, //RemoteSockaddr,
			&dummy// RemoteSockaddrLength
			);
	//FINEST("HANDLE: %"PRIz"u", sizeof (HANDLE));
	//FINEST("SOCKET: %"PRIz"u", sizeof (SOCKET));
	//FINEST("int: %"PRIz"u", sizeof (int));
	//FINEST("_pEvent->listenSock: %"PRIz"u", sizeof (_pEvent->listenSock));
	//int one=_pEvent->listenSock;
	/*string str=format("sizeof(one): %"PRIz"u; sizeof(HANDLE): %"PRIz"u; one: %d; ",sizeof(one),sizeof(HANDLE),one);
	for(uint32_t i=0;i<sizeof(int);i++){
		str+=format("%02"PRIx8,((uint8_t *)&one)[sizeof(int)-1-i]);
	}
	FINEST("%s",STR(str));*/
	if (setsockopt(_pEvent->acceptedSock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char *) &_pEvent->listenSock, sizeof (_pEvent->listenSock)) != 0) {
		SOCKET_CLOSE(_pEvent->acceptedSock);
		_droppedCount++;
		DWORD err = WSAGetLastError();
		FATAL("Unable to set SO_UPDATE_ACCEPT_CONTEXT. Client dropped: %s:%"PRIu16" -> %s:%"PRIu16"; error: %"PRIu32,
				inet_ntoa(((sockaddr_in *) pLocalAddress)->sin_addr),
				ENTOHS(((sockaddr_in *) pLocalAddress)->sin_port),
				STR(_ipAddress),
				_port,
				err);
		return false;
	}

	if (!_enabled) {
		SOCKET_CLOSE(_pEvent->acceptedSock);
		_droppedCount++;
		WARN("Acceptor is not enabled. Client dropped: %s:%"PRIu16" -> %s:%"PRIu16,
				inet_ntoa(((sockaddr_in *) pLocalAddress)->sin_addr),
				ENTOHS(((sockaddr_in *) pLocalAddress)->sin_port),
				STR(_ipAddress),
				_port);
		return true;
	}

	if (!setFdOptions(_pEvent->acceptedSock, false)) {
		FATAL("Unable to set socket options");
		SOCKET_CLOSE(_pEvent->acceptedSock);
		return false;
	}

	//4. Create the chain
	BaseProtocol *pProtocol = ProtocolFactoryManager::CreateProtocolChain(
			_protocolChain, _parameters);
	if (pProtocol == NULL) {
		FATAL("Unable to create protocol chain");
		SOCKET_CLOSE(_pEvent->acceptedSock);
		return false;
	}

	//5. Create the carrier and bind it
	TCPCarrier *pTCPCarrier = new TCPCarrier(_pEvent->acceptedSock, true);
	pTCPCarrier->SetProtocol(pProtocol->GetFarEndpoint());
	pProtocol->GetFarEndpoint()->SetIOHandler(pTCPCarrier);
	if (_parameters.HasKeyChain(V_STRING, false, 1, "witnessFile"))
		pProtocol->GetNearEndpoint()->SetWitnessFile((string) _parameters.GetValue("witnessFile", false));

	//6. Register the protocol stack with an application
	if (_pApplication != NULL) {
		pProtocol = pProtocol->GetNearEndpoint();
		pProtocol->SetApplication(_pApplication);

		//		EventLogger *pEvtLog = _pApplication->GetEventLogger();
		//		if (pEvtLog != NULL) {
		//			pEvtLog->LogInboundConnectionStart(_ipAddress, _port, STR(*(pProtocol->GetFarEndpoint())));
		//			pTCPCarrier->SetEventLogger(pEvtLog);
		//		}
	}

	if (pProtocol->GetNearEndpoint()->GetOutputBuffer() != NULL)
		pProtocol->GetNearEndpoint()->EnqueueForOutbound();

	_acceptedCount++;

	INFO("Inbound connection accepted: %s", STR(*(pProtocol->GetNearEndpoint())));

	//7. Done
	return IOHandlerManager::EnableAcceptConnections(this);
}

bool TCPAcceptor::Drop() {
	NYIR;
}

Variant & TCPAcceptor::GetParameters() {
	return _parameters;
}

BaseClientApplication *TCPAcceptor::GetApplication() {
	return _pApplication;
}

vector<uint64_t> &TCPAcceptor::GetProtocolChain() {
	return _protocolChain;
}

void TCPAcceptor::GetStats(Variant &info, uint32_t namespaceId) {
	info = _parameters;
	info["id"] = (((uint64_t) namespaceId) << 32) | GetId();
	info["enabled"] = (bool)_enabled;
	info["acceptedConnectionsCount"] = _acceptedCount;
	info["droppedConnectionsCount"] = _droppedCount;
	if (_pApplication != NULL) {
		info["appId"] = (((uint64_t) namespaceId) << 32) | _pApplication->GetId();
		info["appName"] = _pApplication->GetName();
	} else {
		info["appId"] = (((uint64_t) namespaceId) << 32);
		info["appName"] = "";
	}
}

bool TCPAcceptor::Enable() {
	return _enabled;
}

void TCPAcceptor::Enable(bool enabled) {
	_enabled = enabled;
}

#endif /* NET_IOCP */
