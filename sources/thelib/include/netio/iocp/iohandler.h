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

#include "common.h"
#include "netio/iohandlertype.h"
#include "netio/iocp/iocpevent.h"

class BaseProtocol;
//class EventLogger;

class DLLEXP IOHandler {
protected:
	static uint32_t _idGenerator;
	uint32_t _id;
	SOCKET_TYPE _inboundFd;
	SOCKET_TYPE _outboundFd;
	BaseProtocol *_pProtocol;
	IOHandlerType _type;
//	EventLogger *_pEvtLog;
public:
	IOHandler(SOCKET_TYPE inboundFd, SOCKET_TYPE outboundFd, IOHandlerType type);
	virtual ~IOHandler();
	uint32_t GetId();
	SOCKET_TYPE GetInboundFd();
	SOCKET_TYPE GetOutboundFd();
	IOHandlerType GetType();
	void SetProtocol(BaseProtocol *pPotocol);
	BaseProtocol *GetProtocol();
//	void SetEventLogger(EventLogger *pEvtLogger);
	virtual bool SignalOutputData() = 0;
	virtual bool OnEvent(iocp_event &event) = 0;
	static string IOHTToString(IOHandlerType type);
	virtual void GetStats(Variant &info, uint32_t namespaceId = 0) = 0;
	virtual void CancelIO() = 0;
};

#endif /* NET_IOCP */
