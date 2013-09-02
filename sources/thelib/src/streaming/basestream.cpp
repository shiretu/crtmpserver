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


#include "streaming/basestream.h"
#include "streaming/streamsmanager.h"
#include "protocols/baseprotocol.h"
#include "netio/netio.h"
#include "streaming/codectypes.h"
#include "application/baseclientapplication.h"

uint32_t BaseStream::_uniqueIdGenerator = 1;

BaseStream::BaseStream(BaseProtocol *pProtocol, uint64_t type, string name) {
	memset(&_stats, 0, sizeof (_stats));
	_pStreamsManager = NULL;
	_type = type;
	_uniqueId = _uniqueIdGenerator++;
	_pProtocol = pProtocol;
	_name = name;
	GETCLOCKS(_creationTimestamp, double);
	_creationTimestamp /= (double) CLOCKS_PER_SECOND;
	_creationTimestamp *= 1000.00;
	GetIpPortInfo();
	StoreConnectionType();
}

BaseStream::~BaseStream() {
	if (_pStreamsManager != NULL) {
		_pStreamsManager->UnRegisterStream(this);
		_pStreamsManager = NULL;
	}
}

bool BaseStream::SetStreamsManager(StreamsManager *pStreamsManager) {
	if (pStreamsManager == NULL) {
		FATAL("no streams manager provided for stream %s(%"PRIu32")",
				STR(tagToString(_type)), _uniqueId);
		return false;
	}
	if (_pStreamsManager != NULL) {
		FATAL("Stream %s(%"PRIu32") already registered to the streams manager",
				STR(tagToString(_type)), _uniqueId);
		return false;
	}
	_pStreamsManager = pStreamsManager;
	_pStreamsManager->RegisterStream(this);
	return true;
}

StreamsManager *BaseStream::GetStreamsManager() {
	return _pStreamsManager;
}

uint64_t BaseStream::GetType() {
	return _type;
}

uint32_t BaseStream::GetUniqueId() {
	return _uniqueId;
}

double BaseStream::GetSpawnTimestamp() {
	return _creationTimestamp;
}

string BaseStream::GetName() {
	return _name;
}

void BaseStream::SetName(string name) {
	if (_name != "") {
		ASSERT("name already set");
	}
	_name = name;
}

void BaseStream::GetStats(Variant &info, uint32_t namespaceId) {
	GetIpPortInfo();
	info["uniqueId"] = (((uint64_t) namespaceId) << 32) | _uniqueId;
	info["type"] = tagToString(_type);
	info["typeNumeric"] = _type;
	info["name"] = _name;
	info["creationTimestamp"] = _creationTimestamp;
	info["ip"] = _nearIp;
	info["port"] = _nearPort;
	info["nearIp"] = _nearIp;
	info["nearPort"] = _nearPort;
	info["farIp"] = _farIp;
	info["farPort"] = _farPort;
	double queryTimestamp = 0;
	GETCLOCKS(queryTimestamp, double);
	queryTimestamp /= (double) CLOCKS_PER_SECOND;
	queryTimestamp *= 1000.00;
	info["queryTimestamp"] = queryTimestamp;
	info["upTime"] = queryTimestamp - _creationTimestamp;
	StreamCapabilities *pCapabilities = GetCapabilities();
	if (pCapabilities != NULL) {
		info["video"]["codec"] = tagToString(pCapabilities->GetVideoCodecType());
		info["video"]["codecNumeric"] = (uint64_t) pCapabilities->GetVideoCodecType();
		info["audio"]["codec"] = tagToString(pCapabilities->GetAudioCodecType());
		info["audio"]["codecNumeric"] = (uint64_t) pCapabilities->GetAudioCodecType();
		info["bandwidth"] = pCapabilities->GetTransferRate();
	} else {
		info["video"]["codec"] = tagToString(CODEC_VIDEO_UNKNOWN);
		info["video"]["codecNumeric"] = (uint64_t) CODEC_VIDEO_UNKNOWN;
		info["audio"]["codec"] = tagToString(CODEC_AUDIO_UNKNOWN);
		info["audio"]["codecNumeric"] = (uint64_t) CODEC_AUDIO_UNKNOWN;
		info["bandwidth"] = 0;
	}

	info["audio"]["packetsCount"] = _stats.audio.packetsCount;
	info["audio"]["droppedPacketsCount"] = _stats.audio.droppedPacketsCount;
	info["audio"]["bytesCount"] = _stats.audio.bytesCount;
	info["audio"]["droppedBytesCount"] = _stats.audio.droppedBytesCount;
	info["video"]["packetsCount"] = _stats.video.packetsCount;
	info["video"]["droppedPacketsCount"] = _stats.video.droppedPacketsCount;
	info["video"]["bytesCount"] = _stats.video.bytesCount;
	info["video"]["droppedBytesCount"] = _stats.video.droppedBytesCount;
	BaseClientApplication *pApp = NULL;
	if ((_pProtocol != NULL)
			&& ((pApp = _pProtocol->GetLastKnownApplication()) != NULL)) {
		info["appName"] = pApp->GetName();
	}
	StoreConnectionType();
	if (_connectionType == V_MAP) {

		FOR_MAP(_connectionType, string, Variant, i) {
			info[MAP_KEY(i)] = MAP_VAL(i);
		}
	}
}

