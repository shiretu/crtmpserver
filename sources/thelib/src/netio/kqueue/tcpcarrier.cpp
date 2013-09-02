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


#ifdef NET_KQUEUE
#include "netio/kqueue/tcpcarrier.h"
#include "netio/kqueue/iohandlermanager.h"
#include "protocols/baseprotocol.h"

#define ENABLE_WRITE_DATA \
if (!_writeDataEnabled) { \
    _writeDataEnabled = true; \
    IOHandlerManager::EnableWriteData(this); \
} \
_enableWriteDataCalled=true;

#define DISABLE_WRITE_DATA \
if (_writeDataEnabled) { \
	_enableWriteDataCalled=false; \
	_pProtocol->ReadyForSend(); \
	if(!_enableWriteDataCalled) { \
		if(_pProtocol->GetOutputBuffer()==NULL) {\
			_writeDataEnabled = false; \
			IOHandlerManager::DisableWriteData(this); \
		} \
	} \
}

TCPCarrier::TCPCarrier(int32_t fd)
: IOHandler(fd, fd, IOHT_TCP_CARRIER) {
	IOHandlerManager::EnableReadData(this);
	_writeDataEnabled = false;
	_enableWriteDataCalled = false;
	memset(&_farAddress, 0, sizeof (sockaddr_in));
	_farIp = "";
	_farPort = 0;
	memset(&_nearAddress, 0, sizeof (sockaddr_in));
	_nearIp = "";
	_nearPort = 0;
	GetEndpointsInfo();
	_rx = 0;
	_tx = 0;
	_ioAmount = 0;
	_lastRecvError = 0;
	_lastSendError = 0;

	Variant stats;
	GetStats(stats);
}

TCPCarrier::~TCPCarrier() {
	Variant stats;
	GetStats(stats);
	CLOSE_SOCKET(_inboundFd);
}

bool TCPCarrier::OnEvent(struct kevent &event) {
	//3. Do the I/O
	switch (event.filter) {
		case EVFILT_READ:
		{
			IOBuffer *pInputBuffer = _pProtocol->GetInputBuffer();
			o_assert(pInputBuffer != NULL);
			if (!pInputBuffer->ReadFromTCPFd(event.ident, event.data, _ioAmount,
					_lastRecvError)) {
				FATAL("Unable to read data from connection: %s. Error was (%d): %s",
						(_pProtocol != NULL) ? STR(*_pProtocol) : "", _lastRecvError,
						strerror(_lastRecvError));
				return false;
			}
			_rx += _ioAmount;
			ADD_IN_BYTES_MANAGED(_type, _ioAmount);
			if (!_pProtocol->SignalInputData(_ioAmount)) {
				FATAL("Unable to read data from connection: %s. Signaling upper protocols failed",
						(_pProtocol != NULL) ? STR(*_pProtocol) : "");
				return false;
			}
			return true;
		}
		case EVFILT_WRITE:
		{
			IOBuffer *pOutputBuffer = NULL;
			if ((pOutputBuffer = _pProtocol->GetOutputBuffer()) != NULL) {
				if (!pOutputBuffer->WriteToTCPFd(event.ident, event.data,
						_ioAmount, _lastSendError)) {
					FATAL("Unable to write data on connection: %s. Error was (%d): %s",
							(_pProtocol != NULL) ? STR(*_pProtocol) : "", _lastSendError,
							strerror(_lastSendError));
					IOHandlerManager::EnqueueForDelete(this);
					return false;
				}
				_tx += _ioAmount;
				ADD_OUT_BYTES_MANAGED(_type, _ioAmount);
				if (GETAVAILABLEBYTESCOUNT(*pOutputBuffer) == 0) {
					DISABLE_WRITE_DATA;
				}
			} else {
				DISABLE_WRITE_DATA;
			}
			return true;
		}
		default:
		{
			FATAL("Unable to read/write data from/to connection: %s. Invalid event type: %d",
					(_pProtocol != NULL) ? STR(*_pProtocol) : "", event.filter);
			return false;
		}
	}
}

bool TCPCarrier::SignalOutputData() {
	ENABLE_WRITE_DATA;
	return true;
}

void TCPCarrier::GetStats(Variant &info, uint32_t namespaceId) {
	if (!GetEndpointsInfo()) {
		FATAL("Unable to get endpoints info");
		info = "unable to get endpoints info";
		return;
	}
	info["type"] = "IOHT_TCP_CARRIER";
	info["farIP"] = _farIp;
	info["farPort"] = _farPort;
	info["nearIP"] = _nearIp;
	info["nearPort"] = _nearPort;
	info["rx"] = _rx;
	info["tx"] = _tx;
}

sockaddr_in &TCPCarrier::GetFarEndpointAddress() {
	if ((_farIp == "") || (_farPort == 0))
		GetEndpointsInfo();
	return _farAddress;
}

string TCPCarrier::GetFarEndpointAddressIp() {
	if (_farIp != "")
		return _farIp;
	GetEndpointsInfo();
	return _farIp;
}

uint16_t TCPCarrier::GetFarEndpointPort() {
	if (_farPort != 0)
		return _farPort;
	GetEndpointsInfo();
	return _farPort;
}

sockaddr_in &TCPCarrier::GetNearEndpointAddress() {
	if ((_nearIp == "") || (_nearPort == 0))
		GetEndpointsInfo();
	return _nearAddress;
}

string TCPCarrier::GetNearEndpointAddressIp() {
	if (_nearIp != "")
		return _nearIp;
	GetEndpointsInfo();
	return _nearIp;
}

uint16_t TCPCarrier::GetNearEndpointPort() {
	if (_nearPort != 0)
		return _nearPort;
	GetEndpointsInfo();
	return _nearPort;
}

bool TCPCarrier::GetEndpointsInfo() {
	if ((_farIp != "")
			&& (_farPort != 0)
			&& (_nearIp != "")
			&& (_nearPort != 0)) {
		return true;
	}
	socklen_t len = sizeof (sockaddr);
	if (getpeername(_inboundFd, (sockaddr *) & _farAddress, &len) != 0) {
		FATAL("Unable to get peer's address");
		return false;
	}
	_farIp = format("%s", inet_ntoa(((sockaddr_in *) & _farAddress)->sin_addr));
	_farPort = ENTOHS(((sockaddr_in *) & _farAddress)->sin_port); //----MARKED-SHORT----

	if (getsockname(_inboundFd, (sockaddr *) & _nearAddress, &len) != 0) {
		FATAL("Unable to get peer's address");
		return false;
	}
	_nearIp = format("%s", inet_ntoa(((sockaddr_in *) & _nearAddress)->sin_addr));
	_nearPort = ENTOHS(((sockaddr_in *) & _nearAddress)->sin_port); //----MARKED-SHORT----
	return true;
}

#endif /* NET_KQUEUE */

