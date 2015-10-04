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

#include "netio/iocp/iohandler.h"

class BaseClientApplication;

class DLLEXP TCPAcceptor
: public IOHandler {
private:
	iocp_event_accept *_pEvent;
	sockaddr_in _address;
	vector<uint64_t> _protocolChain;
	BaseClientApplication *_pApplication;
	Variant _parameters;
	bool _enabled;
	uint32_t _acceptedCount;
	uint32_t _droppedCount;
	string _ipAddress;
	uint16_t _port;
public:
	TCPAcceptor(string ipAddress, uint16_t port, Variant parameters,
			vector<uint64_t>/*&*/ protocolChain);
	virtual ~TCPAcceptor();
	virtual void CancelIO();
	iocp_event_accept *GetEvent();
	bool Bind();
	void SetApplication(BaseClientApplication *pApplication);
	bool StartAccept();
	virtual bool SignalOutputData();
	virtual bool OnEvent(iocp_event &event);
	virtual bool OnConnectionAvailable(iocp_event &event);
	bool Accept();
	bool Drop();
	Variant & GetParameters();
	BaseClientApplication *GetApplication();
	vector<uint64_t> &GetProtocolChain();
	virtual void GetStats(Variant &info, uint32_t namespaceId = 0);
	bool Enable();
	void Enable(bool enabled);
};

#endif /* NET_IOCP */
