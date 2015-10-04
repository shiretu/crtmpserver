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

#include "common.h"

bool deleteFile(const string &path) {
	return remove(path.c_str()) == 0;
}

void splitFileName(const string &fileName, string &name, string &extension, char separator) {
	size_t dotPosition = fileName.find_last_of(separator);
	if (dotPosition == string::npos) {
		name = fileName;
		extension = "";
		return;
	}
	name = fileName.substr(0, dotPosition);
	extension = fileName.substr(dotPosition + 1);
}

bool moveFile(const string &src, const string &dst) {
	if (rename(src.c_str(), dst.c_str()) != 0) {
		FATAL("Unable to move file from `%s` to `%s`",
				src.c_str(), dst.c_str());
		return false;
	}
	return true;
}

bool setFdJoinMulticast(SOCKET_TYPE sock, string bindIp, uint16_t bindPort, const string &ssmIp) {
#ifdef OSX
	if (ssmIp != "") {
		WARN("Mac OS X doesn't have SSM support. SSM on %s will not be applied", ssmIp.c_str());
		return setFdJoinMulticast(sock, bindIp, bindPort, "");
	}
#endif /* OSX */
	if (ssmIp == "") {
		struct ip_mreq group;
		group.imr_multiaddr.s_addr = inet_addr(bindIp.c_str());
		group.imr_interface.s_addr = INADDR_ANY;
		if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
				(char *) &group, sizeof (group)) < 0) {
			int err = SOCKET_LAST_ERROR;
#ifdef WIN32
			FATAL("Adding multicast failed. Error was: %d", err);
#else /* WIN32 */
			FATAL("Adding multicast failed. Error was: (%d) %s", err, strerror(err));
#endif /* WIN32 */
			return false;
		}
		return true;
	} else {
		struct group_source_req multicast;
		struct sockaddr_in *pGroup = (struct sockaddr_in*) &multicast.gsr_group;
		struct sockaddr_in *pSource = (struct sockaddr_in*) &multicast.gsr_source;

		memset(&multicast, 0, sizeof (multicast));

		//Setup the group we want to join
		pGroup->sin_family = AF_INET;
		pGroup->sin_addr.s_addr = inet_addr(bindIp.c_str());
		pGroup->sin_port = EHTONS(bindPort);

		//setup the source we want to listen
		pSource->sin_family = AF_INET;
		pSource->sin_addr.s_addr = inet_addr(bindIp.c_str());
		if (pSource->sin_addr.s_addr == INADDR_NONE) {
			FATAL("Unable to SSM on address %s", bindIp.c_str());
			return false;
		}
		pSource->sin_port = 0;

		INFO("Try to SSM on ip %s", bindIp.c_str());

		if (setsockopt(sock, IPPROTO_IP, MCAST_JOIN_SOURCE_GROUP, (char *) &multicast,
				sizeof (multicast)) < 0) {
			int err = SOCKET_LAST_ERROR;
#ifdef WIN32
			FATAL("Adding multicast failed. Error was: %d", err);
#else /* WIN32 */
			FATAL("Adding multicast failed. Error was: (%d) %s", err, strerror(err));
#endif /* WIN32 */
			return false;
		}

		return true;
	}
}

bool setFdKeepAlive(SOCKET_TYPE fd, bool isUdp) {
	if (isUdp)
		return true;
	int one = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (const char *) &one, sizeof (one)) != 0) {
		int err = SOCKET_LAST_ERROR;
#ifdef WIN32
		FATAL("setsockopt with SOL_SOCKET/SO_KEEPALIVE failed. Error was: %d", err);
#else /* WIN32 */
		FATAL("setsockopt with SOL_SOCKET/SO_KEEPALIVE failed. Error was: (%d) %s", err, strerror(err));
#endif /* WIN32 */
		return false;
	}
	return true;
}

bool setFdNoNagle(SOCKET_TYPE fd, bool isUdp) {
	if (isUdp)
		return true;
	int one = 1;
	if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char *) &one, sizeof (one)) != 0) {
		int err = SOCKET_LAST_ERROR;
#ifdef WIN32
		FATAL("setsockopt with IPPROTO_TCP/TCP_NODELAY failed. Error was: %d", err);
#else /* WIN32 */
		FATAL("setsockopt with IPPROTO_TCP/TCP_NODELAY failed. Error was: (%d) %s", err, strerror(err));
#endif /* WIN32 */
		return false;
	}
	return true;
}

