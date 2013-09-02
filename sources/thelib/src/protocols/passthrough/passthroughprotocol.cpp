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

#include "protocols/passthrough/passthroughprotocol.h"
#include "application/baseclientapplication.h"

PassThroughProtocol::PassThroughProtocol()
: BaseProtocol(PT_PASSTHROUGH) {
	_pDummyStream = NULL;
}

PassThroughProtocol::~PassThroughProtocol() {
	CloseStream();
}

IOBuffer * PassThroughProtocol::GetOutputBuffer() {
	if (GETAVAILABLEBYTESCOUNT(_outputBuffer) > 0)
		return &_outputBuffer;
	return NULL;
}

void PassThroughProtocol::GetStats(Variant &info, uint32_t namespaceId) {
	BaseProtocol::GetStats(info, namespaceId);
	if (_pDummyStream != NULL) {
		Variant si;
		_pDummyStream->GetStats(si, namespaceId);
		info["streams"].PushToArray(si);
	}
}

bool PassThroughProtocol::Initialize(Variant &parameters) {
	GetCustomParameters() = parameters;
	return true;
}

void PassThroughProtocol::SetDummyStream(BaseStream *pDummyStream) {
	_pDummyStream = pDummyStream;
}

bool PassThroughProtocol::AllowFarProtocol(uint64_t type) {
	return (type == PT_TCP) || (type == PT_UDP);
}

bool PassThroughProtocol::AllowNearProtocol(uint64_t type) {
	return type == PT_INBOUND_TS;
}

bool PassThroughProtocol::SignalInputData(int32_t recvAmount) {
	NYIR;
}

bool PassThroughProtocol::SignalInputData(IOBuffer &buffer) {
	if (_pNearProtocol != NULL) {
		//deep parse
		_inputBuffer.ReadFromBuffer(GETIBPOINTER(buffer), GETAVAILABLEBYTESCOUNT(buffer));
		if (!_pNearProtocol->SignalInputData(_inputBuffer)) {
			FATAL("Unable to call TS deep parser");
			return false;
		}
	}
	return buffer.IgnoreAll();
}

bool PassThroughProtocol::SignalInputData(IOBuffer &buffer, sockaddr_in *pPeerAddress) {
	return SignalInputData(buffer);
}

bool PassThroughProtocol::SendTCPData(string &data) {
	_outputBuffer.ReadFromString(data);
	return EnqueueForOutbound();
}

void PassThroughProtocol::CloseStream() {
	if (_pDummyStream != NULL) {
		delete _pDummyStream;
		_pDummyStream = NULL;
	}
}
