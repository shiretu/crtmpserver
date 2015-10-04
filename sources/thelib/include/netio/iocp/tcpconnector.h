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

#pragma once

#ifdef NET_IOCP

#include "Mswsock.h"
#include "netio/iocp/iohandler.h"
#include "protocols/baseprotocol.h"
#include "protocols/protocolfactorymanager.h"
#include "netio/iocp/iohandlermanager.h"
#include "netio/iocp/tcpcarrier.h"
//#include "eventlogger/eventlogger.h"
#include <Mswsock.h>

template<class T>
class TCPConnector
	: public IOHandler {
private:
	string _ip;
	uint16_t _port;
	vector<uint64_t> _protocolChain;
	bool _closeSocket;
	Variant _customParameters;
	bool _success;
	iocp_event_connect *_pEvent;

public:

	TCPConnector(SOCKET_TYPE fd, string ip, uint16_t port,
		vector<uint64_t>& protocolChain, const Variant& customParameters)
		: IOHandler(fd, fd, IOHT_TCP_CONNECTOR) {
		_ip = ip;
		_port = port;
		_protocolChain = protocolChain;
		_closeSocket = true;
		_customParameters = customParameters;
		_success = false;
		_pEvent = iocp_event_connect::GetInstance(this);
		_pEvent->closeSocket = _closeSocket;
		IOHandlerManager::RegisterIOCPFD(GetInboundFd());
	}

	virtual ~TCPConnector() {
		if (!_success) {
			T::SignalProtocolCreated(NULL, _customParameters);
		}
		if (_closeSocket) {
			SOCKET_CLOSE(_inboundFd);
		}
		if (_pEvent != NULL) {
			//			_pEvent->closeSocket = true;
			_pEvent->Release();
			_pEvent = NULL;
		}
	}

	virtual void CancelIO() {
		CancelIoEx((HANDLE)_inboundFd, _pEvent);
	}

	virtual bool SignalOutputData() {
		ASSERT("Operation not supported");
		return false;
	}

	virtual bool OnEvent(iocp_event &event) {
		DWORD temp = 0;
		BOOL result = GetOverlappedResult((HANDLE)_inboundFd, &event, &temp, true);
		if (!result) {
			DWORD err = GetLastError();
			FATAL("TCPConnector failed. Error code was: %"PRIu32, err);
			_pEvent->closeSocket = true;
			return false;
		}

		if (setsockopt(_inboundFd, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0) != 0) {
			DWORD err = WSAGetLastError();
			WARN("Unable to set SO_UPDATE_ACCEPT_CONTEXT. Error:%"PRIu32, err);
			return true;
		}

		BaseProtocol *pProtocol = ProtocolFactoryManager::CreateProtocolChain(_protocolChain, _customParameters);

		if (pProtocol == NULL) {
			FATAL("Unable to create protocol chain");
			_closeSocket = true;
			_pEvent->closeSocket = _closeSocket;
			return false;
		}

		TCPCarrier *pTCPCarrier = new TCPCarrier(_inboundFd, false); //inbound is already registered to iocp
		pTCPCarrier->SetProtocol(pProtocol->GetFarEndpoint());
		pProtocol->GetFarEndpoint()->SetIOHandler(pTCPCarrier);
		if (_customParameters.HasKeyChain(V_STRING, false, 1, "witnessFile"))
			pProtocol->GetNearEndpoint()->SetWitnessFile((string)_customParameters.GetValue("witnessFile", false));
		else if (_customParameters.HasKeyChain(V_STRING, false, 3, "customParameters", "externalStreamConfig", "_witnessFile"))
			pProtocol->GetNearEndpoint()->SetWitnessFile((string)
			(_customParameters.GetValue("customParameters", false)
			.GetValue("externalStreamConfig", false)
			.GetValue("_witnessFile", false))
			);
		else if (_customParameters.HasKeyChain(V_STRING, false, 3, "customParameters", "localStreamConfig", "_witnessFile"))
			pProtocol->GetNearEndpoint()->SetWitnessFile((string)
			(_customParameters.GetValue("customParameters", false)
			.GetValue("localStreamConfig", false)
			.GetValue("_witnessFile", false))
			);

		IOHandlerManager::EnqueueForDelete(this);

		if (!T::SignalProtocolCreated(pProtocol, _customParameters)) {
			FATAL("Unable to signal protocol created");
			delete pProtocol;
			_closeSocket = true;
			_pEvent->closeSocket = _closeSocket;
			return false;
		}
		_success = true;

		INFO("Outbound connection established: %s", STR(*pProtocol));

		_closeSocket = false;
		_pEvent->closeSocket = _closeSocket;

		//Log Outbound Connection Event
		//		EventLogger * pEvtLog = pProtocol->GetEventLogger();
		//		if (pEvtLog != NULL) {
		//			pEvtLog->LogOutboundConnectionStart(pTCPCarrier->GetFarEndpointAddressIp(),
		//				pTCPCarrier->GetFarEndpointPort(), STR(*pProtocol));
		//			pTCPCarrier->SetEventLogger(pEvtLog);
		//		}
		return true;
	}

	static bool Connect(string ip, uint16_t port,
		vector<uint64_t>& protocolChain, Variant customParameters) {

		SOCKET_TYPE fd = socket(PF_INET, SOCK_STREAM, 0);
		if (SOCKET_IS_INVALID(fd)) {
			int err = SOCKET_LAST_ERROR;
			T::SignalProtocolCreated(NULL, customParameters);
			FATAL("Unable to create fd: %d", err);
			return 0;
		}

		if ((!setFdOptions(fd, false)) || (!setFdCloseOnExec(fd))) {
			SOCKET_CLOSE(fd);
			T::SignalProtocolCreated(NULL, customParameters);
			FATAL("Unable to set socket options");
			return false;
		}

		TCPConnector<T> *pTCPConnector = new TCPConnector(fd, ip, port,
			protocolChain, customParameters);

		if (!pTCPConnector->Connect()) {
			IOHandlerManager::EnqueueForDelete(pTCPConnector);
			FATAL("Unable to connect");
			return false;
		}
		else {
			//pTCPConnector->OnEvent(*_pEvent);
		}
		return true;
	}

	bool Connect() {
		sockaddr_in address;
		memset(&address, 0, sizeof(sockaddr_in));

		address.sin_family = PF_INET;
		address.sin_addr.s_addr = inet_addr(_ip.c_str());
		if (address.sin_addr.s_addr == INADDR_NONE) {
			FATAL("Unable to translate string %s to a valid IP address", STR(_ip));
			return 0;
		}
		address.sin_port = EHTONS(_port); //----MARKED-SHORT----

		sockaddr_in anyAddress;
		memset(&anyAddress, 0, sizeof(sockaddr_in));

		anyAddress.sin_family = PF_INET;
		anyAddress.sin_addr.s_addr = INADDR_ANY;
		anyAddress.sin_port = EHTONS(0);

		_pEvent->connectedSock = _inboundFd;
		//Bind before ConnectEx
		if (bind(_pEvent->connectedSock, (sockaddr *)& anyAddress, sizeof(sockaddr)) != 0) {
			int err = SOCKET_LAST_ERROR;
			FATAL("Unable to bind on address: %s:%hu; Error was: %d",
				inet_ntoa(((sockaddr_in *)& anyAddress)->sin_addr),
				ENTOHS(((sockaddr_in *)& anyAddress)->sin_port),
				err);
			return false;
		}

		// Load ConnectEx
		GUID GuidConnectEx = WSAID_CONNECTEX;
		LPFN_CONNECTEX lpfnConnectEx = NULL;
		DWORD dwBytes = 0;

		if (WSAIoctl(_pEvent->connectedSock,
			SIO_GET_EXTENSION_FUNCTION_POINTER,
			&GuidConnectEx,
			sizeof(GuidConnectEx),
			&lpfnConnectEx,
			sizeof(lpfnConnectEx),
			&dwBytes,
			NULL,
			NULL
			) != 0) {

			FATAL("Error WSAIoctl");
			return false;
		}

		// Try ConnectEx
		//dwBytes = 0;
		BOOL bRet = lpfnConnectEx(
			_pEvent->connectedSock, //__in      SOCKET s,
			(sockaddr *)& address, //__in      const struct sockaddr *name,
			sizeof(sockaddr), //__in      int namelen,
			NULL, // __in_opt  PVOID lpSendBuffer,
			0, //__in      DWORD dwSendDataLength,
			(LPDWORD)_pEvent->transferedBytes, //__out     LPDWORD lpdwBytesSent,
			_pEvent //__in      LPOVERLAPPED lpOverlapped
			);
		if (bRet != TRUE) {
			int err = WSAGetLastError();
			switch (err) {
			case ERROR_IO_PENDING:
			{
				_pEvent->pendingOperation = true;
				_closeSocket = false;
				_pEvent->closeSocket = _closeSocket;
				return true;
			}

			default:
			{
				_pEvent->pendingOperation = false;
				FATAL("Unable to connect to %s:%hu (%d)", STR(_ip), _port, err);
				_closeSocket = true;
				_pEvent->closeSocket = _closeSocket;
				return false;
			}
			}
		}

		_closeSocket = false;
		_pEvent->closeSocket = _closeSocket;
		return true;
	}

	virtual void GetStats(Variant &info, uint32_t namespaceId = 0) {
		//NYIA;
	}
};

#endif /* NET_IOCP */