bool setFdReuseAddress(SOCKET_TYPE fd) {
	int one = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *) &one, sizeof (one)) != 0) {
		int err = SOCKET_LAST_ERROR;
#ifdef WIN32
		FATAL("setsockopt with SOL_SOCKET/SO_REUSEADDR failed. Error was: %d", err);
#else /* WIN32 */
		FATAL("setsockopt with SOL_SOCKET/SO_REUSEADDR failed. Error was: (%d) %s", err, strerror(err));
#endif /* WIN32 */
		return false;
	}
#ifdef SO_REUSEPORT
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, (const char *) &one, sizeof (one)) != 0) {
		int err = SOCKET_LAST_ERROR;
#ifdef WIN32
		FATAL("setsockopt with SOL_SOCKET/SO_REUSEPORT failed. Error was: %d", err);
#else /* WIN32 */
		FATAL("setsockopt with SOL_SOCKET/SO_REUSEPORT failed. Error was: (%d) %s", err, strerror(err));
#endif /* WIN32 */
		return false;
	}
#endif /* SO_REUSEPORT */
	return true;
}

bool setFdTTL(SOCKET_TYPE fd, uint8_t ttl) {
	int temp = ttl;
	if (setsockopt(fd, IPPROTO_IP, IP_TTL, (const char *) &temp, sizeof (temp)) != 0) {
		int err = SOCKET_LAST_ERROR;
#ifdef WIN32
		WARN("setsockopt with IPPROTO_IP/IP_TTL failed. Error was: %d", err);
#else /* WIN32 */
		WARN("setsockopt with IPPROTO_IP/IP_TTL failed. Error was: (%d) %s", err, strerror(err));
#endif /* WIN32 */
	}
	return true;
}

bool setFdMulticastTTL(SOCKET_TYPE fd, uint8_t ttl) {
	int temp = ttl;
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, (const char *) &temp, sizeof (temp)) != 0) {
		int err = SOCKET_LAST_ERROR;
#ifdef WIN32
		WARN("setsockopt with IPPROTO_IP/IP_MULTICAST_TTL failed. Error was: %d", err);
#else /* WIN32 */
		WARN("setsockopt with IPPROTO_IP/IP_MULTICAST_TTL failed. Error was: (%d) %s", err, strerror(err));
#endif /* WIN32 */
	}
	return true;
}

bool setFdTOS(SOCKET_TYPE fd, uint8_t tos) {
	int temp = tos;
	if (setsockopt(fd, IPPROTO_IP, IP_TOS, (const char *) &temp, sizeof (temp)) != 0) {
		int err = SOCKET_LAST_ERROR;
#ifdef WIN32
		WARN("setsockopt with IPPROTO_IP/IP_TOS failed. Error was: %d", err);
#else /* WIN32 */
		WARN("setsockopt with IPPROTO_IP/IP_TOS failed. Error was: (%d) %s", err, strerror(err));
#endif /* WIN32 */
	}
	return true;
}

bool setFdLinger(SOCKET_TYPE fd) {
	struct linger temp;
	temp.l_onoff = 1;
	temp.l_linger = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_LINGER, (const char*) &temp, sizeof (temp)) != 0) {
		int err = SOCKET_LAST_ERROR;
#ifdef WIN32
		WARN("setsockopt with SOL_SOCKET/SO_LINGER failed. Error was: %d", err);
#else /* WIN32 */
		WARN("setsockopt with SOL_SOCKET/SO_LINGER failed. Error was: (%d) %s", err, strerror(err));
#endif /* WIN32 */
	}
	return true;
}

static bool ComputeValues(SOCKET_TYPE sock, int option, bool isUdp) {
	if ((option == SO_SNDBUF) && isUdp)
		return true;
	int testing = MAX_SOCKET_SND_RCV_KERNEL_BUFFER;
	int lastGood = 0;
	int lastFailed = MAX_SOCKET_SND_RCV_KERNEL_BUFFER;
	while (lastGood != testing) {
		//		printf("B: %s %s: lastGood: %10d; testing: %10d; lastFailed: %d ",
		//				isUdp ? "UDP" : "TCP",
		//				(option == SO_SNDBUF) ? "SND" : "RCV",
		//				lastGood, testing, lastFailed);
		if (setsockopt(sock, SOL_SOCKET, option, (const char*) &testing, sizeof (testing)) != 0) {
			int err = SOCKET_LAST_ERROR;
			if (err != SOCKET_ERROR_ENOBUFS) {
#ifdef WIN32
				FATAL("socket() failed. Error was: %d", err);
#else /* WIN32 */
				FATAL("socket() failed. Error was: (%d) %s", err, strerror(err));
#endif /* WIN32 */
				return false;
			}
			//			printf("FAILED %d %s\n", err, strerror(err));
			lastFailed = testing;
			testing = lastGood + (testing - lastGood) / 2;
		} else {
			//			printf("SUCCESS\n");
			lastGood = testing;
			testing = lastGood + (lastFailed - lastGood) / 2;
		}
		//		printf("A: %s %s: lastGood: %10d; testing: %10d; lastFailed: %d\n",
		//				isUdp ? "UDP" : "TCP",
		//				(option == SO_SNDBUF) ? "SND" : "RCV",
		//				lastGood, testing, lastFailed);
	}
	return lastGood > 0;
}

