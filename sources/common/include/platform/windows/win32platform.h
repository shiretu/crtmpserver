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

#ifdef WIN32

#include "platform/baseplatform.h"
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <io.h>
#include <Shlwapi.h>
#include <process.h>
#include <IPHlpApi.h>

//define missing PRI_64 specifiers
#ifndef PRId64
#define PRId64 "I64d"
#endif /* PRId64 */

#ifndef PRIu64
#define PRIu64 "I64u"
#endif /* PRIu64 */

#ifndef PRIx64
#define PRIx64 "I64x"
#endif /* PRIx64 */

#ifndef PRIX64
#define PRIX64 "I64X"
#endif /* PRIX64 */

#ifndef PRId32
#define PRId32 "I32d"
#endif /* PRId32 */

#ifndef PRIu32
#define PRIu32 "I32u"
#endif /* PRIu32 */

#ifndef PRIx32
#define PRIx32 "I32x"
#endif /* PRIx32 */

#ifndef PRIX32
#define PRIX32 "I32X"
#endif /* PRIX32 */

#ifndef PRId16
#define PRId16 "hd"
#endif /* PRId16 */

#ifndef PRIu16
#define PRIu16 "hu"
#endif /* PRIu16 */

#ifndef PRIx16
#define PRIx16 "hx"
#endif /* PRIx16 */

#ifndef PRIX16
#define PRIX16 "hX"
#endif /* PRIX16 */

#ifndef PRId8
#define PRId8 "d"
#endif /* PRId8 */

#ifndef PRIu8
#define PRIu8 "u"
#endif /* PRIu8 */

#ifndef PRIx8
#define PRIx8 "x"
#endif /* PRIx8 */

#ifndef PRIX8
#define PRIX8 "X"
#endif /* PRIX8 */

#ifndef PRIz
#define PRIz "I"
#endif /* PRIz */

#ifndef ssize_t
#define ssize_t   __int64
#endif

#define IOV_MAX 512

struct msghdr {
	const void *msg_name; /* optional address */
	socklen_t msg_namelen; /* size of address */
	struct iovec *msg_iov; /* scatter/gather array */
	size_t msg_iovlen; /* # elements in msg_iov */
	void *msg_control; /* ancillary data, see below */
	size_t msg_controllen; /* ancillary data buffer len */
	int msg_flags; /* flags on received message */
};

struct if_data {
	/* generic interface information */
	u_char ifi_type; /* ethernet, tokenring, etc */
	u_char ifi_addrlen; /* media address length */
	u_char ifi_hdrlen; /* media header length */
	u_long ifi_mtu; /* maximum transmission unit */
	u_long ifi_metric; /* routing metric (external only) */
	u_long ifi_baudrate; /* linespeed */
	/* volatile statistics */
	u_long ifi_ipackets; /* packets received on interface */
	u_long ifi_ierrors; /* input errors on interface */
	u_long ifi_opackets; /* packets sent on interface */
	u_long ifi_oerrors; /* output errors on interface */
	u_long ifi_collisions; /* collisions on csma interfaces */
	u_long ifi_ibytes; /* total number of octets received */
	u_long ifi_obytes; /* total number of octets sent */
	u_long ifi_imcasts; /* packets received via multicast */
	u_long ifi_omcasts; /* packets sent via multicast */
	u_long ifi_iqdrops; /* dropped on input, this interface */
	u_long ifi_noproto; /* destined for unsupported protocol */
	struct timeval ifi_lastchange; /* last updated */
};

struct ifaddrs {
	IP_ADAPTER_ADDRESSES *pRawItem;
	IP_ADAPTER_ADDRESSES *pRawHead;
	if_data _data;
	struct ifaddrs *ifa_next; // Next item in list
#define ifa_name pRawItem->AdapterName
	//const char *ifa_name; // Name of interface
	unsigned int ifa_flags; // Flags from SIOCGIFFLAGS
	struct sockaddr *ifa_addr; // Address of interface
	struct sockaddr *ifa_netmask; // Netmask of interface

	union {
		struct sockaddr *ifu_broadaddr;
		/* Broadcast address of interface */
		struct sockaddr *ifu_dstaddr;
		/* Point-to-point destination address */
	} ifa_ifu;
#define ifa_broadaddr ifa_ifu.ifu_broadaddr
#define ifa_dstaddr   ifa_ifu.ifu_dstaddr
	void *ifa_data; /* Address-specific data */

	ifaddrs() {
		pRawItem = NULL;
		pRawHead = NULL;
		memset(&_data, 0, sizeof (_data));
		ifa_next = NULL;
		ifa_flags = 0;
		ifa_addr = NULL;
		ifa_netmask = NULL;
		ifa_ifu.ifu_broadaddr = NULL;
		ifa_ifu.ifu_dstaddr = NULL;
		ifa_data = &_data;
	}

	virtual ~ifaddrs() {
		if (ifa_netmask != NULL) {
			delete ifa_netmask;
			ifa_netmask = NULL;
		}
	}
};
#define        IFF_POINTOPOINT IFF_POINTTOPOINT
#define        IFF_RUNNING     0x40

#define atoll atol

#define DLLEXP
#define EMS_RESTRICT

#define __func__ __FUNCTION__
#define COLOR_TYPE WORD
#define FATAL_COLOR FOREGROUND_INTENSITY | FOREGROUND_RED
#define ERROR_COLOR FOREGROUND_RED
#define WARNING_COLOR 6
#define INFO_COLOR FOREGROUND_GREEN | FOREGROUND_BLUE
#define DEBUG_COLOR 7
#define FINE_COLOR 8
#define FINEST_COLOR 8
#define NORMAL_COLOR 8
#define SET_CONSOLE_TEXT_COLOR(color) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color)

