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
#include "netio/iocp/iotimer.h"
#include "protocols/baseprotocol.h"
#include "netio/iocp/iohandlermanager.h"

int32_t IOTimer::_idGenerator;

IOTimer::IOTimer()
: IOHandler(0, 0, IOHT_TIMER) {
	_outboundFd = _inboundFd = ++_idGenerator;
#ifdef HAS_IOCP_TIMER
	_pTimerEvent = iocp_event_timer_elapsed::GetInstance(this);
	_pTimer = CreateThreadpoolTimer(TimerCallback, (PVOID) _pTimerEvent, NULL);
	if (_pTimer == NULL) {
		FATAL("Unable to create timer. Err: %"PRIu32, GetLastError());
	}
#ifdef TIMER_PERFORMANCE_TEST
	QueryPerformanceCounter(&startCounter);
	_tick = 0;
	WARN("Timer started.");
#endif /* TIMER_PERFORMANCE_TEST*/
#endif /* HAS_IOCP_TIMER */
}

IOTimer::~IOTimer() {
	IOHandlerManager::DisableTimer(this, true);
#ifdef HAS_IOCP_TIMER
	if (_pTimer != NULL)
		CloseThreadpoolTimer(_pTimer);
	_pTimerEvent->Release();
#endif /* HAS_IOCP_TIMER */
}

void IOTimer::CancelIO() {
}

bool IOTimer::SignalOutputData() {
	ASSERT("Operation not supported");
	return false;
}

bool IOTimer::OnEvent(iocp_event &event) {
	if (!_pProtocol->IsEnqueueForDelete()) {
		if (!_pProtocol->TimePeriodElapsed()) {
			FATAL("Unable to handle TimeElapsed event");
			IOHandlerManager::EnqueueForDelete(this);
			return false;
		}
	}
#ifdef TIMER_PERFORMANCE_TEST
	LARGE_INTEGER testMark;
	LARGE_INTEGER freq;
	LARGE_INTEGER elapsed;
	QueryPerformanceCounter(&testMark);
	QueryPerformanceFrequency(&freq);
	elapsed.QuadPart = ((testMark.QuadPart - startCounter.QuadPart) * 1000000) / freq.QuadPart;
	WARN("Timer %s tick #%"PRIu32" at %"PRIu32" microsec", STR(*_pProtocol), ++_tick, elapsed.QuadPart);
#endif /* TIMER_PERFORMANCE_TEST*/
	return true;
}

bool IOTimer::EnqueueForTimeEvent(uint32_t seconds) {
	return IOHandlerManager::EnableTimer(this, seconds);
}

bool IOTimer::EnqueueForHighGranularityTimeEvent(uint32_t milliseconds) {
	return IOHandlerManager::EnableHighGranularityTimer(this, milliseconds);
}

void IOTimer::GetStats(Variant &info, uint32_t namespaceId) {
	info["id"] = (((uint64_t) namespaceId) << 32) | GetId();
}
#ifdef HAS_IOCP_TIMER

iocp_event_timer_elapsed * IOTimer::GetTimerEvent() {
	return _pTimerEvent;
}

PTP_TIMER IOTimer::GetPTTimer() {
	return _pTimer;
}

void CALLBACK TimerCallback(PTP_CALLBACK_INSTANCE Instance,
		PVOID Context, PTP_TIMER Timer) {
	iocp_event_timer_elapsed * pEvent = (iocp_event_timer_elapsed *) Context;
	IOHandlerManager::PostCompletedEvent(pEvent);
}
#endif /* HAS_IOCP_TIMER */
#endif /* NET_IOCP */
