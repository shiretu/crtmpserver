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


#ifndef _TIMERSMANAGER_H
#define	_TIMERSMANAGER_H

#include "platform/platform.h"

struct TimerEvent {
	uint32_t period;
	uint64_t triggerTime;
	uint32_t id;
	void *pUserData;
	operator string();
};

typedef bool (*ProcessTimerEvent)(TimerEvent &event);

class DLLEXP TimersManager {
private:
	ProcessTimerEvent _processTimerEvent;
	map<uint64_t, map<uint32_t, TimerEvent *> > _timers;
	uint64_t _lastTime;
	uint64_t _currentTime;
	bool _processResult;
	bool _processing;
public:
	TimersManager(ProcessTimerEvent processTimerEvent);
	virtual ~TimersManager();
	void AddTimer(TimerEvent &timerEvent);
	void RemoveTimer(uint32_t eventTimerId);
	int32_t TimeElapsed();
private:
	string DumpTimers();
};

#endif	/* _TIMERSMANAGER_H */