BaseProtocol * BaseStream::GetProtocol() {
	return _pProtocol;
}

bool BaseStream::IsEnqueueForDelete() {
	if (_pProtocol != NULL)
		return _pProtocol->IsEnqueueForDelete();
	return false;
}

void BaseStream::EnqueueForDelete() {
	if (_pProtocol != NULL) {
		_pProtocol->EnqueueForDelete();
	} else {
		delete this;
	}
}

BaseStream::operator string() {
	return format("%s(%"PRIu32") with name `%s` from protocol %s(%"PRIu32")",
			STR(tagToString(_type)),
			_uniqueId,
			STR(_name),
			_pProtocol != NULL ? STR(tagToString(_pProtocol->GetType())) : "",
			_pProtocol != NULL ? _pProtocol->GetId() : 0);
}

void BaseStream::StoreConnectionType() {
	if (_connectionType != V_NULL)
		return;
	BaseClientApplication *pApp = NULL;
	if ((_pProtocol != NULL)
			&& ((pApp = _pProtocol->GetLastKnownApplication()) != NULL))
		pApp->StoreConnectionType(_connectionType, _pProtocol);
}

void BaseStream::GetIpPortInfo() {
	if ((_nearIp != "")
			&& (_nearPort != 0)
			&& (_farIp != "")
			&& (_farPort != 0)) {
		return;
	}
	IOHandler *pIOHandler = NULL;
	if ((_pProtocol != NULL)
			&& ((pIOHandler = _pProtocol->GetIOHandler()) != NULL)
			&& (pIOHandler->GetType() == IOHT_TCP_CARRIER)) {
		_nearIp = ((TCPCarrier *) pIOHandler)->GetNearEndpointAddressIp();
		_nearPort = ((TCPCarrier *) pIOHandler)->GetNearEndpointPort();
		_farIp = ((TCPCarrier *) pIOHandler)->GetFarEndpointAddressIp();
		_farPort = ((TCPCarrier *) pIOHandler)->GetFarEndpointPort();
	} else if ((pIOHandler != NULL)
			&& (pIOHandler->GetType() == IOHT_UDP_CARRIER)) {
		_nearIp = ((UDPCarrier *) pIOHandler)->GetNearEndpointAddress();
		_nearPort = ((UDPCarrier *) pIOHandler)->GetNearEndpointPort();
		_farIp = "";
		_farPort = 0;
	} else {
		_nearIp = "";
		_nearPort = 0;
		_farIp = "";
		_farPort = 0;
	}
}
