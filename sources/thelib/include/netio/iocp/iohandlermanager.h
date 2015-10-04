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
#include "netio/fdstats.h"
#include "netio/iocp/iocpevent.h"

class IOHandler;

class DLLEXP IOHandlerManager {
private:
	static FdStats _fdStats;
	static HANDLE _iocpHandle;
	static map<uint32_t, IOHandler *> _activeIOHandlers;
	static map<uint32_t, IOHandler *> _deadIOHandlers;
	static OVERLAPPED_ENTRY *_pDetectedEvents;
	static bool _active;
	static int32_t _nextWaitPeriod;
#ifndef HAS_IOCP_TIMER
	static TimersManager *_pTimersManager;
	static iocp_event _dummyEvent;
#endif /* HAS_IOCP_TIMER */
public:
	static map<uint32_t, IOHandler *> & GetActiveHandlers();
	static map<uint32_t, IOHandler *> & GetDeadHandlers();
	static FdStats &GetStats(bool updateSpeeds);
	static void Initialize();
	static void Start();
	static void SignalShutdown();
	static void ShutdownIOHandlers();
	static void Shutdown();
	static void RegisterIOHandler(IOHandler *pIOHandler);
	static void UnRegisterIOHandler(IOHandler *pIOHandler);
	static SOCKET CreateRawUDPSocket();
	static void CloseRawUDPSocket(SOCKET socket);
#ifdef GLOBALLY_ACCOUNT_BYTES
	static void AddInBytesManaged(IOHandlerType type, uint64_t bytes);
	static void AddOutBytesManaged(IOHandlerType type, uint64_t bytes);
	static void AddInBytesRawUdp(uint64_t bytes);
	static void AddOutBytesRawUdp(uint64_t bytes);
#endif /* GLOBALLY_ACCOUNT_BYTES */
	static bool EnableReadData(IOHandler *pIOHandler);
	static bool DisableReadData(IOHandler *pIOHandler, bool ignoreError = false);
	static bool EnableWriteData(IOHandler *pIOHandler);
	static bool DisableWriteData(IOHandler *pIOHandler, bool ignoreError = false);
	static bool EnableAcceptConnections(IOHandler *pIOHandler);
	static bool DisableAcceptConnections(IOHandler *pIOHandler, bool ignoreError = false);
	static bool EnableTimer(IOHandler *pIOHandler, uint32_t seconds);
	static bool EnableHighGranularityTimer(IOHandler *pIOHandler, uint32_t milliseconds);
	static bool DisableTimer(IOHandler *pIOHandler, bool ignoreError = false);
	static bool Pulse();
	static void EnqueueForDelete(IOHandler *pIOHandler);
	static uint32_t DeleteDeadHandlers();
	static bool RegisterIOCPFD(SOCKET_TYPE fd);
#ifdef HAS_IOCP_TIMER
	static BOOL PostCompletedEvent(iocp_event *pEvent);
#else /* HAS_IOCP_TIMER */
private:
	static bool ProcessTimer(TimerEvent &event);
#endif /* HAS_IOCP_TIMER */
};
#ifdef HAS_IOCP_TIMER
void CALLBACK TimerCallback(LPVOID lpArgToCompletionRoutine,
		DWORD dwTimerLowValue,	DWORD dwTimerHighValue);
#endif /* HAS_IOCP_TIMER */

#endif /* NET_IOCP */
