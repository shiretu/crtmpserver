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


#include "protocols/timer/basetimerprotocol.h"
#include "netio/netio.h"

BaseTimerProtocol::BaseTimerProtocol()
: BaseProtocol(PT_TIMER) {
	_pTimer = new IOTimer();
	_pTimer->SetProtocol(this);
	_milliseconds = 0;
}

BaseTimerProtocol::~BaseTimerProtocol() {
	if (_pTimer != NULL) {
		IOTimer *pTimer = _pTimer;
		_pTimer = NULL;
		pTimer->SetProtocol(NULL);
		delete pTimer;
	}
}

string BaseTimerProtocol::GetName() {
	return _name;
}

uint32_t BaseTimerProtocol::GetTimerPeriodInMilliseconds() {
	return _milliseconds;
}

double BaseTimerProtocol::GetTimerPeriodInSeconds() {
	return (double) _milliseconds / 1000.0;
}

IOHandler *BaseTimerProtocol::GetIOHandler() {
	return _pTimer;
}

void BaseTimerProtocol::SetIOHandler(IOHandler *pIOHandler) {
	if (pIOHandler != NULL) {
		if (pIOHandler->GetType() != IOHT_TIMER) {
			ASSERT("This protocol accepts only Timer carriers");
		}
	}
	_pTimer = (IOTimer *) pIOHandler;
}

bool BaseTimerProtocol::EnqueueForTimeEvent(uint32_t seconds) {
	if (_pTimer == NULL) {
		ASSERT("BaseTimerProtocol has no timer");
		return false;
	}
	_milliseconds = seconds * 1000;
	return _pTimer->EnqueueForTimeEvent(seconds);
}

bool BaseTimerProtocol::EnqueueForHighGranularityTimeEvent(uint32_t milliseconds) {
	if (_pTimer == NULL) {
		ASSERT("BaseTimerProtocol has no timer");
		return false;
	}
	_milliseconds = milliseconds;
	return _pTimer->EnqueueForHighGranularityTimeEvent(milliseconds);
}

bool BaseTimerProtocol::AllowFarProtocol(uint64_t type) {
	ASSERT("Operation not supported");
	return false;
}

bool BaseTimerProtocol::AllowNearProtocol(uint64_t type) {
	ASSERT("Operation not supported");
	return false;
}

bool BaseTimerProtocol::SignalInputData(int32_t recvAmount) {
	ASSERT("OPERATION NOT SUPPORTED");
	return false;
}

bool BaseTimerProtocol::SignalInputData(IOBuffer &buffer) {
	ASSERT("OPERATION NOT SUPPORTED");
	return false;
}