bool setFdMaxSndRcvBuff(SOCKET_TYPE fd, bool isUdp) {
	return ComputeValues(fd, SO_SNDBUF, isUdp)
			&& ComputeValues(fd, SO_RCVBUF, isUdp);
}

bool setFdOptions(SOCKET_TYPE fd, bool isUdp) {
	setFdNoNagle(fd, isUdp);
	if (!isUdp)
		setFdLinger(fd);

	if ((!setFdNonBlock(fd))&&(!isUdp))
		return false;

	if ((!setFdNoSIGPIPE(fd))
			|| (!setFdKeepAlive(fd, isUdp))
			|| (!setFdReuseAddress(fd))
			|| (!setFdMaxSndRcvBuff(fd, isUdp))
			)
		return false;
	return true;
}

static time_t _gUTCOffset = -1;

void computeUTCOffset() {
	time_t now = time(NULL);
	struct tm timeLocal;
	struct tm timeUTC;
	localtime_r(&now, &timeLocal);
	gmtime_r(&now, &timeUTC);

	timeLocal.tm_isdst = 0;
	timeUTC.tm_isdst = 0;

	char *tz;

	tz = getenv("TZ");
	setenv("TZ", "", 1);
	TzSet();
	_gUTCOffset = mktime(&timeLocal) - mktime(&timeUTC);
	if (tz)
		setenv("TZ", tz, 1);
	else
		unsetenv("TZ");
	TzSet();
}

time_t gettimeoffset() {
	if (_gUTCOffset == -1)
		computeUTCOffset();
	return _gUTCOffset;
}

time_t getlocaltime() {
	if (_gUTCOffset == -1)
		computeUTCOffset();
	return getutctime() + _gUTCOffset;
}

string getHostByName(const string &name) {
	struct hostent *pHostEnt = gethostbyname(name.c_str());
	if (pHostEnt == NULL)
		return "";
	if (pHostEnt->h_length <= 0)
		return "";
	return format("%"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8,
			(uint8_t) pHostEnt->h_addr_list[0][0],
			(uint8_t) pHostEnt->h_addr_list[0][1],
			(uint8_t) pHostEnt->h_addr_list[0][2],
			(uint8_t) pHostEnt->h_addr_list[0][3]);
}

uint64_t GetTimeMillis() {
#ifdef HAS_clock_gettime
#if !defined __CLOCK_TYPE && defined CLOCK_MONOTONIC_COARSE
#define __CLOCK_TYPE CLOCK_MONOTONIC_COARSE
#endif /* !defined __CLOCK_TYPE && defined CLOCK_MONOTONIC_COARSE */
#if !defined __CLOCK_TYPE && defined CLOCK_MONOTONIC_RAW
#define __CLOCK_TYPE CLOCK_MONOTONIC_RAW
#endif /* !defined __CLOCK_TYPE && defined CLOCK_MONOTONIC_RAW */
#if !defined __CLOCK_TYPE && defined CLOCK_MONOTONIC
#define __CLOCK_TYPE CLOCK_MONOTONIC
#endif /* !defined __CLOCK_TYPE && defined CLOCK_MONOTONIC */
#ifndef __CLOCK_TYPE
#error "Unable to determine clock type"
#endif /* __CLOCK_TYPE */
	struct timespec ts;
	clock_gettime(__CLOCK_TYPE, &ts);
	return (uint64_t) ts.tv_sec * 1000 + (uint64_t) ts.tv_nsec / 1000000;
#else /* HAS_clock_gettime */
	struct timeval ts;
	gettimeofday(&ts, NULL);
	return (uint64_t) ts.tv_sec * 1000 + (uint64_t) ts.tv_usec / 1000;
#endif /* clock_gettime */
}

