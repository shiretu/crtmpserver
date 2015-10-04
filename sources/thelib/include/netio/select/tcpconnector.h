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

#ifdef NET_SELECT

#include "netio/select/iohandler.h"
#include "protocols/baseprotocol.h"
#include "protocols/protocolfactorymanager.h"
#include "netio/select/iohandlermanager.h"
#include "netio/select/tcpcarrier.h"

template<class T>
class DLLEXP TCPConnector
: public IOHandler {
private:
	string _ip;
	uint16_t _port;
	vector<uint64_t> _protocolChain;
	bool _closeSocket;
	Variant _customParameters;
	bool _success;
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
	}

	virtual ~TCPConnector() {
		if (!_success) {
			T::SignalProtocolCreated(NULL, _customParameters);
		}
		if (_closeSocket) {
			SOCKET_CLOSE(_inboundFd);
		}
	}

	virtual bool SignalOutputData() {
		ASSERT("Operation not supported");
		return false;
	}

	virtual bool OnEvent(select_event &event) {
		IOHandlerManager::EnqueueForDelete(this);
		WARN("THIS IS NOT COMPLETELY IMPLEMENTED");

		BaseProtocol *pProtocol = ProtocolFactoryManager::CreateProtocolChain(
				_protocolChain, _customParameters);

		if (pProtocol == NULL) {
			FATAL("Unable to create protocol chain");
			_closeSocket = true;
			return false;
		}

		TCPCarrier *pTCPCarrier = new TCPCarrier(_inboundFd);
		pTCPCarrier->SetProtocol(pProtocol->GetFarEndpoint());
		pProtocol->GetFarEndpoint()->SetIOHandler(pTCPCarrier);
		if (_customParameters.HasKeyChain(V_STRING, false, 1, "witnessFile"))
			pProtocol->GetNearEndpoint()->SetWitnessFile((string) _customParameters.GetValue("witnessFile", false));
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

		if (!T::SignalProtocolCreated(pProtocol, _customParameters)) {
			FATAL("Unable to signal protocol created");
			delete pProtocol;
			_closeSocket = true;
			return false;
		}
		_success = true;

		INFO("Outbound connection established: %s", STR(*pProtocol));

		_closeSocket = false;
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

		return true;
	}

	bool Connect() {
		sockaddr_in address;

		address.sin_family = PF_INET;
		address.sin_addr.s_addr = inet_addr(STR(_ip));
		if (address.sin_addr.s_addr == INADDR_NONE) {
			FATAL("Unable to translate string %s to a valid IP address", STR(_ip));
			return 0;
		}
		address.sin_port = EHTONS(_port); //----MARKED-SHORT----

		if (!IOHandlerManager::EnableWriteData(this)) {
			FATAL("Unable to enable reading data");
			return false;
		}

		if (connect(_inboundFd, (sockaddr *) & address, sizeof (address)) != 0) {
			int err = SOCKET_LAST_ERROR;
			if (err != SOCKET_ERROR_EINPROGRESS) {
				FATAL("Unable to connect to %s:%hu %d", STR(_ip), _port, err);
				_closeSocket = true;
				return false;
			}
		}

		_closeSocket = false;
		return true;
	}

	virtual void GetStats(Variant &info, uint32_t namespaceId = 0) {

	}
};

#endif /* NET_SELECT */
