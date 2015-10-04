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

class IOTimer
: public IOHandler {
private:
	static int32_t _idGenerator;
#ifdef HAS_IOCP_TIMER
	iocp_event_timer_elapsed * _pTimerEvent;
	PTP_TIMER _pTimer;

#ifdef TIMER_PERFORMANCE_TEST
	LARGE_INTEGER startCounter;
	int32_t _tick;
#endif /* TIMER_PERFORMANCE_TEST */

#endif /* HAS_IOCP_TIMER */
public:
	IOTimer();
	virtual ~IOTimer();
	virtual void CancelIO();
	virtual bool SignalOutputData();
	virtual bool OnEvent(iocp_event &event);
	bool EnqueueForTimeEvent(uint32_t seconds);
	bool EnqueueForHighGranularityTimeEvent(uint32_t milliseconds);
	virtual void GetStats(Variant &info, uint32_t namespaceId = 0);
#ifdef HAS_IOCP_TIMER
	iocp_event_timer_elapsed * GetTimerEvent();
	PTP_TIMER GetPTTimer();
#endif /* HAS_IOCP_TIMER */
};
#ifdef HAS_IOCP_TIMER
void CALLBACK TimerCallback(PTP_CALLBACK_INSTANCE Instance,
		PVOID Context, PTP_TIMER Timer);
#endif /* HAS_IOCP_TIMER */

#endif /* NET_IOCP */
