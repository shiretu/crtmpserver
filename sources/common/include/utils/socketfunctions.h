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

bool setFdJoinMulticast(SOCKET_TYPE sock, string bindIp, uint16_t bindPort, const string &ssmIp);
bool setFdCloseOnExec(SOCKET_TYPE fd);
bool setFdNonBlock(SOCKET_TYPE fd);
bool setFdNoSIGPIPE(SOCKET_TYPE fd);
bool setFdKeepAlive(SOCKET_TYPE fd, bool isUdp);
bool setFdNoNagle(SOCKET_TYPE fd, bool isUdp);
bool setFdReuseAddress(SOCKET_TYPE fd);
bool setFdTTL(SOCKET_TYPE fd, uint8_t ttl);
bool setFdMulticastTTL(SOCKET_TYPE fd, uint8_t ttl);
bool setFdTOS(SOCKET_TYPE fd, uint8_t tos);
bool setFdLinger(SOCKET_TYPE fd);
bool setFdMaxSndRcvBuff(SOCKET_TYPE fd, bool isUdp);
bool setFdOptions(SOCKET_TYPE fd, bool isUdp);
