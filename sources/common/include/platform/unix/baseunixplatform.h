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

#if defined FREEBSD || defined LINUX || defined OSX || defined SOLARIS

#include "platform/baseplatform.h"

#include <arpa/inet.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <glob.h>
#include <ifaddrs.h>
#include <inttypes.h>
#include <limits.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/uio.h>
#include <pthread.h>

#ifndef PRIz
#define PRIz "z"
#endif /* PRIz */

#define DLLEXP
#define EMS_RESTRICT __restrict__
#define HAS_MMAP 1
#define COLOR_TYPE const char *
#define FATAL_COLOR "\033[01;31m"
#define ERROR_COLOR "\033[22;31m"
#define WARNING_COLOR "\033[01;33m"
#define INFO_COLOR "\033[22;36m"
#define DEBUG_COLOR "\033[01;37m"
#define FINE_COLOR "\033[22;37m"
#define FINEST_COLOR "\033[22;37m"
#define NORMAL_COLOR "\033[0m"
#define SET_CONSOLE_TEXT_COLOR(color) fprintf(stdout,"%s",color)
#define LIB_HANDLER void *
#define FREE_LIBRARY(libHandler) dlclose((libHandler))
#define LOAD_LIBRARY(file,flags) dlopen((file), (flags))
#define LOAD_LIBRARY_FLAGS RTLD_NOW | RTLD_LOCAL
#define OPEN_LIBRARY_ERROR STR(string(dlerror()))
#define GET_PROC_ADDRESS(libHandler, procName) dlsym((libHandler), (procName))
#define PATH_SEPARATOR '/'
#define PIOFFT off_t
#define GetPid getpid
#define PutEnv putenv
#define TzSet tzset
#define Chmod chmod
#define ftell64 ftello
#define fseek64 fseeko
#define InitNetworking()
#define READ_FD read
#define WRITE_FD write
#define MSGHDR struct msghdr
#define IOVEC iovec
#define MSGHDR_MSG_IOV msg_iov
#define MSGHDR_MSG_IOVLEN msg_iovlen
#define MSGHDR_MSG_NAME msg_name
#define MSGHDR_MSG_NAMELEN msg_namelen
#define IOVEC_IOV_BASE iov_base
#define IOVEC_IOV_LEN iov_len
#define IOVEC_IOV_BASE_TYPE uint8_t
#define SENDMSG(s,msg,flags,sent) sendmsg(s,msg,flags)

//used to abstract overlapped disk I/O for windows

struct overlapped_wrapper_t {
	int _fd;
};
#define writev_ex(ow, iov, iovcnt) writev(ow._fd,iov,iovcnt)
#define fsync_ex(ow) fsync(ow._fd)

//socket abstractions
#define SOCKET_TYPE int
#define SOCKET_INVALID (-1)
#define SOCKET_IS_INVALID(sock) ((sock)<0)
#define SOCKET_IS_VALID(sock) ((sock)>=0)
#define SOCKET_CLOSE(fd) do{ if(SOCKET_IS_VALID(fd)) { shutdown(fd, SHUT_WR); close(fd); } fd=SOCKET_INVALID; } while(0)
#define SOCKET_LAST_ERROR			errno
#define SOCKET_ERROR_EINPROGRESS	EINPROGRESS
#define SOCKET_ERROR_EAGAIN			EAGAIN
#define SOCKET_ERROR_EWOULDBLOCK	EWOULDBLOCK
#define SOCKET_ERROR_ECONNRESET		ECONNRESET
#define SOCKET_ERROR_ENOBUFS		ENOBUFS

//threading: mutex
#define THREADING_PTHREAD
#define MUTEX_TYPE pthread_mutex_t
#define MUTEX_INIT pthread_mutex_init
#define MUTEX_STATIC_INIT PTHREAD_MUTEX_INITIALIZER
#define MUTEX_DESTROY pthread_mutex_destroy
#define MUTEX_LOCK pthread_mutex_lock
#define MUTEX_UNLOCK pthread_mutex_unlock

//threading: threads
#define THREAD_TYPE pthread_t
#define THREAD_CREATE pthread_create
#define THREAD_JOIN pthread_join
#define THREAD_EXIT pthread_exit

#endif /* defined FREEBSD || defined LINUX || defined OSX || defined SOLARIS */

