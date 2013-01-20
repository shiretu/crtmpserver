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

#ifndef _UDPSENDERPROTOCOL_H
#define	_UDPSENDERPROTOCOL_H

#include "protocols/udpprotocol.h"

class ReadyForSendInterface;

class UDPSenderProtocol
: public UDPProtocol {
private:
	sockaddr_in _destAddress;
	SOCKET _fd;
	ReadyForSendInterface *_pReadyForSend;
	string _bindIp;
	uint16_t _bindPort;
public:
	UDPSenderProtocol();
	virtual ~UDPSenderProtocol();

	virtual void SetIOHandler(IOHandler *pCarrier);
	virtual void ReadyForSend();

	static UDPSenderProtocol *GetInstance(string bindIp, uint16_t bindPort,
			string targetIp, uint16_t targetPort, uint16_t ttl, uint16_t tos,
			ReadyForSendInterface *pReadyForSend);
	void ResetReadyForSendInterface();

	uint16_t GetUdpBindPort();

	virtual bool SendChunked(uint8_t *pData, uint32_t dataLength,
			uint32_t maxChunkSize);
	virtual bool SendBlock(uint8_t *pData, uint32_t dataLength);
};

#endif	/* _UDPSENDERPROTOCOL_H */
