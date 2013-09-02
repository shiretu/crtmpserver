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

#ifndef _PASSTHROUGHPROTOCOL_H
#define	_PASSTHROUGHPROTOCOL_H

#include "protocols/baseprotocol.h"
#include "streaming/basestream.h"

class InNetPassThroughStream;
class OutNetPassThroughStream;

class DLLEXP PassThroughProtocol
: public BaseProtocol {
private:
	IOBuffer _inputBuffer;
	IOBuffer _outputBuffer;
	BaseStream *_pDummyStream;
public:
	PassThroughProtocol();
	virtual ~PassThroughProtocol();

	virtual IOBuffer * GetOutputBuffer();
	virtual void GetStats(Variant &info, uint32_t namespaceId);
	virtual bool Initialize(Variant &parameters);
	void SetDummyStream(BaseStream *pDummyStream);
	virtual bool AllowFarProtocol(uint64_t type);
	virtual bool AllowNearProtocol(uint64_t type);
	virtual bool SignalInputData(int32_t recvAmount);
	virtual bool SignalInputData(IOBuffer &buffer);
	virtual bool SignalInputData(IOBuffer &buffer, sockaddr_in *pPeerAddress);
	bool SendTCPData(string &data);
private:
	void CloseStream();
};
#endif	/* _PASSTHROUGHPROTOCOL_H */
