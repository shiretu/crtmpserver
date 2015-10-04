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

class IOHandler;

typedef enum iocp_event_type {
	iocp_event_type_timer = 0,
	iocp_event_type_read,
	iocp_event_type_write,
	iocp_event_type_accept,
	iocp_event_type_connect,
	iocp_event_type_udp
} iocp_event_type;

//extern int ____allocs;

typedef struct iocp_event : public OVERLAPPED {
	iocp_event_type type;
	bool isValid;
	bool pendingOperation;
	IOHandler *pIOHandler;
	DWORD transferedBytes;

	iocp_event(iocp_event_type _type, IOHandler * _pIOHandler) {
		OVERLAPPED *pTemp = (OVERLAPPED *)this;
		memset(pTemp, 0, sizeof (OVERLAPPED));
		type = _type;
		isValid = true;
		pendingOperation = false;
		pIOHandler = _pIOHandler;
		transferedBytes = 0;
		//____allocs++;
		//FINEST("C: %p %d %d", this, type, ____allocs);
	}

	virtual ~iocp_event() {
		//____allocs--;
		//FINEST("D: %p %d %d", this, type, ____allocs);
	}

	virtual void Release() {
		isValid = false;
		pIOHandler = NULL;
		transferedBytes = 0;
		if (!pendingOperation) {
			delete this;
		}
	}
} iocp_event;

typedef struct iocp_event_accept : public iocp_event {
	SOCKET_TYPE listenSock;
	SOCKET_TYPE acceptedSock;
	uint8_t pBuffer[2 * sizeof (sockaddr_in) + 32];

	iocp_event_accept(IOHandler * _pIOHandler) : iocp_event(iocp_event_type_accept, _pIOHandler) {
		listenSock = SOCKET_INVALID;
		acceptedSock = SOCKET_INVALID;
		memset(pBuffer, 0, sizeof (pBuffer));
	}

	static iocp_event_accept * GetInstance(IOHandler * _pIOHandler) {
		iocp_event_accept *pResult = new iocp_event_accept(_pIOHandler);
		return pResult;
	}

	virtual void Release() {
		SOCKET_CLOSE(listenSock);
		SOCKET_CLOSE(acceptedSock);
		iocp_event::Release();
	}
} iocp_event_accept;

#define MAX_IOCP_EVT_READ_SIZE 16*1024

typedef struct iocp_event_tcp_read : public iocp_event {
	WSABUF inputBuffer;
	DWORD flags;

	iocp_event_tcp_read(IOHandler * _pIOHandler) : iocp_event(iocp_event_type_read, _pIOHandler) {
		inputBuffer.buf = new CHAR[MAX_IOCP_EVT_READ_SIZE];
		inputBuffer.len = MAX_IOCP_EVT_READ_SIZE;
		memset(inputBuffer.buf, 0, MAX_IOCP_EVT_READ_SIZE);
		flags = 0;
	}

	static iocp_event_tcp_read * GetInstance(IOHandler * _pIOHandler) {
		iocp_event_tcp_read *pResult = new iocp_event_tcp_read(_pIOHandler);
		return pResult;
	}

	virtual void Release() {
		if (inputBuffer.buf != NULL) {
			delete[] inputBuffer.buf;
		}
		inputBuffer.buf = NULL;
		inputBuffer.len = 0;
		iocp_event::Release();
	}
} iocp_event_tcp_read;

typedef struct iocp_event_tcp_write : public iocp_event {
	IOBuffer outputBuffer;
	WSABUF wsaBuf;

	iocp_event_tcp_write(IOHandler * _pIOHandler) : iocp_event(iocp_event_type_write, _pIOHandler) {
	}

	static iocp_event_tcp_write * GetInstance(IOHandler * _pIOHandler) {
		iocp_event_tcp_write *pResult = new iocp_event_tcp_write(_pIOHandler);
		return pResult;
	}

	virtual void Release() {
		outputBuffer.IgnoreAll();
		iocp_event::Release();
	}
} iocp_event_tcp_write;

typedef struct iocp_event_udp_read : public iocp_event {
	WSABUF inputBuffer;
	DWORD flags;
	sockaddr_in sourceAddress;
	socklen_t sourceLen;

	iocp_event_udp_read(IOHandler * _pIOHandler) : iocp_event(iocp_event_type_udp, _pIOHandler) {
		inputBuffer.buf = new CHAR[MAX_IOCP_EVT_READ_SIZE];
		inputBuffer.len = MAX_IOCP_EVT_READ_SIZE;
		memset(inputBuffer.buf, 0, MAX_IOCP_EVT_READ_SIZE);
		memset(&sourceAddress, 0, sizeof (sockaddr_in));
		sourceLen = sizeof (sockaddr_in);
		flags = 0;
	}

	static iocp_event_udp_read * GetInstance(IOHandler * _pIOHandler) {
		iocp_event_udp_read *pResult = new iocp_event_udp_read(_pIOHandler);
		return pResult;
	}

	virtual void Release() {
		if (inputBuffer.buf != NULL) {
			delete[] inputBuffer.buf;
		}
		inputBuffer.buf = NULL;
		inputBuffer.len = 0;
		iocp_event::Release();
	}
} iocp_event_udp_read;

typedef struct iocp_event_connect : public iocp_event {
	uint8_t pBuffer[2 * sizeof (sockaddr_in) + 32];
	SOCKET_TYPE connectedSock;
	bool closeSocket;

	iocp_event_connect(IOHandler * _pIOHandler) : iocp_event(iocp_event_type_connect, _pIOHandler) {
		connectedSock = SOCKET_INVALID;
		memset(pBuffer, 0, sizeof (pBuffer));
	}

	static iocp_event_connect * GetInstance(IOHandler * _pIOHandler) {
		iocp_event_connect *pResult = new iocp_event_connect(_pIOHandler);
		return pResult;
	}

	virtual void Release() {
		if (closeSocket) {
			SOCKET_CLOSE(connectedSock);
		}
		iocp_event::Release();
	}
} iocp_event_connect;

#ifdef HAS_IOCP_TIMER
typedef struct iocp_event_timer_elapsed : public iocp_event {

	iocp_event_timer_elapsed(IOHandler * _pIOHandler) : iocp_event(iocp_event_type_timer, _pIOHandler) {
//		hEvent = CreateEvent(NULL, false, false, NULL);
	}

	static iocp_event_timer_elapsed * GetInstance(IOHandler * _pIOHandler) {
		return new iocp_event_timer_elapsed(_pIOHandler);
	}
	virtual void Release() {
		iocp_event::Release();
//		CloseHandle(hEvent);
	}
} iocp_event_timer_elapsed;
#endif /* HAS_IOCP_TIMER */

#endif /* NET_IOCP */