#define MSG_NOSIGNAL 0
#define READ_FD _read
#define WRITE_FD _write

#define PATH_SEPARATOR '\\'
#define LIBRARY_NAME_PATTERN "%s.dll"
#define LIB_HANDLER HMODULE
#define LOAD_LIBRARY(file,flags) UnicodeLoadLibrary((file))
#define OPEN_LIBRARY_ERROR "OPEN_LIBRARY_ERROR NOT IMPLEMENTED YET"
#define GET_PROC_ADDRESS(libHandler, procName) GetProcAddress((libHandler), (procName))
#define FREE_LIBRARY(libHandler) FreeLibrary((libHandler))
#define S_IRUSR S_IREAD
#define S_IWUSR S_IWRITE
#define S_IRGRP 0000
#define S_IWGRP 0000
#define snprintf sprintf_s
#define pid_t DWORD
#define PIOFFT __int64
#define GetPid _getpid
#define PutEnv _putenv
#define TzSet _tzset
#define Chmod _chmod

#define gmtime_r(_p_time_t, _p_struct_tm) *(_p_struct_tm) = *gmtime(_p_time_t);

#define FD_COPY(f, t)   (void)(*(t) = *(f))

struct iovec {
	void * iov_base;
	size_t iov_len;
};

#define MSGHDR WSAMSG
#define IOVEC WSABUF
#define MSGHDR_MSG_IOV lpBuffers
#define MSGHDR_MSG_IOVLEN dwBufferCount
#define MSGHDR_MSG_IOVLEN_TYPE DWORD
#define MSGHDR_MSG_NAME name
#define MSGHDR_MSG_NAMELEN namelen
#define IOVEC_IOV_BASE buf
#define IOVEC_IOV_LEN len
#define IOVEC_IOV_BASE_TYPE CHAR
#define SENDMSG(s,msg,flags,sent) WSASendMsg(s,msg,flags,(LPDWORD)(sent),NULL,NULL)
#define sleep(x) Sleep((x)*1000)

#define ftell64 _ftelli64
#define fseek64 _fseeki64
#define lseek64 _lseeki64

#define localtime_r(time_t_val, struct_tm_val) localtime_s(struct_tm_val,time_t_val)
#define setenv(n,v,d) _putenv_s(n,v)
#define unsetenv(n) _putenv_s(n,"")
DLLEXP time_t timegm(struct tm *tm);
DLLEXP char *strptime(const char *buf, const char *fmt, struct tm *timeptr);
DLLEXP int strcasecmp(const char *s1, const char *s2);
DLLEXP int strncasecmp(const char *s1, const char *s2, size_t n);
DLLEXP int vasprintf(char **strp, const char *fmt, va_list ap, int size = 256);
DLLEXP int gettimeofday(struct timeval *tv, void* tz);
DLLEXP void InitNetworking();
DLLEXP HMODULE UnicodeLoadLibrary(string fileName);
DLLEXP int inet_aton(const char *pStr, struct in_addr *pRes);
DLLEXP void *memmem(const void *l, size_t l_len, const void *s, size_t s_len);
#define strtoull _strtoui64
DLLEXP int getifaddrs(struct ifaddrs **ppIfap);
DLLEXP void freeifaddrs(struct ifaddrs *pIf);
DLLEXP ssize_t sendmsg(SOCKET socket, const struct msghdr *message, int flags);

struct overlapped_wrapper_t {
	OVERLAPPED *_pOverlapped;
	HANDLE *_pEvents;
	size_t _count;
	int _fd;
	HANDLE _fdHandle;
	overlapped_wrapper_t();
	virtual ~overlapped_wrapper_t();
	bool Init(size_t count);
	void Free();
};
DLLEXP ssize_t writev_ex(overlapped_wrapper_t &ow, const struct iovec *iov, int iovcnt);
DLLEXP int fsync_ex(overlapped_wrapper_t &ow);
DLLEXP void usleep(uint64_t microSeconds);

//socket abstractions
#define SOCKET_TYPE SOCKET
#define SOCKET_INVALID INVALID_SOCKET
#define SOCKET_IS_INVALID(sock) ((sock)==INVALID_SOCKET)
#define SOCKET_IS_VALID(sock) ((sock)!=INVALID_SOCKET)
#define SOCKET_CLOSE(fd) do{ if(SOCKET_IS_VALID(fd)) { shutdown(fd, SD_SEND); closesocket(fd); } fd=SOCKET_INVALID; } while(0)
#define SOCKET_LAST_ERROR			WSAGetLastError()
#define SOCKET_ERROR_EINPROGRESS			WSAEINPROGRESS
#define SOCKET_ERROR_EAGAIN				WSAEWOULDBLOCK
#define SOCKET_ERROR_EWOULDBLOCK			WSAEWOULDBLOCK
#define SOCKET_ERROR_ECONNRESET			WSAECONNRESET
#define SOCKET_ERROR_ENOBUFS				WSAENOBUFS

//threading: mutex
#define THREADING_WINTHREAD
#define MUTEX_TYPE CSW
#define MUTEX_INIT(p1,p2) 0
#define MUTEX_STATIC_INIT CSW()
#define MUTEX_DESTROY(p1)
#define MUTEX_LOCK(pCSW) (pCSW)->EnterCriticalSection()
#define MUTEX_UNLOCK(pCSW) (pCSW)->LeaveCriticalSection()

//threading: threads
#define THREAD_TYPE HANDLE
DLLEXP int THREAD_CREATE(THREAD_TYPE *pThread, void *pReserved, void *(*start_routine)(void *), void *pArguments);
DLLEXP int THREAD_JOIN(THREAD_TYPE thread, void **ppReserved);
DLLEXP void THREAD_EXIT(void *pReserved);

#endif /* WIN32 */
